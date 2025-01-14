/*
* IIO (ADC) driver for Hilscher netX4000 based platforms
*
* iio-adc-netx4000.c
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


#define DRIVER_DESC "IIO (ADC) driver for Hilscher netX4000 based platforms"
#define DRIVER_NAME "iio-adc-netx4000"


#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/iio/iio.h>
#include <linux/io.h>
#include <linux/of_address.h>

#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#include <linux/delay.h>

/* Regdef for the ADC */
#define CONFIG				0x00 // general config register
#define TASK0				0x04 // ADC control register for task0
#define TASK1				0x08 // ADC control register for task1
#define TASK2				0x0c // ADC control register for task2
#define TASK3				0x10 // ADC control register for task3
#define TASK4				0x14 // ADC control register for task4
#define TASK5				0x18 // ADC control register for task5
#define TASK6				0x1c // ADC control register for task6
#define TASK7				0x20 // ADC control register for task7
#define TASK8				0x24 // ADC control register for task8
#define TASK9				0x28 // ADC control register for task9
#define TASK10				0x2c // ADC control register for task10
#define TASK11				0x30 // ADC control register for task11
#define TASK12				0x34 // ADC control register for task12
#define TASK13				0x38 // ADC control register for task13
#define TASK14				0x3c // ADC control register for task14
#define TASK(x)				(TASK0 + 0x04 * (x))
#define SEQ0_CTRL			0x40 // Sequencer0 control register
#define SEQ1_CTRL			0x44 // Sequencer1 control register
#define SEQ2_CTRL			0x48 // Sequencer2 control register
#define SEQ3_CTRL			0x4c // Sequencer3 control register
#define SEQ_CTRL(x)			(SEQ0_CTRL + 0x04 * (x))
#define SEQ0_STATUS			0x50 // Sequence 0 status register
#define SEQ1_STATUS			0x54 // Sequence 1 status register
#define SEQ2_STATUS			0x58 // Sequence 2 status register
#define SEQ3_STATUS			0x5c // Sequence 3 status register
#define SEQ_STATUS(x)		(SEQ0_STATUS + 0x04 * (x))
#define SEQ0_VAL			0x60 // ADC value
#define SEQ1_VAL			0x64 // ADC value
#define SEQ2_VAL			0x68 // ADC value
#define SEQ3_VAL			0x6c // ADC value
#define SEQ_VAL(x)			(SEQ0_VAL + 0x04 * (x))
#define COMPARE0_CONFIG		0x70 // Digital comparator 0 config register
#define COMPARE1_CONFIG		0x74 // Digital comparator 1 config register
#define COMPARE2_CONFIG		0x78 // Digital comparator 2 config register
#define COMPARE3_CONFIG		0x7c // Digital comparator 3 config register
#define COMPARE4_CONFIG		0x80 // Digital comparator 4 config register
#define COMPARE5_CONFIG		0x84 // Digital comparator 5 config register
#define COMPARE6_CONFIG		0x88 // Digital comparator 6 config register
#define COMPARE7_CONFIG		0x8c // Digital comparator 7 config register
#define COMPARE_CONFIG(x)	(COMPARE0_CONFIG + 0x04 * (x))
#define DEBUG_CONFIG		0x90 // ADC config register for direct control
#define DEBUG_STATUS		0x94 // Debug status register
#define DEBUG_VAL			0x98 // ADC value in debug mode
#define IRQ_RAW				0x9c // Raw IRQ
#define IRQ_MASKED			0xa0 // Masked IRQ
#define IRQ_MSK_SET			0xa4 // IRQ mask set
#define IRQ_MSK_RESET		0xa8 // IRQ mask reset
#define DEBUG_CTRL0			0xac // ADC debug parameters for statemachine
#define DEBUG_CTRL1			0xb0 // ADC debug parameters for statemachine

#define setBF(val,regpos)	((val & regpos##_mask) << regpos##_shift)
#define getBF(val,regpos)	(val >> regpos##_shift & regpos##_mask)

#define TASKx_IRQ_EN_mask			0x1
#define TASKx_IRQ_EN_shift			31
#define TASKx_COMPARE_SEL_mask		0xf
#define TASKx_COMPARE_SEL_shift		27
#define TASKx_OUTPUT_DISABLE_mask	0x1
#define TASKx_OUTPUT_DISABLE_shift	26
#define TASKx_TIMESTAMP_mask		0x1
#define TASKx_TIMESTAMP_shift		25
#define TASKx_OVERSAMPLING_mask		0x7
#define TASKx_OVERSAMPLING_shift	22
#define TASKx_SAMPLE_AIN8_mask		0x1
#define TASKx_SAMPLE_AIN8_shift		21
#define TASKx_SAMPLE_AIN7_mask		0x1
#define TASKx_SAMPLE_AIN7_shift		20
#define TASKx_SAMPLE_AIN6_mask		0x1
#define TASKx_SAMPLE_AIN6_shift		19
#define TASKx_ANALOG_SEL_mask		0xf
#define TASKx_ANALOG_SEL_shift		15
#define TASKx_ANALOG_SEL(val)		setBF(val,TASKx_ANALOG_SEL)
#define TASKx_START_DELAY_mask		0xf
#define TASKx_START_DELAY_shift		11
#define TASKx_NXT_TASK_mask			0xf
#define TASKx_NXT_TASK_shift		7
#define TASKx_NXT_TASK(val)			setBF(val,TASKx_NXT_TASK)
#define TASKx_START_COND_mask		0x3f
#define TASKx_START_COND_shift		0
#define TASKx_START_COND(val)		setBF(val,TASKx_START_COND)

#define SEQx_CTRL_TIMER_mask		0xff
#define SEQx_CTRL_TIMER_shift		8
#define SEQx_CTRL_FIRST_TASK_mask	0xf
#define SEQx_CTRL_FIRST_TASK_shift	4
#define SEQx_CTRL_FIRST_TASK(val)	setBF(val,SEQx_CTRL_FIRST_TASK)
#define SEQx_CTRL_RESTART_mask		0x1
#define SEQx_CTRL_RESTART_shift		1
#define SEQx_CTRL_RESTART(val)		setBF(val,SEQx_CTRL_RESTART)
#define SEQx_CTRL_ENABLE_mask		0x1
#define SEQx_CTRL_ENABLE_shift		0
#define SEQx_CTRL_ENABLE(val)		setBF(val,SEQx_CTRL_ENABLE)

#define SEQx_STATUS_FINISHED_mask	0x1
#define SEQx_STATUS_FINISHED_shift	6
#define gSEQx_STATUS_FINISHED(val)	getBF(val,SEQx_STATUS_FINISHED)
#define SEQx_STATUS_URUN_mask		0x1
#define SEQx_STATUS_URUN_shift		5
#define gSEQx_STATUS_URUN(val)		getBF(val,SEQx_STATUS_URUN)
#define SEQx_STATUS_OVFL_mask		0x1
#define SEQx_STATUS_OVFL_shift		4
#define gSEQx_STATUS_OVFL(val)		getBF(val,SEQx_STATUS_OVFL)
#define SEQx_STATUS_FIFO_FILL_mask	0xf
#define SEQx_STATUS_FIFO_FILL_shift	0
#define gSEQx_STATUS_FIFO_FILL(val)	getBF(val,SEQx_STATUS_FIFO_FILL)

#define SEQx_VAL_TASKNR_mask		0xf
#define SEQx_VAL_TASKNR_shift		12
#define gSEQx_VAL_TASKNR(val)		getBF(val,SEQx_VAL_TASKNR)
#define SEQx_VAL_VAL_mask			0xfff
#define SEQx_VAL_VAL_shift			0
#define gSEQx_VAL_VAL(val)			getBF(val,SEQx_VAL_VAL)

struct priv_data {
	void *baseaddr;
	uint32_t vref; /* in mV */
	uint32_t task;
	uint32_t sequencer;
	struct mutex lock;
};

#define MAX_SEQ_TASKS 5

#define NETX4000_IIO_ADC_CHANNEL(_channel, _type, _name) {	\
	.type = _type,					\
	.channel = _channel,				\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), \
	.datasheet_name = _name,			\
	.indexed = 1,					\
	.address = _channel,					\
	.scan_index = _channel,					\
	.scan_type = { \
		.sign = 'u', \
		.realbits = 12, \
		.storagebits = 32, \
		.shift = 0, \
	}, \
}

static const struct iio_chan_spec netx4000_iio_adc_channels[] = {
	NETX4000_IIO_ADC_CHANNEL(0, IIO_VOLTAGE, "ain0"),
	NETX4000_IIO_ADC_CHANNEL(1, IIO_VOLTAGE, "ain1"),
	NETX4000_IIO_ADC_CHANNEL(2, IIO_VOLTAGE, "ain2"),
	NETX4000_IIO_ADC_CHANNEL(3, IIO_VOLTAGE, "ain3"),
	NETX4000_IIO_ADC_CHANNEL(4, IIO_VOLTAGE, "ain4"),
	NETX4000_IIO_ADC_CHANNEL(5, IIO_VOLTAGE, "ain5"),
	NETX4000_IIO_ADC_CHANNEL(6, IIO_VOLTAGE, "ain6"),
	NETX4000_IIO_ADC_CHANNEL(7, IIO_VOLTAGE, "ain7"),
	NETX4000_IIO_ADC_CHANNEL(8, IIO_VOLTAGE, "ain8"),
	IIO_CHAN_SOFT_TIMESTAMP(9),
};

static irqreturn_t netx4000_trigger_th_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct priv_data *pdata = iio_priv(indio_dev);
	uint32_t s;

	/* Start sequencers */
	/* Note: The seq_restart bit is necessary to clear the seq_finished bit in the status register. */
	for (s = 0; s < pdata->sequencer; s++)
		writel(SEQx_CTRL_FIRST_TASK(s * MAX_SEQ_TASKS) | SEQx_CTRL_RESTART(1) | SEQx_CTRL_ENABLE(1), pdata->baseaddr+SEQ_CTRL(s));

	pf->timestamp = iio_get_time_ns(pf->indio_dev);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t netx4000_trigger_bh_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct priv_data *pdata = iio_priv(indio_dev);
	uint32_t regval, buf[10+2], s, t; /* NOTE: As the timestamp must be 8 Byte aligned, a array of 10 instead of 9 is declared! */
	unsigned long timeout = jiffies+(1*HZ); /* TODO: timeout value */
	int rc = 0;

	/* Format buffer */
	memset(buf, 0, sizeof(buf));

	/* Read out sequencer FIFOs */
	for (s = 0, t = 0; s < pdata->sequencer; s++) {
		do {
			regval = readl(pdata->baseaddr+SEQ_STATUS(s));
			if (gSEQx_STATUS_OVFL(regval)) {
				dev_err(indio_dev->dev.parent, "Sequencer%d FIFO overflow occured!\n", s);
				rc = -EIO;
			}
			if (gSEQx_STATUS_URUN(regval)) {
				dev_err(indio_dev->dev.parent, "Sequencer%d FIFO underrun occured!\n", s);
				rc = -EIO;
			}
			if (time_after(jiffies,timeout)) {
				dev_err(indio_dev->dev.parent, "Sequencer%d (task%d) timed out\n", s, t);
				rc = -ETIMEDOUT;
			}
			if (gSEQx_STATUS_FIFO_FILL(regval)) {
				/* Read out the ADC value */
				buf[t++] = gSEQx_VAL_VAL(readl(pdata->baseaddr+SEQ_VAL(s)));
			}
			else if (gSEQx_STATUS_FINISHED(regval))
				break;
		} while (rc == 0);
	}

	/* Disable sequencers */
	/* Note: This will clear also the fifo_urun and fifo_ovfl bits in the status register. */
	for (s = 0; s < pdata->sequencer; s++)
		writel(SEQx_CTRL_ENABLE(0), pdata->baseaddr+SEQ_CTRL(s));

	if (rc)
		goto done;

	iio_push_to_buffers_with_timestamp(indio_dev, buf, pf->timestamp);

done:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static int netx4000_direct_scan(struct iio_dev *indio_dev, struct iio_chan_spec const *chan, int *val)
{
	struct priv_data *pdata = iio_priv(indio_dev);
	uint32_t regval, buf, s = 3, t = 14; /* Sequencer 3 and task 14 are used for single shot access */
	unsigned long timeout = jiffies+(1*HZ); /* TODO: timeout value */
	int rc = 0;

// 	rc = iio_device_claim_direct_mode(indio_dev);
// 	if (rc)
// 		return rc;
	mutex_lock(&pdata->lock);

	/* Initialize task(t) for single shot */
	writel(TASKx_ANALOG_SEL(chan->channel) | TASKx_NXT_TASK(15) | TASKx_START_COND(1), pdata->baseaddr+TASK(t));

	/* Start low prior sequencer(s) for task(t) */
	/* The seq_restart bit is necessary to clear the seq_finished bit in the status register. */
	writel(SEQx_CTRL_FIRST_TASK(t) | SEQx_CTRL_RESTART(1) | SEQx_CTRL_ENABLE(1), pdata->baseaddr+SEQ_CTRL(s));

	do {
		regval = readl(pdata->baseaddr+SEQ_STATUS(s));
		if (gSEQx_STATUS_OVFL(regval)) {
			dev_err(indio_dev->dev.parent, "FIFO overflow occured at channel-%d\n", chan->channel);
			rc = -EIO;
		}
		if (gSEQx_STATUS_URUN(regval)) {
			dev_err(indio_dev->dev.parent, "FIFO underrun occured at channel-%d\n", chan->channel);
			rc = -EIO;
		}
		if (time_after(jiffies,timeout)) {
			dev_err(indio_dev->dev.parent, "Channel-%d timed out\n", chan->channel);
			rc = -ETIMEDOUT;
		}
		if (gSEQx_STATUS_FINISHED(regval)) {
			/* Read out the ADC value */
			buf = gSEQx_VAL_VAL(readl(pdata->baseaddr+SEQ_VAL(s)));
			break;
		}
	} while (rc == 0);

	/* Disable sequencer */
	/* This will clear also the fifo_urun and fifo_ovfl bits in the status register. */
	writel(SEQx_CTRL_ENABLE(0), pdata->baseaddr+SEQ_CTRL(s));

// 	iio_device_release_direct_mode(indio_dev);
	mutex_unlock(&pdata->lock);

	if (!rc && val)
		*val = buf;

	return rc;
}

static int netx4000_iio_adc_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan, int *val, int *val2, long info)
{
	struct priv_data *pdata = iio_priv(indio_dev);
	int rc = 0;

	switch (info) {
		case IIO_CHAN_INFO_RAW:
			rc = netx4000_direct_scan(indio_dev, chan, val);
			if (rc)
				break;
			rc = IIO_VAL_INT;
			break;
		case IIO_CHAN_INFO_SCALE:
			*val = pdata->vref;
			*val2 = chan->scan_type.realbits;
			rc = IIO_VAL_FRACTIONAL_LOG2;
			break;
		default:
			rc = -EINVAL;
			break;
	}

	return rc;
}

static int add_channel(struct iio_dev *indio_dev, int channel)
{
	struct priv_data *pdata = iio_priv(indio_dev);
	uint32_t regval;

	if (pdata->task >= 14) { /* NOTE: task14 is used for single shot access (read raw mode) */
		dev_info(indio_dev->dev.parent, "max. task count exceeded!\n");
		return -EINVAL;
	}

	if (pdata->sequencer > 3) { /* NOTE: sequencer3 is used for single shot access (read raw mode) */
		dev_info(indio_dev->dev.parent, "max. sequencer count exceeded!\n");
		return -EINVAL;
	}

	/* Initialize new task */
	writel(TASKx_ANALOG_SEL(channel) | TASKx_NXT_TASK(15) | TASKx_START_COND(1), pdata->baseaddr+TASK(pdata->task));

	/* Link tasks */
	if (pdata->task % MAX_SEQ_TASKS) {
		regval = readl(pdata->baseaddr+TASK(pdata->task - 1));

		regval &= ~TASKx_NXT_TASK(-1);
		regval |= TASKx_NXT_TASK(pdata->task);
		writel(regval, pdata->baseaddr+TASK(pdata->task - 1));
	}

	pdata->sequencer = pdata->task / MAX_SEQ_TASKS + 1;
	pdata->task++;

	return 0;
}

static int netx4000_update_scan_mode(struct iio_dev *indio_dev, const unsigned long *active_scan_mask)
{
	struct priv_data *pdata = iio_priv(indio_dev);
	int i, rc = 0;

	pdata->task = 0;
	pdata->sequencer = 0;

	for (i = 0; i < indio_dev->masklength; i++) {
		if (!test_bit(i, active_scan_mask))
			continue;
		rc = add_channel(indio_dev, i);
		if (rc)
			break;
	}

	return 0;
}

static const struct iio_info netx4000_iio_adc_info = {
	.read_raw = &netx4000_iio_adc_read_raw,
	.update_scan_mode = &netx4000_update_scan_mode,
};

static int netx4000_iio_adc_chip_init(struct priv_data *pdata)
{
	writel(0x1, pdata->baseaddr+CONFIG); // reset the state machine
	writel(0x2, pdata->baseaddr+CONFIG); // power up
	usleep_range(2, 10); // > 1us

	return 0;
}

static int netx4000_iio_adc_chip_deinit(struct priv_data *pdata)
{
	writel(0x1, pdata->baseaddr+CONFIG); // reset the state machine

	return 0;
}

static int netx4000_iio_adc_probe(struct platform_device *pdev)
{
	struct iio_dev *indio_dev;
	struct priv_data *pdata;
	int rc;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*pdata)); /* automatically freed on driver detach */
	if (!indio_dev) {
		dev_err(&pdev->dev, "devm_iio_device_alloc() failed\n");
		return -ENOMEM;
	}

	pdata = iio_priv(indio_dev);

	/* Read the register base address from DT and map it */
	pdata->baseaddr = of_iomap(pdev->dev.of_node, 0);
	if (pdata->baseaddr == NULL) {
		dev_err(&pdev->dev, "of_iomap() failed\n");
		rc = -EIO;
		goto err_out;
	}

	/* Read 'vref' from DT */
	rc = of_property_read_u32(pdev->dev.of_node, "vref", &pdata->vref);
	if (rc) {
		pdata->vref = 3300;
		dev_info(&pdev->dev, "'vref' not provided by DT => falling back to default (vref=%d)\n", pdata->vref);
	}

	/* Check for access permissions */
	if (readl(pdata->baseaddr) == 0xdeafdeaf) {
		dev_err(&pdev->dev, "permission denied by internal firewall of SOC.\n");
		rc = -EIO;
		goto err_out;
	}

	platform_set_drvdata(pdev, indio_dev);

	indio_dev->name = pdev->name;
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &netx4000_iio_adc_info;
	indio_dev->modes = INDIO_DIRECT_MODE; // NOTE: see stm32-adc.c for INDIO_HARDWARE_TRIGGERED
	indio_dev->channels = netx4000_iio_adc_channels;
	indio_dev->num_channels = ARRAY_SIZE(netx4000_iio_adc_channels);

	rc = iio_triggered_buffer_setup(indio_dev, &netx4000_trigger_th_handler, &netx4000_trigger_bh_handler, NULL);
	if (rc) {
		dev_err(&pdev->dev, "iio_triggered_buffer_setup() failed\n");
		return rc;
	}

	mutex_init(&pdata->lock);

	/* Initialize the chip */
	netx4000_iio_adc_chip_init(pdata);

	rc = devm_iio_device_register(&pdev->dev, indio_dev); /* automatically unregistered on driver detach */
	if (rc) {
		dev_err(&pdev->dev, "devm_iio_device_register() failed\n");
		return rc;
	}

	dev_info(&pdev->dev, "successfully initialized!\n");

err_out:
	return rc;
}

static int netx4000_iio_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct priv_data *pdata = iio_priv(indio_dev);

	netx4000_iio_adc_chip_deinit(pdata);

	dev_info(&pdev->dev, "successfully removed!\n");

	return 0;
}



static const struct of_device_id netx4000_iio_adc_match[] = {
	{ .compatible = "hilscher,iio-adc-netx4000", },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, netx4000_iio_adc_match);

static struct platform_driver netx4000_iio_adc_driver = {
	.probe = netx4000_iio_adc_probe,
	.remove = netx4000_iio_adc_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = netx4000_iio_adc_match,
	},
};

static int __init netx4000_iio_adc_init(void)
{
	pr_info("%s: %s\n", DRIVER_NAME, DRIVER_DESC);
	return platform_driver_register(&netx4000_iio_adc_driver);
}
module_init(netx4000_iio_adc_init);

static void __exit netx4000_iio_adc_exit(void)
{
	platform_driver_unregister(&netx4000_iio_adc_driver);
}
module_exit(netx4000_iio_adc_exit);

MODULE_AUTHOR("Hilscher Gesellschaft fuer Systemautomation mbH");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");
