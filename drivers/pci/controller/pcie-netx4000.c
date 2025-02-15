/*
* PCIe host controller for Hilscher netx4000 based platforms
*
* drivers/pci/host/pcie-netx4000.c
*
* (C) Copyright 2017 Hilscher Gesellschaft fuer Systemautomation mbH
* http://www.hilscher.com
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; version 2 of
* the License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
*/

#define DRIVER_DESC  "PCIe host controller driver for Hilscher netx4000 based platforms"
#define DRIVER_NAME  "pcierc-netx4000"

#include <mach/hardware.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_pci.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include "../pci.h"

/* PCI -> AXI */
#define OFFS_AXI_WINx_BASE(x)		(0x00 | (x<<4)) /* 0x00, 0x10, 0x20, 30 */
#define OFFS_AXI_WINx_MASK(x)		(0x04 | (x<<4)) /* 0x04, 0x14, 0x24, 34 */
#define OFFS_AXI_DESTx(x)		(0x08 | (x<<4)) /* 0x08, 0x18, 0x28, 38 */

/* AXI -> PCI */
#define OFFS_PCI_WINx_BASE(x)		(0x00 | ((4+x)<<4)) /* 0x40, 0x50, 0x60, 0x70 */
#define OFFS_PCI_WINx_MASK(x)		(0x04 | ((4+x)<<4)) /* 0x44, 0x54, 0x64, 0x74 */
#define OFFS_PCI_DESTx_LOW(x)		(0x08 | ((4+x)<<4)) /* 0x48, 0x58, 0x68, 0x78 */
#define OFFS_PCI_DESTx_HI(x)		(0x0c | ((4+x)<<4)) /* 0x4c, 0x5c, 0x6c, 0x7c */

#define OFFS_REQ_DATA3			(0x88)
#define OFFS_REQ_RECV_DATA		(0x8c)
#define OFFS_REQ_ADDR_REG1		(0x90)
#define OFFS_REQ_ADDR_REG2		(0x94)
#define OFFS_REQ_BYTE_EN		(0x98)
#define OFFS_REQ_ISSUE			(0x9c)
#define REQ_ISSUE_TYPE_CFG_RD_0		(4 << 8)
#define REQ_ISSUE_TYPE_CFG_WR_0		(5 << 8)
#define REQ_ISSUE_TYPE_CFG_RD_1		(6 << 8)
#define REQ_ISSUE_TYPE_CFG_WR_1		(7 << 8)
#define REQ_ISSUE_REQUEST		(1 << 0)

#define OFFS_MSI_RX_WIN_ADDR		(0x100)
#define MSI_RX_WIN_ENABLE		(1 << 0)
#define OFFS_MSI_RX_WIN_MASK		(0x108)
#define OFFS_RX_IRQ_ENABLE		(0x110)
#define MSI_RX_IRQ_ENABLE		(1 << 4)
#define INTD_RX_IRQ_ENABLE		(1 << 3)
#define INTC_RX_IRQ_ENABLE		(1 << 2)
#define INTB_RX_IRQ_ENABLE		(1 << 1)
#define INTA_RX_IRQ_ENABLE		(1 << 0)
#define INTX_RX_IRQ_ENABLE		(INTA_RX_IRQ_ENABLE | INTB_RX_IRQ_ENABLE | INTC_RX_IRQ_ENABLE | INTD_RX_IRQ_ENABLE)
#define OFFS_RX_IRQ_STATUS		(0x114)
#define MSI_RX_IRQ_STATUS		(1 << 4)
#define INTD_RX_IRQ_STATUS		(1 << 3)
#define INTC_RX_IRQ_STATUS		(1 << 2)
#define INTB_RX_IRQ_STATUS		(1 << 1)
#define INTA_RX_IRQ_STATUS		(1 << 0)
#define INTX_RX_IRQ_STATUS		(INTA_RX_IRQ_STATUS | INTB_RX_IRQ_STATUS | INTC_RX_IRQ_STATUS | INTD_RX_IRQ_STATUS)
#define OFFS_IRQ_TABLE			(0x140)
#define INTMSI_RC			(1 << 4)
#define INTD_RC				(1 << 3)
#define INTC_RC				(1 << 2)
#define INTB_RC				(1 << 1)
#define INTA_RC				(1 << 0)
#define INTx_RC				(INTA_RC | INTB_RC | INTC_RC | INTD_RC)

#define OFFS_PERMISSION			(0x300)
#define PERMISSION_HWINIT_EN		(1 << 2)
#define PERMISSION_PIPE_PHY		(1 << 1)
#define OFFS_RESET			(0x310)
#define RESET_CFG_B			(1 << 3)
#define RESET_LOAD_B			(1 << 4)
#define OFFS_CORE_MODE_SET_1		(0x400)
#define CORE_MODE_SET_1_MODE_PORT_RC	(1 << 1)
#define CORE_MODE_SET_1_MODE_PORT_DIS	(1 << 0)
#define OFFS_CORE_STATUS_1		(0x408)
#define CORE_STATUS_1_DL_DOWN		(1 << 0)

#define OFFS_MEM_BASE_LIMIT		(0x1020)
#define OFFS_PREF_MEM_BASE_LIMIT	(0x1024)

#define OFFS_LINK_CONTROL_STATUS	(0x1070)
#define LINK_TRAINING			(1 << 27)
#define CURRENT_LINK_SPEED(x)		((x >> 16) & 0xf)
#define RETRAIN_LINK			(1 <<  5)

#define MAX_INTX			(4)
#define MAX_MSI_VECTORS			(32)

struct netx4000_pcie_msi_priv {
	struct irq_domain  *devIrqDomain;
	struct irq_domain  *msiIrqDomain;
	dma_addr_t         dmaHandle;
	void               *dmaHandleVirt;
	u32                num_of_vectors;
	DECLARE_BITMAP(used, MAX_MSI_VECTORS);
	struct mutex       lock; /* protect "used" bitmap */
};

struct netx4000_pcie_priv {
	struct device *dev;
	void __iomem *regs;
	int irq_all;
	struct clk *clk;

	struct list_head pci_res;

	u8 busnr;
	u32 link_gen;

	int gpio_reset;
	enum of_gpio_flags gpio_reset_flags;
	int gpio_clkreq;
	enum of_gpio_flags gpio_clkreq_flags;

	struct netx4000_pcie_msi_priv *msi;

	struct irq_domain *legacyIrqDomain;
};

#define PCIE_RESET_DELAY_US	(1000)
static int netx4000_pcie_reset_deassert(struct netx4000_pcie_priv *priv)
{
	if (!gpio_is_valid(priv->gpio_reset)) {
		return -ENODEV;
	}

	gpio_set_value(priv->gpio_reset, (priv->gpio_reset_flags & OF_GPIO_ACTIVE_LOW) ? 1 : 0);

	usleep_range(PCIE_RESET_DELAY_US, PCIE_RESET_DELAY_US + 500);

	return 0;
}

static int netx4000_pcie_wait_for_clkreq(struct netx4000_pcie_priv *priv, u32 timeout_ms)
{
	unsigned long start_jiffies = jiffies;
	u32 clkreq;

	if (!gpio_is_valid(priv->gpio_clkreq)) {
		return -ENODEV;
	}

	do {
		clkreq = gpio_get_value(priv->gpio_clkreq) ^ ((priv->gpio_clkreq_flags & OF_GPIO_ACTIVE_LOW) ? 1 : 0);
		if (clkreq)
			return 0;
	} while (time_before(jiffies, start_jiffies + msecs_to_jiffies(timeout_ms)));

	dev_err(priv->dev, "Error: clkreq timed out (%dms)!\n", timeout_ms);

	return -ETIMEDOUT;
}

static int netx4000_pcie_wait_for_link(struct netx4000_pcie_priv *priv, u32 timeout_ms)
{
	unsigned long start_jiffies = jiffies;
	u32 link;

	do {
		link = (ioread32(priv->regs + OFFS_CORE_STATUS_1) & CORE_STATUS_1_DL_DOWN) ? 0 : 1;
		if (link) {
			return 0;
		}
	} while (time_before(jiffies, start_jiffies + msecs_to_jiffies(timeout_ms)));

	dev_err(priv->dev, "Error: link up timed out (%dms)!\n", timeout_ms);

	return -ETIMEDOUT;
}

static int netx4000_pcie_retrain_link(struct netx4000_pcie_priv *priv)
{
	unsigned long start_jiffies = jiffies;
	u32 timeout_ms = 500;
	u32 val32;

	val32 = ioread32(priv->regs + OFFS_LINK_CONTROL_STATUS);
	val32 |= RETRAIN_LINK;
	iowrite32(val32, priv->regs + OFFS_LINK_CONTROL_STATUS);

	do {
		val32 = (ioread32(priv->regs + OFFS_LINK_CONTROL_STATUS));
		if (!(val32 & LINK_TRAINING)) {
			return 0;
		}
	} while (time_before(jiffies, start_jiffies + msecs_to_jiffies(timeout_ms)));

	dev_err(priv->dev, "Error: link training timed out (%dms)!\n", timeout_ms);

	return -ETIMEDOUT;
}

static void netx4000_pcie_irq_enable(struct netx4000_pcie_priv *priv, u32 mask)
{
	u32 val32;

	val32 = ioread32(priv->regs + OFFS_RX_IRQ_ENABLE);
	val32 |= mask;
	iowrite32(val32, priv->regs + OFFS_RX_IRQ_ENABLE);
}

static void netx4000_pcie_all_isr(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct netx4000_pcie_priv *priv;
	u32 bit, vIrq, hwIrq, irqStatus;

	chained_irq_enter(chip, desc);
	priv = irq_desc_get_handler_data(desc);

	irqStatus = ioread32(priv->regs + OFFS_IRQ_TABLE);
	if (irqStatus & INTx_RC) {
		dev_dbg(priv->dev, "Receiving INTx IRQ\n");
		iowrite32(INTX_RX_IRQ_STATUS, priv->regs + OFFS_RX_IRQ_STATUS);

		for_each_set_bit(bit, (unsigned long *)&irqStatus, MAX_INTX) {
			vIrq = irq_find_mapping(priv->legacyIrqDomain, bit + 1);
			if (vIrq) {
				generic_handle_irq(vIrq);
			}
		}
	}
	else if (irqStatus & INTMSI_RC) {
		dev_dbg(priv->dev, "Receiving MSI IRQ\n");
		iowrite32(MSI_RX_IRQ_STATUS, priv->regs + OFFS_RX_IRQ_STATUS);

		hwIrq = *(u32*)priv->msi->dmaHandleVirt;
		vIrq = irq_find_mapping(priv->msi->devIrqDomain, hwIrq);

		if (vIrq)
			generic_handle_irq(vIrq);
		else
			dev_err(priv->dev, "Error receiving unexpected MSI IRQ (%d).\n", hwIrq);
	}
	else {
		dev_err(priv->dev, "Error receiving unexpected IRQ (irqStatus 0x%x).\n", irqStatus);
	}

	chained_irq_exit(chip, desc);
}

static int netx4000_pcie_intx_map(struct irq_domain *domain, unsigned int irq, irq_hw_number_t hwirq)
{
	struct netx4000_pcie_priv* priv = domain->host_data;

	irq_set_chip_and_handler(irq, &dummy_irq_chip, handle_simple_irq);
	irq_set_chip_data(irq, domain->host_data);

	dev_dbg(priv->dev, "Mapping hwirq%d to irq%d succeeded.\n", (int)hwirq, irq);

	return 0;
}

static const struct irq_domain_ops intx_domain_ops = {
	.map = netx4000_pcie_intx_map,
};

static int netx4000_pcie_legacy_irq_init(struct netx4000_pcie_priv *priv)
{
	struct device *dev = priv->dev;
	struct device_node *node = dev->of_node;
	struct device_node *pcie_intc_node =  of_get_next_child(node, NULL);

	if (!pcie_intc_node) {
		dev_err(dev, "No PCIe Intc node found\n");
		return -ENODEV;
	}

	priv->legacyIrqDomain = irq_domain_add_linear(pcie_intc_node, MAX_INTX + 1, &intx_domain_ops, priv);
	if (!priv->legacyIrqDomain) {
		dev_err(dev, "Failed to get a INTx IRQ domain\n");
		return -ENODEV;
	}

	netx4000_pcie_irq_enable(priv, INTX_RX_IRQ_ENABLE);

	return 0;
}

static void netx4000_pcie_compose_msi_msg(struct irq_data *data, struct msi_msg *msg)
{
	struct netx4000_pcie_priv* priv = irq_data_get_irq_chip_data(data);
	struct netx4000_pcie_msi_priv *msi = priv->msi;
	phys_addr_t addr = msi->dmaHandle;

	msg->address_lo = lower_32_bits(addr);
	msg->address_hi = upper_32_bits(addr);
	msg->data = data->hwirq;

	dev_dbg(priv->dev, "msi#%d address_hi %#x address_lo %#x\n", (int)data->hwirq, msg->address_hi, msg->address_lo);
}

static int netx4000_pcie_msi_set_affinity(struct irq_data *data, const struct cpumask *mask, bool force)
{
	return -EINVAL;
}

static struct irq_chip netx4000_pcie_msi_bottom_irq_chip = {
	.name                = "PCIe-MSI",
	.irq_compose_msi_msg = netx4000_pcie_compose_msi_msg,
	.irq_set_affinity    = netx4000_pcie_msi_set_affinity,
};

static int netx4000_pcie_irq_domain_alloc(struct irq_domain *domain, unsigned int irq, unsigned int nr_irqs, void *args)
{
	struct netx4000_pcie_priv* priv = domain->host_data;
	struct netx4000_pcie_msi_priv *msi = priv->msi;
	u32 hwirq, i;

	mutex_lock(&msi->lock);

	hwirq = bitmap_find_next_zero_area(msi->used, msi->num_of_vectors, 0, nr_irqs, 0);
	if (hwirq >= msi->num_of_vectors) {
		mutex_unlock(&msi->lock);
		return -ENOSPC;
	}

	bitmap_set(msi->used, hwirq, nr_irqs);

	mutex_unlock(&msi->lock);

	for (i = 0; i < nr_irqs; i++) {
		irq_domain_set_info(domain, irq + i, hwirq + i, &netx4000_pcie_msi_bottom_irq_chip, domain->host_data, handle_simple_irq, NULL, NULL);
	}

	dev_dbg(priv->dev, "Allocating irq%u..%u (hwirq%u..%u) succeeded.\n", irq, irq + nr_irqs - 1, hwirq, hwirq + nr_irqs - 1);

	return 0;
}

static void netx4000_pcie_irq_domain_free(struct irq_domain *domain, unsigned int irq, unsigned int nr_irqs)
{
	struct irq_data *irqData = irq_domain_get_irq_data(domain, irq);
	struct netx4000_pcie_priv* priv = irq_data_get_irq_chip_data(irqData);
	struct netx4000_pcie_msi_priv *msi = priv->msi;

	mutex_lock(&msi->lock);

	if (!test_bit(irqData->hwirq, msi->used)) {
		mutex_unlock(&msi->lock);
		dev_err(priv->dev, "Error trying to free unused MSI#%lu\n", irqData->hwirq);
		return;
	}

	bitmap_clear(msi->used, irqData->hwirq, nr_irqs);

	mutex_unlock(&msi->lock);

	dev_dbg(priv->dev, "Freeing irq%u..%u (hwirq%lu..%lu) succeeded.\n", irq, irq + nr_irqs - 1, irqData->hwirq, irqData->hwirq + nr_irqs - 1);
}

static const struct irq_domain_ops netx4000_pcie_msi_domain_ops = {
	.alloc = netx4000_pcie_irq_domain_alloc,
	.free  = netx4000_pcie_irq_domain_free,
};

static struct irq_chip netx4000_pcie_msi_irq_chip = {
	.name       = "pcie:msi",
	.irq_mask   = pci_msi_mask_irq, /* generic function */
	.irq_unmask = pci_msi_unmask_irq, /* generic function */
};

static struct msi_domain_info netx4000_pcie_msi_domain_info = {
	.flags = (MSI_FLAG_USE_DEF_DOM_OPS | MSI_FLAG_USE_DEF_CHIP_OPS | MSI_FLAG_MULTI_PCI_MSI),
	.chip  = &netx4000_pcie_msi_irq_chip,
};

static int netx4000_pcie_msi_irq_init(struct netx4000_pcie_priv *priv)
{
	struct netx4000_pcie_msi_priv *msi = priv->msi;
	struct fwnode_handle *fwnode = of_node_to_fwnode(priv->dev->of_node);

	msi->dmaHandleVirt = dma_alloc_coherent(priv->dev, sizeof(u32), &priv->msi->dmaHandle, GFP_KERNEL);
	if (!msi->dmaHandleVirt) {
		dev_err(priv->dev, "Error allocating memory for MSI IRQs.\n");
		return -ENOMEM;
	}
	dev_dbg(priv->dev, "Allocating MSI IRQ memory (va:%p/pa:%x) succeeded.\n", msi->dmaHandleVirt, msi->dmaHandle);

	mutex_init(&msi->lock);

	priv->msi->num_of_vectors = MAX_MSI_VECTORS;

	iowrite32(msi->dmaHandle, priv->regs + OFFS_MSI_RX_WIN_ADDR);
	iowrite32(0x3, priv->regs + OFFS_MSI_RX_WIN_MASK); /* 4 bytes */
	iowrite32(msi->dmaHandle | MSI_RX_WIN_ENABLE, priv->regs + OFFS_MSI_RX_WIN_ADDR);

	msi->devIrqDomain = irq_domain_add_linear(NULL, msi->num_of_vectors, &netx4000_pcie_msi_domain_ops, priv);
	if (!msi->devIrqDomain) {
		dev_err(priv->dev, "Error creating IRQ domain.\n");
		return -ENOMEM;
	}

	msi->msiIrqDomain = pci_msi_create_irq_domain(fwnode, &netx4000_pcie_msi_domain_info, msi->devIrqDomain);
	if (!msi->msiIrqDomain) {
		dev_err(priv->dev, "Error creating MSI domain.\n");
		irq_domain_remove(msi->devIrqDomain);
		return -ENOMEM;
	}

	netx4000_pcie_irq_enable(priv, MSI_RX_IRQ_ENABLE);

	return 0;
}

static int netx4000_pcierc_cfg_read(struct pci_bus *bus, unsigned int devfn,
				    int where, int size, u32 *val)
{
	struct netx4000_pcie_priv *priv = bus->sysdata;
	u32 reg;
	u32 result_shift;
	int ret;
	int retry;
	int retry_count = 10;
	u32 issue_type = REQ_ISSUE_TYPE_CFG_RD_1;

	dev_dbg(priv->dev, "Trying to read cfg. (bus=%u, devfn=%u, where=%d, size=%d)",
		bus->number, devfn, where, size);

	if(bus->number == priv->busnr) {
		if(PCI_SLOT(devfn) > 0) {
			ret = PCIBIOS_DEVICE_NOT_FOUND;
			goto out;
		}
		if(netx4000_pcie_wait_for_link(priv, 0 /* ms */) < 0) {
			ret = PCIBIOS_DEVICE_NOT_FOUND;
			goto out;
		}
		issue_type = REQ_ISSUE_TYPE_CFG_RD_0;
	}

retry:
	retry = 0;

	/* RAR1 contains for cfg read
	   31:24 : Bus number
	   23:19 : Device number
	   18:16 : Function number
	   11:8  : Ext. Reg. number
	   7:2   : Register number */
	reg = ((bus->number & 0xFF) << 24) |
	      ((devfn & 0xFF) << 16) |
	      (where & 0xFFC);

	result_shift = where & 0x3;

	/* NOTE: no locking required, this is already done in access.c (pci_lock) */
	iowrite32(reg, priv->regs + OFFS_REQ_ADDR_REG1);

	/* setup byte enable */
	if (size == 1)
		iowrite32(0x1 << result_shift, priv->regs + OFFS_REQ_BYTE_EN);
	else if (size == 2)
		iowrite32(0x3 << result_shift, priv->regs + OFFS_REQ_BYTE_EN);
	else
		iowrite32(0xf, priv->regs + OFFS_REQ_BYTE_EN);

	/* Issue request */
	iowrite32(issue_type | REQ_ISSUE_REQUEST,
		priv->regs + OFFS_REQ_ISSUE);

	/* Timeout is done inside hardware (Completion timeout) */
	while(ioread32(priv->regs + OFFS_REQ_ISSUE) & REQ_ISSUE_REQUEST) ;

	/* Check for errors */
	reg = ioread32(priv->regs + OFFS_REQ_ISSUE);
	switch((reg >> 16) & 0x7) {
		case 0:  ret = PCIBIOS_SUCCESSFUL;         break;
		case 1:  ret = PCIBIOS_FUNC_NOT_SUPPORTED; break; /* unsupprived request */
		case 2:  retry = 1;                        break; /* CRS (configuration request retry status) */
		case 3:  ret = PCIBIOS_DEVICE_NOT_FOUND;   break; /* completion timeout */
		case 7:  ret = PCIBIOS_BUFFER_TOO_SMALL;   break; /* overrun */
		default:
			ret = PCIBIOS_SET_FAILED;
			dev_info(priv->dev, "Config read error: 0x%02X\n", (reg >> 16) & 0x7);
			break;
	}

	if (retry) {
		if(retry_count-- > 0) {
			dev_dbg(priv->dev, "Retrying in 100ms CFG_RD after receiving CRS\n");
			mdelay(100);
			goto retry;
		} else {
			dev_err(priv->dev, "Aborting CFG_RD (too many retried after CRS)\n");
			ret = PCIBIOS_DEVICE_NOT_FOUND;
		}
	}

	if (ret == PCIBIOS_SUCCESSFUL)
		*val = ioread32(priv->regs + OFFS_REQ_RECV_DATA) >> (result_shift * 8);

out:
	dev_dbg(priv->dev, "Return from read cfg. (val=0x%08x, ret=%d)",
		*val, ret);

	return ret;
}

static int netx4000_pcierc_cfg_write(struct pci_bus *bus, unsigned int devfn,
				     int where, int size, u32 val)
{
	struct netx4000_pcie_priv *priv = bus->sysdata;
	u32 val_shift;
	u32 reg;
	int ret;
	u32 issue_type = REQ_ISSUE_TYPE_CFG_WR_1;

	dev_dbg(priv->dev, "Trying to write cfg. (bus=%u, devfn=%u, where=%d, size=%d, val=0x%08x)",
		bus->number, devfn, where, size, val);

	if(bus->number == priv->busnr) {
		if(PCI_SLOT(devfn) > 0) {
			ret = PCIBIOS_DEVICE_NOT_FOUND;
			goto out;
		}
		if(netx4000_pcie_wait_for_link(priv, 0 /* ms */) < 0) {
			ret = PCIBIOS_DEVICE_NOT_FOUND;
			goto out;
		}
		issue_type = REQ_ISSUE_TYPE_CFG_WR_0;
	}

	/* RAR1 contains for cfg read
	   31:24 : Bus number
	   23:19 : Device number
	   18:16 : Function number
	   11:8  : Ext. Reg. number
	   7:2   : Register number */
	reg = ((bus->number & 0xFF) << 24) |
	      ((devfn & 0xFF) << 16) |
	      (where & 0xFFC);

	val_shift = where &0x3;

	/* NOTE: no locking required, this is already done in access.c (pci_lock) */
	iowrite32(reg, priv->regs + OFFS_REQ_ADDR_REG1);

	/* setup byte enable */
	if (size == 1)
		iowrite32(0x1 << val_shift, priv->regs + OFFS_REQ_BYTE_EN);
	else if (size == 2)
		iowrite32(0x3 << val_shift, priv->regs + OFFS_REQ_BYTE_EN);
	else
		iowrite32(0xf, priv->regs + OFFS_REQ_BYTE_EN);

	/* Insert data to write */
	iowrite32(val << (val_shift * 8), priv->regs + OFFS_REQ_DATA3);

	/* Issue request */
	iowrite32(issue_type | REQ_ISSUE_REQUEST,
		priv->regs + OFFS_REQ_ISSUE);

	/* Timeout is done inside hardware (Completion timeout) */
	while(ioread32(priv->regs + OFFS_REQ_ISSUE) & REQ_ISSUE_REQUEST) ;

	/* Check for errors */
	reg = ioread32(priv->regs + OFFS_REQ_ISSUE);
	switch((reg >> 16) & 0x7) {
		case 0:  ret = PCIBIOS_SUCCESSFUL;         break;
		case 1:  ret = PCIBIOS_FUNC_NOT_SUPPORTED; break; /* unsupprived request */
		case 3:  ret = PCIBIOS_DEVICE_NOT_FOUND;   break; /* completion timeout */
		case 7:  ret = PCIBIOS_BUFFER_TOO_SMALL;   break; /* overrun */
		default:
			ret = PCIBIOS_SET_FAILED;
			dev_info(priv->dev, "Config write error: 0x%02X\n", (reg >> 16) & 0x7);
			break;
	}

out:
	dev_dbg(priv->dev, "Return from write cfg. (ret=%d)",  ret);

	return ret;
}

static struct pci_ops pci_netx4000_ops = {
	.read   = netx4000_pcierc_cfg_read,
	.write  = netx4000_pcierc_cfg_write,
};

static void netx4000_pcie_core_init(struct netx4000_pcie_priv *priv,
				struct list_head *pci_res)
{
	struct resource_entry *win;
	u32 axiWinAddr, axiWinSize, axiWin = 0, val32;
	int err;

	/* Reset PCIe core */
	iowrite32(0, priv->regs + OFFS_RESET);

	/* Enable writing of configuration register */
	iowrite32(RESET_LOAD_B | RESET_CFG_B, priv->regs + OFFS_RESET);

	/* Setup prefetchable / non-prefetchable bases according to device tree */
	resource_list_for_each_entry(win, pci_res) {
		switch (resource_type(win->res)) {
			case IORESOURCE_MEM:
				if (axiWin >= 4) {
					dev_err(priv->dev, "Error allocating AXI->PCI window.\n");
					continue;
				}
				axiWinAddr = win->res->start;
				axiWinSize = resource_size(win->res);

				/* AXI->PCI mapping */
				iowrite32(axiWinAddr, priv->regs + OFFS_PCI_WINx_BASE(axiWin));
				iowrite32(axiWinSize - 1, priv->regs + OFFS_PCI_WINx_MASK(axiWin));
				iowrite32(axiWinAddr, priv->regs + OFFS_PCI_DESTx_LOW(axiWin));
				iowrite32(0x00000000, priv->regs + OFFS_PCI_DESTx_HI(axiWin));
				iowrite32(axiWinAddr | 0x1, priv->regs + OFFS_PCI_WINx_BASE(axiWin));

				if (win->res->flags & IORESOURCE_PREFETCH) {
					/* Prefetchable Memory Base / Limit setup */
					iowrite32(((axiWinSize-1) & 0xffff0000) | (axiWinAddr >> 16), priv->regs + OFFS_PREF_MEM_BASE_LIMIT);
					dev_dbg(priv->dev, "AXI->PCI Win%d: IOMEM (prefetch): 0x%08x (size=%uMiB)\n", axiWin, axiWinAddr, axiWinSize/1024/1024);
				}
				else {
					/* Memory Base / Limit setup */
					iowrite32(((axiWinSize-1) & 0xffff0000) | (axiWinAddr >> 16), priv->regs + OFFS_MEM_BASE_LIMIT);
					dev_dbg(priv->dev, "AXI->PCI Win%d: IOMEM: 0x%08x (size=%uMiB)\n", axiWin, axiWinAddr, axiWinSize/1024/1024);
				}
				axiWin++;
				break;

			case IORESOURCE_BUS:
				priv->busnr = win->res->start;
				dev_dbg(priv->dev, "Bus number: %u\n", priv->busnr);
				break;

			case  IORESOURCE_IO:
				dev_dbg(priv->dev, "Error: IO-ports not supported!\n");
				break;

			default:
				continue;
		}
	}

	/* PCI->AXI mapping */
	iowrite32(PHYS_OFFSET, priv->regs + OFFS_AXI_WINx_BASE(0)); /* DDR-RAM 0x40000000 */
	iowrite32(SZ_2G - 1, priv->regs + OFFS_AXI_WINx_MASK(0)); /* max. 2GiB RAM */
	iowrite32(PHYS_OFFSET, priv->regs + OFFS_AXI_DESTx(0));
	iowrite32(PHYS_OFFSET | 0x1, priv->regs + OFFS_AXI_WINx_BASE(0));
	dev_dbg(priv->dev, "PCI->AXI Win%d: 0x%08x (size=%uMiB)\n", 0, PHYS_OFFSET, SZ_2G/1024/1024);

	/* Unlock cfg register write via AXI */
	iowrite32(PERMISSION_HWINIT_EN | PERMISSION_PIPE_PHY, priv->regs + OFFS_PERMISSION);

	/* BAR0 Setup */
	iowrite32(0xffffffff, priv->regs + 0x10A0);
	iowrite32(0xffffffff, priv->regs + 0x10A4);

	/* Busmaster/Memory Enable */
	iowrite32(0x00100006, priv->regs + 0x1004);

	/* Lock cfg register write via AXI */
	iowrite32(0, priv->regs + OFFS_PERMISSION);

	netx4000_pcie_reset_deassert(priv);

	/* Wait for CLKREQn to assert, before enabling core */
	netx4000_pcie_wait_for_clkreq(priv, 500 /* ms */);

	/* Reset must be deasserted at least 5ms later than enabling port (see chapter 6.15.2) */
	mdelay(5);

	/* release reset (should also enable clocks) */
	iowrite32(0x7f, priv->regs + OFFS_RESET);

	/* Wait for link up */
	err = netx4000_pcie_wait_for_link(priv, 500 /* ms */);
	if (err) {
		dev_err(priv->dev, "Error: PCI link down!\n");
		return;
	}

	if (priv->link_gen == 2) {
		/* Retrain for gen2 link */
		netx4000_pcie_retrain_link(priv);
	}

	val32 = ioread32(priv->regs + OFFS_LINK_CONTROL_STATUS);
	dev_dbg(priv->dev, "PCI link up (gen%d)", CURRENT_LINK_SPEED(val32));
}

static int netx4000_pcie_parse_dt(struct platform_device *pdev)
{
	struct netx4000_pcie_priv *priv = platform_get_drvdata(pdev);
	struct resource *res;
	int err;

	/* Mandatory stuff */

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Error retrieving chip base address 'reg' from DT.\n");
		return -EINVAL;
	}
	priv->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->regs)) {
		dev_err(&pdev->dev, "Error remapping chip base address.\n");
		return PTR_ERR(priv->regs);
	}

	priv->irq_all = platform_get_irq_byname(pdev, "all");
	if (priv->irq_all < 0) {
		dev_err(&pdev->dev, "Error retrieving IRQ 'all' from DT.\n");
		return priv->irq_all;
	}

	priv->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->clk)) {
		dev_err(&pdev->dev, "Error retrieving clock 'clocks' from DT.\n");
		return PTR_ERR(priv->clk);
	}

	INIT_LIST_HEAD(&priv->pci_res);
	err = devm_of_pci_get_host_bridge_resources(&pdev->dev, 0, 0xff, &priv->pci_res, NULL);
	if (err) {
		dev_err(&pdev->dev, "Error retrieving bridge resources 'ranges' from DT.\n");
		return err;
	}
	err = devm_request_pci_bus_resources(&pdev->dev, &priv->pci_res);
	if (err) {
		dev_err(&pdev->dev, "Error requesting bridge resources.\n");
		return err;
	}

	/* Limit link speed */
	err = of_property_read_u32(pdev->dev.of_node, "max-link-speed", &priv->link_gen);
	if (err)
		priv->link_gen = 2;

	priv->gpio_reset = of_get_named_gpio_flags(pdev->dev.of_node, "reset-gpio", 0, &priv->gpio_reset_flags);
	if (priv->gpio_reset < 0) {
		dev_warn(&pdev->dev, "Error retrieving gpio 'reset-gpio' from DT.\n");
	}
	else {
		if ((!gpio_is_valid(priv->gpio_reset)) || (devm_gpio_request(&pdev->dev, priv->gpio_reset, "pcie-reset") < 0)) {
			dev_err(&pdev->dev, "Error requesting reset-gpio (%d).\n", priv->gpio_reset);
			return -EINVAL;
		}
		else {
			gpio_direction_output(priv->gpio_reset, (priv->gpio_reset_flags & OF_GPIO_ACTIVE_LOW) ? 0 : 1);
		}
	}

	/* NOTE: The clkreq is normally connected to hardware clock provider. */
	priv->gpio_clkreq = of_get_named_gpio_flags(pdev->dev.of_node, "clkreq-gpio", 0, &priv->gpio_clkreq_flags);
	if (priv->gpio_clkreq < 0) {
		dev_dbg(&pdev->dev, "Error retrieving gpio 'clkreq-gpio' from DT.\n");
	}
	else {
		if ((!gpio_is_valid(priv->gpio_clkreq)) || (devm_gpio_request(&pdev->dev, priv->gpio_clkreq, "pcie-clkreq") < 0)) {
			dev_err(&pdev->dev, "Error requesting clkreq-gpio (%d).\n", priv->gpio_clkreq);
			return -EINVAL;
		}
		else {
			gpio_direction_input(priv->gpio_clkreq);
		}
	}

	return 0;
}

static void netx4000_pcie_core_enable(struct netx4000_pcie_priv *priv)
{
	int err;

	/* With 100ohm differential termination of reference clock input. */
	iowrite32(0x3e, (void __iomem*)(NETX4000_SYSTEMCTRL_VIRT_BASE + 0x18));

	/* Without 100ohm differential termination of reference clock input. */
// 	iowrite32(0x1e, (void __iomem*)(NETX4000_SYSTEMCTRL_VIRT_BASE + 0x18));

	err = clk_prepare_enable(priv->clk);
	BUG_ON(err);

	mdelay(5);
}

static void netx4000_pcie_core_disable(struct netx4000_pcie_priv *priv)
{
	u32 val32;

	/* Disable by strapping pins */
	iowrite32(0x7 /* reset value */, (void __iomem*)(NETX4000_SYSTEMCTRL_VIRT_BASE + 0x18));

	/* Same as above, but disable by core register */
	val32 = ioread32(priv->regs + OFFS_CORE_MODE_SET_1);
	val32 |= CORE_MODE_SET_1_MODE_PORT_DIS;
	iowrite32(val32, priv->regs + OFFS_CORE_MODE_SET_1);

	clk_disable_unprepare(priv->clk);
}

static int netx4000_pcie_probe(struct platform_device *pdev)
{
	struct netx4000_pcie_priv *priv;
	struct pci_bus *bus, *child;
	struct pci_host_bridge *bridge;
	int err;

	bridge = devm_pci_alloc_host_bridge(&pdev->dev, sizeof(*priv));
	if (!bridge) {
		dev_err(&pdev->dev, "Error allocating PCI host bridge structure.\n");
		return -ENOMEM;
	}

	priv = pci_host_bridge_priv(bridge);

	platform_set_drvdata(pdev, priv);

	priv->dev = &pdev->dev;

	priv->msi = devm_kzalloc(&pdev->dev, sizeof(*priv->msi), GFP_KERNEL);
	if (!priv->msi) {
		dev_err(&pdev->dev, "Error allocating private msi structure.\n");
		return -ENOMEM;
	}

	err = netx4000_pcie_parse_dt(pdev);
	if (err) {
		dev_err(&pdev->dev, "Error parsing device tree (%i).\n", err);
		goto err_out;
	}

	netx4000_pcie_core_enable(priv);

	/* Initialization */
	netx4000_pcie_core_init(priv, &priv->pci_res);

	irq_set_chained_handler_and_data(priv->irq_all, netx4000_pcie_all_isr, priv);

	err = netx4000_pcie_legacy_irq_init(priv);
	if (err) {
		dev_err(&pdev->dev, "Error initializing legacy IRQs.\n");
	}

	/* Initialize and enable interrupts */
	if (priv->msi) {
		err = netx4000_pcie_msi_irq_init(priv);
		if (err < 0 ) {
			dev_err(&pdev->dev, "Error initializing MSI IRQs.\n");
			goto err_out;
		}
	}

	list_splice_init(&priv->pci_res, &bridge->windows);
	bridge->dev.parent = &pdev->dev;
	bridge->sysdata = priv;
	bridge->busnr = priv->busnr;
	bridge->ops = &pci_netx4000_ops;
	bridge->map_irq = of_irq_parse_and_map_pci;
	bridge->swizzle_irq = pci_common_swizzle;

	err = pci_scan_root_bus_bridge(bridge);
	if (err) {
		dev_err(&pdev->dev, "Error: pci_scan_root_bus_bridge() failed!\n");
		return err;
	}

	bus = bridge->bus;

	pci_scan_child_bus(bus);
	pci_assign_unassigned_bus_resources(bus);
	list_for_each_entry(child, &bus->children, node)
		pcie_bus_configure_settings(child);
	pci_bus_add_devices(bus);

	dev_info(&pdev->dev, "successfully initialized!\n");

	return 0;

err_out:
	pci_free_resource_list(&priv->pci_res);
	irq_set_chained_handler_and_data(priv->irq_all, NULL, NULL);
	netx4000_pcie_core_disable(priv);

	irq_domain_remove(priv->msi->msiIrqDomain);
	irq_domain_remove(priv->msi->devIrqDomain);
	irq_domain_remove(priv->legacyIrqDomain);

	return err;
}

static const struct of_device_id netx4000_pcie_of_match[] = {
	{.compatible = "hilscher,netx4000-pcie-rc"},
	{},
};

static struct platform_driver netx4000_pcie_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .of_match_table = netx4000_pcie_of_match,
	},
	.probe  = netx4000_pcie_probe,
};

static int __init netx4000_pcie_init(void)
{
	pr_info("%s: %s\n", DRIVER_NAME, DRIVER_DESC);
	return platform_driver_register(&netx4000_pcie_driver);
}
module_init(netx4000_pcie_init);

static void __exit netx4000_pcie_exit(void)
{
	platform_driver_unregister(&netx4000_pcie_driver);
}
module_exit(netx4000_pcie_exit);

MODULE_AUTHOR("Hilscher Gesellschaft fuer Systemautomation mbH");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");
