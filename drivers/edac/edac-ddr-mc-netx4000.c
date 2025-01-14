/*
* EDAC DDR MC driver for Hilscher netX4000 based platforms
*
* drivers/edac/edac-ddr-mc-netx4000.c
*
* (C) Copyright 2015 Hilscher Gesellschaft fuer Systemautomation mbH
* http://www.hilscher.com
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; version 2 of
* the License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#define DRIVER_DESC "EDAC DDR MC driver for Hilscher netX4000 based platforms"
#define DRIVER_NAME "edac-ddr-mc-netx4000"

#include <linux/edac.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>

#include "edac_core.h"

/* Controller definitions and macros */
#define DENALI_CTL_0  0x00
#define dram_class(base)  (readl(base+DENALI_CTL_0) >> 8 & 0xf)

#define DENALI_CTL_1  0x04
#define max_row_reg(base)  (readl(base+DENALI_CTL_1) >> 0 & 0x1f)
#define max_col_reg(base)  (readl(base+DENALI_CTL_1) >> 8 & 0xf)

#define DENALI_CTL_39  0x9c
#define fwc(base,mask)   (writel(mask&0x1, base+DENALI_CTL_39))

#define DENALI_CTL_40  0xa0
#define xor_check_bits(base,mask)  (writel(mask&0x0fffffff, base+DENALI_CTL_40))

#define DENALI_CTL_41  0xa4
#define ecc_disable_w_uc_err(base)  (readl(base+DENALI_CTL_41) >> 0 & 0x1)

#define DENALI_CTL_42  0xa8
#define ecc_u_addr(base)  (readl(base+DENALI_CTL_42))

#define DENALI_CTL_43  0xac
#define ecc_u_synd(base)  (readl(base+DENALI_CTL_43) >> 8 & 0x7f)

#define DENALI_CTL_44  0xb0
#define ecc_u_data(base)  (readl(base+DENALI_CTL_44))

#define DENALI_CTL_45  0xb4
#define ecc_c_addr(base)  (readl(base+DENALI_CTL_45))

#define DENALI_CTL_46  0xb8
#define ecc_c_synd(base)  (readl(base+DENALI_CTL_46) >> 8 & 0x7f)

#define DENALI_CTL_47  0xbc
#define ecc_c_data(base)  (readl(base+DENALI_CTL_47))

#define DENALI_CTL_48  0xc0
#define ecc_u_id(base)  (readl(base+DENALI_CTL_48) >> 0 & 0x7ff)
#define ecc_c_id(base)  (readl(base+DENALI_CTL_48) >> 16 & 0x7ff)

#define DENALI_CTL_53  0xd4
#define bank_diff(base)  (readl(base+DENALI_CTL_53) >> 8 & 0x3)
#define row_diff(base)  (readl(base+DENALI_CTL_53) >> 16 & 0x7)
#define col_diff(base)  (readl(base+DENALI_CTL_53) >> 24 & 0xf)

#define DENALI_CTL_58  0xe8
#define reduc(base)  (readl(base+DENALI_CTL_58) >> 16 & 0x1)

#define DENALI_CTL_152  0x260
#define ecc_en(base)  (readl(base+DENALI_CTL_152) & 0x1)

#define DENALI_CTL_158  0x278
#define int_status(base)  (readl(base+DENALI_CTL_158) & 0x3ffffff)
#define IntLogicalOr  (1 << 25)
#define IntMultiUE    (1 << 6)
#define IntUE         (1 << 5)
#define IntMultiCE    (1 << 4)
#define IntCE         (1 << 3)

#define DENALI_CTL_159  0x27c
#define int_ack(base,mask)  (writel(mask & 0x1ffffff, base+DENALI_CTL_159))

#define DENALI_CTL_160  0x280
#define int_mask(base,mask)  (writel(mask & 0x3ffffff, base+DENALI_CTL_160))

#define NETX4000_EDAC_MSG_SIZE	256


/**
 * struct ecc_error_info - ECC error log information
 * @addr:	Address causing the error
 * @row:	Row number
 * @col:	Column number
 * @bank:	Bank number
 * @data:	Data causing the error
 * @synd:	Syndrom
 */
struct ecc_error_info {
	u32 addr;
	u32 row;
	u32 col;
	u32 bank;
	u32 data;
	u32 synd;
};

/**
 * struct ecc_status - ECC status information to report
 * @ce_cnt:	Correctable error count
 * @ue_cnt:	Uncorrectable error count
 * @ceinfo:	Correctable error log information
 * @ueinfo:	Uncorrectable error log information
 */
struct ecc_status {
	u32 ce_cnt;
	u32 ue_cnt;
	struct ecc_error_info ceinfo;
	struct ecc_error_info ueinfo;
};

/**
 * struct priv_data - DDR memory controller private instance data
 * @baseaddr:	Base address of the DDR controller
 * @irq:			IRQ of ECC controller
 * @cs,rows,banks,cols,dpwidth: Configured DDR-RAM parameters
 * @memsize:	Size of DDR-RAM
 * @message:	Buffer for framing the event specific info
 * @stat:			ECC status information
 */
struct priv_data {
	void __iomem *baseaddr;
	u32 irq;
	u32 cs, rows, banks, cols, dpwidth; /* Bank interleaving */
	u32 memsize;
	char message[NETX4000_EDAC_MSG_SIZE];
	struct ecc_status stat;
#ifdef CONFIG_EDAC_DDR_MC_NETX4000_ERROR_INJECTION
	uint32_t xor_check_bits;
	uint32_t lock;
#endif
};

#ifdef CONFIG_EDAC_DDR_MC_NETX4000_ERROR_INJECTION
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <asm/cacheflush.h>

/**
 * netx4000_edac_ddr_mc_thread_function
 * @data:	Pointer to private data
 *
 * Return:
 */
static int netx4000_edac_ddr_mc_thread_function(void *data)
{
	struct priv_data *priv = data;
	dma_addr_t dma_handle;
	spinlock_t mLock;
	unsigned long flags;
	volatile uint32_t *testbuf, testval;

	pr_debug("%s: running at cpu%d\n", __func__, smp_processor_id());

	testbuf = dma_alloc_coherent(NULL, sizeof(*testbuf), &dma_handle, GFP_KERNEL);
	if (testbuf == NULL) {
		pr_err("dma_alloc_coherent() failed\n");
		return -ENOMEM;
	}

	spin_lock_init(&mLock);
	spin_lock_irqsave(&mLock, flags);

	xor_check_bits(priv->baseaddr, priv->xor_check_bits);

	flush_cache_all();

	fwc(priv->baseaddr, 1);

	/* ATTENTION: The DDR controller raises an exeption in case of multiple ECC errors. */
	*testbuf = 0x20171218;
	testval = *testbuf;

	spin_unlock_irqrestore(&mLock, flags);

	dma_free_coherent(NULL, sizeof(*testbuf), (void*)testbuf, dma_handle);

	priv->lock = 0;

    return 0;
}

/**
 * netx4000_edac_ddr_mc_error_injection_store
 * @dev:	Pointer to the platform_device struct
 * @mattr:
 * @buf:	Pointer to user data
 * @count:	Number of given data bytes
 *
 * Return:
 */
static ssize_t netx4000_edac_ddr_mc_error_injection_store(struct device *dev, struct device_attribute *mattr, const char *buf, size_t count)
{
	struct mem_ctl_info *mci = to_mci(dev);
	struct priv_data *priv = mci->pvt_info;
	struct task_struct *task;
	spinlock_t mLock;
	unsigned long flags, cpu;

	pr_debug("%s: running at cpu%d\n", __func__, smp_processor_id());

	if (sscanf(buf, "%x", &priv->xor_check_bits) < 1) {
		pr_err("Invalid or missing arguments [xor_check_bits]\n");
		return -EINVAL;
	}

	(smp_processor_id()) ? (cpu = 0) : (cpu = 1);

	spin_lock_init(&mLock);
	spin_lock_irqsave(&mLock, flags);

	priv->lock = 1;
	task = kthread_create(&netx4000_edac_ddr_mc_thread_function,(void *)priv,"netx4000-edac-ddr-mc");
	if (task) {
		kthread_bind(task, cpu);
		flush_cache_all();
    	wake_up_process(task);

		while (priv->lock) {
			/* TODO: timeout handling */
			udelay(100);
		}
	}

	spin_unlock_irqrestore(&mLock, flags);

	return count;
}

static DEVICE_ATTR(error_injection, S_IWUSR, NULL, netx4000_edac_ddr_mc_error_injection_store);

static struct attribute *netx4000_edac_ddr_mc_dev_attrs[] = {
	&dev_attr_error_injection.attr,
	NULL
};

ATTRIBUTE_GROUPS(netx4000_edac_ddr_mc_dev);

#endif /* CONFIG_EDAC_DDR_MC_NETX4000_ERROR_INJECTION */


/**
 * netx4000_edac_ddr_mc_get_error_info - Get the current ecc error info
 * @mci:		Pointer to the edac memory controller instance
 * @status:	Interrupt status
 *
 * Determines there is any ecc error or not.
 *
 * Return: Acknowledge mask of handled IRQs (ack must be done by caller)
 */
static int netx4000_edac_ddr_mc_get_error_info(struct mem_ctl_info *mci, u32 status)
{
	struct priv_data *priv = mci->pvt_info;
	struct ecc_status *stat = &priv->stat;
	u32 ack = 0;

	if (status & IntCE) {
		stat->ceinfo.addr = ecc_c_addr(priv->baseaddr) + PHYS_OFFSET;
		stat->ceinfo.row = (stat->ceinfo.addr >> (priv->banks + priv->cols + 2)) & ((1 << priv->rows)-1);
		stat->ceinfo.bank = (stat->ceinfo.addr >> (priv->cols + 2)) & ((1 << priv->banks)-1);
		stat->ceinfo.col = (stat->ceinfo.addr >> 2) & ((1 << priv->cols)-1);
		stat->ceinfo.data = ecc_c_data(priv->baseaddr);
		stat->ceinfo.synd = ecc_c_synd(priv->baseaddr);
		stat->ce_cnt++;
		ack |= IntCE;
	}
	if (status & IntMultiCE) {
		stat->ce_cnt++;
		ack |= IntMultiCE;
	}
	if (status & IntUE) {
		stat->ueinfo.addr = ecc_u_addr(priv->baseaddr) + PHYS_OFFSET;
		stat->ueinfo.row = (stat->ueinfo.addr >> (priv->banks + priv->cols + 2)) & ((1 << priv->rows)-1);
		stat->ueinfo.bank = (stat->ueinfo.addr >> (priv->cols + 2)) & ((1 << priv->banks)-1);
		stat->ueinfo.col = (stat->ueinfo.addr >> 2) & ((1 << priv->cols)-1);
		stat->ueinfo.data = ecc_u_data(priv->baseaddr);
		stat->ueinfo.synd = ecc_u_synd(priv->baseaddr);
		stat->ue_cnt++;
		ack |= IntUE;
	}
	if (status & IntMultiUE) {
		stat->ue_cnt++;
		ack |= IntMultiUE;
	}

	return ack;
}

/**
 * netx4000_edac_ddr_mc_handle_error - Handle controller error types CE and UE
 * @mci:	Pointer to the edac memory controller instance
 *
 * Handles the controller ECC correctable and un correctable error.
 */
static void netx4000_edac_ddr_mc_handle_error(struct mem_ctl_info *mci)
{
	struct priv_data *priv = mci->pvt_info;
	struct ecc_status *stat = &priv->stat;
	struct ecc_error_info *pinf;

	if (stat->ce_cnt) {
		pinf = &stat->ceinfo;
		snprintf(priv->message, NETX4000_EDAC_MSG_SIZE, "detected - DDR: Addr 0x%08x (Row 0x%x, Bank 0x%x, Col 0x%x), Data 0x%08x, Syndrome 0x%02x"
			, pinf->addr, pinf->row, pinf->bank, pinf->col, pinf->data, pinf->synd);
		edac_mc_handle_error(HW_EVENT_ERR_CORRECTED, mci, stat->ce_cnt, __phys_to_pfn(pinf->addr), pinf->addr & ~PAGE_MASK, pinf->synd, 0, 0, -1, priv->message, "");
	}

	if (stat->ue_cnt) {
		pinf = &stat->ueinfo;
		snprintf(priv->message, NETX4000_EDAC_MSG_SIZE, "detected - DDR: Addr 0x%08x (Row 0x%x, Bank 0x%x, Col 0x%x), Data 0x%08x, Syndrome 0x%02x"
			, pinf->addr, pinf->row, pinf->bank, pinf->col, pinf->data, pinf->synd);
		edac_mc_handle_error(HW_EVENT_ERR_UNCORRECTED, mci, stat->ue_cnt, __phys_to_pfn(pinf->addr), pinf->addr & ~PAGE_MASK, pinf->synd, 0, 0, -1, priv->message, "");
	}

	memset(stat, 0, sizeof(*stat));
}

/**
 * netx4000_edac_ddr_mc_check - Check controller for ECC errors
 * @mci:	Pointer to the edac memory controller instance
 *
 * Used to check and post ECC errors. Called by the polling thread.
 */
static void netx4000_edac_ddr_mc_check(struct mem_ctl_info *mci)
{

	struct priv_data *priv = mci->pvt_info;
	u32 status, ack = 0;

	status = int_status(priv->baseaddr);

	ack |= netx4000_edac_ddr_mc_get_error_info(mci, status);
	if (ack) {
		int_ack(priv->baseaddr,ack);
		netx4000_edac_ddr_mc_handle_error(mci);
	}
}

/**
 * netx4000_edac_ddr_mc_isr - Check controller for ECC errors
 * @irq:		IRQ number
 * @dev_id:	Pointer to the edac memory controller instance
 *
 * Interrupt subroutine used to check and post ECC errors.
 */
static irqreturn_t netx4000_edac_ddr_mc_isr(int irq, void *dev_id)
{
	struct mem_ctl_info *mci = dev_id;
	struct priv_data *priv = mci->pvt_info;
	u32 status, ack = 0;

	status = int_status(priv->baseaddr);
	status &= ~IntLogicalOr; /* masks out the logical OR bit */

	ack |= netx4000_edac_ddr_mc_get_error_info(mci, status);

	status ^= ack;
	if (status)
		pr_warn("Warning: Unsupported IRQ occured (status 0x%08x)!", status);

	if (ack) {
		int_ack(priv->baseaddr,ack); /* ack of IRQs must be done prior the error handling! */

		if (ack & (IntMultiUE | IntUE | IntMultiCE | IntCE)) /* ECC IRQs */
			netx4000_edac_ddr_mc_handle_error(mci);

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

/**
 * netx4000_edac_ddr_mc_get_eccstate - Return the controller ecc enable/disable status
 * @base:	Pointer to the ddr memory controller base address
 *
 * Get the ECC enable/disable status for the controller
 *
 * Return: A ecc status boolean true(enabled)/false(disabled)
 */
static bool netx4000_edac_ddr_mc_get_eccstate(void __iomem *base)
{
	return ecc_en(base) ? true : false;
}

/**
 * netx4000_edac_ddr_mc_get_mtype - Returns controller memory type
 * @base:	pointer to the synopsys ecc status structure
 *
 * Get the EDAC memory type appropriate for the current controller
 * configuration.
 *
 * Return: A memory type enumeration.
 */
static enum mem_type netx4000_edac_ddr_mc_get_mtype(const void __iomem *base)
{
	enum mem_type memtype = MEM_UNKNOWN;

	switch dram_class(base) {
		case 0x4:
			memtype = MEM_DDR2;
			break;
		case 0x6:
			memtype = MEM_DDR3;
			break;
	}

	return memtype;
}

/**
 * netx4000_edac_ddr_mc_get_dtype - Return the controller memory width
 * @base:	Pointer to the ddr memory controller base address
 *
 * Get the EDAC device type width appropriate for the current controller
 * configuration.
 *
 * Return: A device type width enumeration.
 */
static enum dev_type netx4000_edac_ddr_mc_get_dtype(const void __iomem *base)
{
	return reduc(base) ? DEV_X16 : DEV_X32;
}

/**
 * netx4000_edac_ddr_mc_init_csrows - Initialize the cs row data
 * @mci:	Pointer to the edac memory controller instance
 *
 * Initializes the chip select rows associated with the EDAC memory
 * controller instance
 *
 * Return: Unconditionally 0.
 */
static int netx4000_edac_ddr_mc_init_csrows(struct mem_ctl_info *mci)
{
	struct csrow_info *csi;
	struct dimm_info *dimm;
	struct priv_data *priv = mci->pvt_info;
	int row, j;

	/* Calulate the memory size of attached DDR RAM */
	priv->cs = 1;
	priv->rows = max_row_reg(priv->baseaddr)-row_diff(priv->baseaddr);
	priv->banks = 3-bank_diff(priv->baseaddr);
	priv->cols = max_col_reg(priv->baseaddr)-col_diff(priv->baseaddr);
	priv->dpwidth = netx4000_edac_ddr_mc_get_dtype(priv->baseaddr);
	priv->memsize = (priv->cs) * (1<<(priv->rows+priv->cols)) * (1<<priv->banks) * ((priv->dpwidth == DEV_X16) ? 2 : 4);

	for (row = 0; row < mci->nr_csrows; row++) {
		csi = mci->csrows[row];

		for (j = 0; j < csi->nr_channels; j++) {
			dimm            = csi->channels[j]->dimm;
			snprintf(dimm->label, sizeof(dimm->label), "onboard RAM");
			dimm->grain     = 4;
			dimm->dtype     = netx4000_edac_ddr_mc_get_dtype(priv->baseaddr);
			dimm->mtype     = netx4000_edac_ddr_mc_get_mtype(priv->baseaddr);
			dimm->edac_mode = EDAC_FLAG_SECDED;
			dimm->nr_pages  = __phys_to_pfn(priv->memsize) / csi->nr_channels;
		}
	}

	return 0;
}

/**
 * netx4000_edac_ddr_mc_init - Initialize driver instance
 * @mci:	Pointer to the edac memory controller instance
 * @pdev:	Pointer to the platform_device struct
 *
 * Performs initialization of the EDAC memory controller instance and
 * related driver-private data associated with the memory controller the
 * instance is bound to.
 *
 * Return: Always zero.
 */
static int netx4000_edac_ddr_mc_init_mci(struct mem_ctl_info *mci, struct platform_device *pdev)
{
	int status;
	struct priv_data *priv;

	mci->pdev = &pdev->dev;
	priv = mci->pvt_info;
	platform_set_drvdata(pdev, mci);

	/* Initialize controller capabilities and configuration */
	mci->mtype_cap = MEM_FLAG_DDR2 | MEM_FLAG_DDR3;
	mci->edac_ctl_cap = EDAC_FLAG_NONE | EDAC_FLAG_SECDED; /* FIXME: EDAC_FLAG_NONE */
	mci->scrub_mode = SCRUB_SW_SRC;

	mci->edac_cap = EDAC_FLAG_SECDED;
	mci->dev_name = dev_name(&pdev->dev);
	mci->ctl_name = DRIVER_NAME;
	mci->mod_name = DRIVER_NAME;
	if (edac_op_state == EDAC_OPSTATE_POLL)
		mci->edac_check = netx4000_edac_ddr_mc_check;
	mci->ctl_page_to_phys = NULL;

	status = netx4000_edac_ddr_mc_init_csrows(mci);

	return status;
}

/**
 * netx4000_edac_ddr_mc_probe - Check controller and bind driver
 * @pdev:	Pointer to the platform_device struct
 *
 * Probes a specific controller instance for binding with the driver.
 *
 * Return: 0 if the controller instance was successfully bound to the
 * driver; otherwise, < 0 on error.
 */
static int netx4000_edac_ddr_mc_probe(struct platform_device *pdev)
{
	struct mem_ctl_info *mci;
	struct edac_mc_layer layers[2];
	struct priv_data *priv;
	struct resource *res;
	void __iomem *baseaddr;
	int rc, irq = 0, change_op_state = 0;

	switch (edac_op_state) {
		case EDAC_OPSTATE_INVAL:
			change_op_state = 1;
			edac_op_state = EDAC_OPSTATE_INT;
		case EDAC_OPSTATE_POLL:
		case EDAC_OPSTATE_INT:
			break;
		default:
			dev_err(&pdev->dev, "Error: Unsupported edac_op_state (%d)\n", edac_op_state);
			return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Error: 'reg' not provided in DT");
		return -EINVAL;
	}

	if (edac_op_state == EDAC_OPSTATE_INT) {
		irq = platform_get_irq(pdev, 0);
		if (irq < 0) {
			if (change_op_state) {
				dev_warn(&pdev->dev, "Warning: 'interrupts' not provided in DT => Fallback to polling mode");
				edac_op_state = EDAC_OPSTATE_POLL;
			}
			else {
				dev_err(&pdev->dev, "Error: 'interrupts' not provided in DT\n");
				return -EINVAL;
			}
		}
	}

	baseaddr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(baseaddr))
		return PTR_ERR(baseaddr);

	if (!netx4000_edac_ddr_mc_get_eccstate(baseaddr)) {
		dev_err(&pdev->dev, "Error: ECC is disabled\n");
		return -ENXIO;
	}

	layers[0].type = EDAC_MC_LAYER_CHIP_SELECT;
	layers[0].size = 1;
	layers[0].is_virt_csrow = true;
	layers[1].type = EDAC_MC_LAYER_CHANNEL;
	layers[1].size = 1;
	layers[1].is_virt_csrow = false;

	mci = edac_mc_alloc(0, ARRAY_SIZE(layers), layers, sizeof(struct priv_data));
	if (!mci) {
		dev_err(&pdev->dev, "Error: Failed memory allocation for mc instance\n");
		return -ENOMEM;
	}

	priv = mci->pvt_info;
	priv->baseaddr = baseaddr;

	rc = netx4000_edac_ddr_mc_init_mci(mci, pdev);
	if (rc) {
		dev_err(&pdev->dev, "Error: Failed to initialize mc instance\n");
		goto free_edac_mc;
	}

#ifdef CONFIG_EDAC_DDR_MC_NETX4000_ERROR_INJECTION
	rc = edac_mc_add_mc_with_groups(mci, netx4000_edac_ddr_mc_dev_groups);
#else
	rc = edac_mc_add_mc(mci);
#endif

	if (rc) {
		dev_err(&pdev->dev, "Error: Failed to register with EDAC core\n");
		goto free_edac_mc;
	}

	if (edac_op_state == EDAC_OPSTATE_INT) {
		priv->irq = irq;
		int_ack(baseaddr, -1); /* Clear all pending IRQs */
		int_mask(baseaddr, ~(IntLogicalOr | IntMultiUE | IntUE | IntMultiCE | IntCE)); /* Enable ECC IRQs */
		rc = devm_request_irq(&pdev->dev, priv->irq, netx4000_edac_ddr_mc_isr, 0, dev_name(&pdev->dev), mci);
		if (rc < 0) {
			dev_err(&pdev->dev, "Error: Unable to request irq %d\n", priv->irq);
			rc = -ENODEV;
			goto free_edac_mc;
		}
	}

	dev_info(&pdev->dev, "successfully initialized!\n");

	return rc;

free_edac_mc:
	edac_mc_free(mci);

	return rc;
}

/**
 * netx4000_edac_ddr_mc_remove - Unbind driver from controller
 * @pdev:	Pointer to the platform_device struct
 *
 * Return: Unconditionally 0
 */
static int netx4000_edac_ddr_mc_remove(struct platform_device *pdev)
{
	struct mem_ctl_info *mci = platform_get_drvdata(pdev);
	struct priv_data *priv = mci->pvt_info;

	int_mask(priv->baseaddr, 0); /* disable ECC IRQs */

	edac_mc_del_mc(&pdev->dev);
	edac_mc_free(mci);

	dev_info(&pdev->dev, "successfully removed!\n");

	return 0;
}



static const struct of_device_id netx4000_edac_ddr_mc_match[] = {
	{ .compatible = "hilscher,edac-ddr-mc-netx4000", },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, netx4000_edac_ddr_mc_match);

static struct platform_driver netx4000_edac_ddr_mc_driver = {
	.probe = netx4000_edac_ddr_mc_probe,
	.remove = netx4000_edac_ddr_mc_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = netx4000_edac_ddr_mc_match,
	},
};

static int __init netx4000_edac_ddr_mc_init(void)
{
	pr_info("%s: %s\n", DRIVER_NAME, DRIVER_DESC);
	return platform_driver_register(&netx4000_edac_ddr_mc_driver);
}
module_init(netx4000_edac_ddr_mc_init);

static void __exit netx4000_edac_ddr_mc_exit(void)
{
	platform_driver_unregister(&netx4000_edac_ddr_mc_driver);
}
module_exit(netx4000_edac_ddr_mc_exit);

MODULE_AUTHOR("Hilscher Gesellschaft fuer Systemautomation mbH");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");

