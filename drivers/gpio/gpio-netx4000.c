/*
* GPIO driver for Hilscher netx4000 based platforms
*
* drivers/gpio/gpio-netx4000.c
*
* (C) Copyright 2016 Hilscher Gesellschaft fuer Systemautomation mbH
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

#define DRIVER_DESC  "GPIO driver for Hilscher netx4000 based platforms"
#define DRIVER_NAME  "gpio-netx4000"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

#define NETX4000_GPIO_MAX_NGPIO	(32)

#define NETX4000_GPIO_IN	(0x00)
#define NETX4000_GPIO_OUT	(0x08)
#define NETX4000_GPIO_OE	(0x18)
#define NETX4000_GPIO_IRQ_SRC	(0x1C)
#define NETX4000_GPIO_IRQ_PEDGE	(0x20)
#define NETX4000_GPIO_IRQ_NEDGE	(0x24)
#define NETX4000_GPIO_OUT_SET	(0x28)
#define NETX4000_GPIO_OUT_CLR	(0x2C)
#define NETX4000_GPIO_OE_SET	(0x30)
#define NETX4000_GPIO_OE_CLR	(0x34)

struct netx4000_gpio_chip {
	struct of_mm_gpio_chip chip;
	struct irq_chip irq_chip;

	/* irq handling stuff */
	spinlock_t gpio_lock;
	int irq;
	u32 posedge_enable;
	u32 negedge_enable;
	u32 irq_enabled;
};

static struct netx4000_gpio_chip *to_netx4000_gc(struct gpio_chip *gc)
{
	return container_of(gc, struct netx4000_gpio_chip, chip.gc);
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,3,0)
static void netx4000_gpio_irq_handler(unsigned int irq, struct irq_desc *desc)
#else
static void netx4000_gpio_irq_handler(struct irq_desc *desc)
#endif
{
	struct netx4000_gpio_chip *netx4000_gc;
	int i;
	struct irq_domain *irqdomain;
	struct irq_chip *chip;
	unsigned long status;

	netx4000_gc = to_netx4000_gc(irq_desc_get_handler_data(desc));
	chip = irq_desc_get_chip(desc);
	irqdomain = netx4000_gc->chip.gc.irq.domain;

	chained_irq_enter(chip, desc);

	status = readl(netx4000_gc->chip.regs + NETX4000_GPIO_IRQ_SRC);
	writel(status, netx4000_gc->chip.regs + NETX4000_GPIO_IRQ_SRC);

	for_each_set_bit(i, &status, netx4000_gc->chip.gc.ngpio)
		generic_handle_irq(irq_find_mapping(irqdomain, i));

	chained_irq_exit(chip, desc);
}

static void netx4000_gpio_irq_unmask(struct irq_data *d)
{
	struct netx4000_gpio_chip *netx4000_gc;
	unsigned offset;
	unsigned long flags;

	netx4000_gc = to_netx4000_gc(irq_data_get_irq_chip_data(d));
	offset = d->hwirq;

	spin_lock_irqsave(&netx4000_gc->gpio_lock, flags);
	writel(netx4000_gc->posedge_enable,
		netx4000_gc->chip.regs + NETX4000_GPIO_IRQ_PEDGE);
	writel(netx4000_gc->negedge_enable,
		netx4000_gc->chip.regs + NETX4000_GPIO_IRQ_NEDGE);

	netx4000_gc->irq_enabled |= (1 << offset);
	spin_unlock_irqrestore(&netx4000_gc->gpio_lock, flags);
}

static void netx4000_gpio_irq_mask(struct irq_data *d)
{
	struct netx4000_gpio_chip *netx4000_gc;
	unsigned offset;
	unsigned long flags;

	netx4000_gc = to_netx4000_gc(irq_data_get_irq_chip_data(d));
	offset = d->hwirq;

	spin_lock_irqsave(&netx4000_gc->gpio_lock, flags);
	writel(netx4000_gc->posedge_enable & ~(1 << offset),
		netx4000_gc->chip.regs + NETX4000_GPIO_IRQ_PEDGE);
	writel(netx4000_gc->negedge_enable & ~(1 << offset),
		netx4000_gc->chip.regs + NETX4000_GPIO_IRQ_NEDGE);

	netx4000_gc->irq_enabled &= ~(1 << offset);
	spin_unlock_irqrestore(&netx4000_gc->gpio_lock, flags);
}

static int netx4000_gpio_irq_set_type(struct irq_data *d, unsigned int type)
{
	struct netx4000_gpio_chip *netx4000_gc;
	unsigned offset;
	unsigned long flags;
	int ret = 0;

	netx4000_gc = to_netx4000_gc(irq_data_get_irq_chip_data(d));
	offset = d->hwirq;

	spin_lock_irqsave(&netx4000_gc->gpio_lock, flags);
	switch(type) {
	case IRQ_TYPE_NONE:
		netx4000_gc->posedge_enable &= ~(1 << offset);
		netx4000_gc->negedge_enable &= ~(1 << offset);
		break;

	case IRQ_TYPE_EDGE_RISING:
		netx4000_gc->posedge_enable |= (1 << offset);
		netx4000_gc->negedge_enable &= ~(1 << offset);
		break;

	case IRQ_TYPE_EDGE_FALLING:
		netx4000_gc->posedge_enable &= ~(1 << offset);
		netx4000_gc->negedge_enable |= (1 << offset);
		break;

	case IRQ_TYPE_EDGE_BOTH:
		netx4000_gc->posedge_enable |= (1 << offset);
		netx4000_gc->negedge_enable |= (1 << offset);
		break;

	case IRQ_TYPE_LEVEL_HIGH:
	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock_irqrestore(&netx4000_gc->gpio_lock, flags);

	return ret;
}

static int netx4000_gpio_get(struct gpio_chip *gc, unsigned offset)
{
	struct of_mm_gpio_chip *mm_gc;
	int ret;

	mm_gc = to_of_mm_gpio_chip(gc);

	if(readl(mm_gc->regs + NETX4000_GPIO_OE) & (1 << offset))
		ret = readl(mm_gc->regs + NETX4000_GPIO_OUT) & (1 << offset);
	else
		ret = readl(mm_gc->regs + NETX4000_GPIO_IN) & (1 << offset);

	return ret;
}

static void netx4000_gpio_set(struct gpio_chip *gc, unsigned offset, int value)
{
	struct of_mm_gpio_chip *mm_gc;

	mm_gc = to_of_mm_gpio_chip(gc);

	if(value)
		writel(1 << offset, mm_gc->regs + NETX4000_GPIO_OUT_SET);
	else
		writel(1 << offset, mm_gc->regs + NETX4000_GPIO_OUT_CLR);
}

static int netx4000_gpio_direction_input(struct gpio_chip *gc, unsigned offset)
{
	struct of_mm_gpio_chip *mm_gc;

	mm_gc = to_of_mm_gpio_chip(gc);

	writel(1 << offset, mm_gc->regs + NETX4000_GPIO_OE_CLR);

	return 0;
}

static int netx4000_gpio_direction_output(struct gpio_chip *gc, unsigned offset, int value)
{
	struct of_mm_gpio_chip *mm_gc;

	mm_gc = to_of_mm_gpio_chip(gc);

	netx4000_gpio_set(gc, offset, value);
	writel(1 << offset, mm_gc->regs + NETX4000_GPIO_OE_SET);

	return 0;
}

static int netx4000_gpio_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int ret;
	struct netx4000_gpio_chip *netx4000_gc;

	netx4000_gc = devm_kzalloc(&pdev->dev, sizeof(*netx4000_gc), GFP_KERNEL);
	if (!netx4000_gc)
		return -ENOMEM;

	spin_lock_init(&netx4000_gc->gpio_lock);

	netx4000_gc->chip.gc.ngpio 		= NETX4000_GPIO_MAX_NGPIO;
	netx4000_gc->chip.gc.direction_input	= netx4000_gpio_direction_input;
	netx4000_gc->chip.gc.direction_output	= netx4000_gpio_direction_output;
	netx4000_gc->chip.gc.get		= netx4000_gpio_get;
	netx4000_gc->chip.gc.set		= netx4000_gpio_set;
	netx4000_gc->chip.gc.owner		= THIS_MODULE;
	netx4000_gc->chip.gc.parent		= &pdev->dev;

	if (of_property_read_bool(node, "gpio-ranges")) {
		netx4000_gc->chip.gc.request = gpiochip_generic_request;
		netx4000_gc->chip.gc.free = gpiochip_generic_free;
	}

	ret = of_mm_gpiochip_add(node, &netx4000_gc->chip);
	if (ret) {
		dev_err(&pdev->dev, "Failed adding memory mapped gpiochip\n");
		return ret;
	}

	platform_set_drvdata(pdev, netx4000_gc);

	netx4000_gc->irq = platform_get_irq(pdev, 0);

	if (netx4000_gc->irq < 0)
		goto skip_irq;

	netx4000_gc->irq_chip.name         = "netx4000-gpio";
	netx4000_gc->irq_chip.irq_mask     = netx4000_gpio_irq_mask;
	netx4000_gc->irq_chip.irq_unmask   = netx4000_gpio_irq_unmask;
	netx4000_gc->irq_chip.irq_set_type = netx4000_gpio_irq_set_type;

	ret = gpiochip_irqchip_add(&netx4000_gc->chip.gc, &netx4000_gc->irq_chip, 0,
		handle_simple_irq, IRQ_TYPE_NONE);

	if (ret) {
		dev_info(&pdev->dev, "could not add irqchip\n");
		return ret;
	}

	gpiochip_set_chained_irqchip(&netx4000_gc->chip.gc,
		&netx4000_gc->irq_chip,
		netx4000_gc->irq,
		netx4000_gpio_irq_handler);

	dev_info(&pdev->dev, "successfully initialized!\n");

skip_irq:
	return 0;
}

static int netx4000_gpio_remove(struct platform_device *pdev)
{
	struct netx4000_gpio_chip *netx4000_gc = platform_get_drvdata(pdev);

	of_mm_gpiochip_remove(&netx4000_gc->chip);

	dev_info(&pdev->dev, "successfully removed!\n");

	return -EIO;
}

static const struct of_device_id netx4000_gpio_of_match[] = {
	{ .compatible = "hilscher,netx4000-gpio", },
	{},
};
MODULE_DEVICE_TABLE(of, netx4000_gpio_of_match);

static struct platform_driver netx4000_gpio_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(netx4000_gpio_of_match),
	},
	.probe		= netx4000_gpio_probe,
	.remove		= netx4000_gpio_remove,
};

static int __init netx4000_gpio_init(void)
{
	pr_info("%s: %s\n", DRIVER_NAME, DRIVER_DESC);
	return platform_driver_register(&netx4000_gpio_driver);
}
subsys_initcall(netx4000_gpio_init);

static void __exit netx4000_gpio_exit(void)
{
	platform_driver_unregister(&netx4000_gpio_driver);
}
module_exit(netx4000_gpio_exit);

MODULE_AUTHOR("Hilscher Gesellschaft fuer Systemautomation mbH");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");

