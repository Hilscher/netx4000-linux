/*
* RTC driver for Hilscher netX4000 based platforms
*
* drivers/rtc/rtc-netx4000.c
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


#define DRIVER_DESC "RTC driver for Hilscher netX4000 based platforms"
#define DRIVER_NAME "rtc-netx4000"


#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/io.h>
#include <linux/of_address.h>

#include <linux/bcd.h>


/* Regdef for the RTC */
#define RTCA0CTL0		0x00 // Control register 0
#define RTCA0CTL1		0x04 // Control register 1
#define RTCA0CTL2		0x08 // Control register 2
#define RTCA0SUBC		0x0c // Sub-count register
#define RTCA0SRBU		0x10 // Sub-count register read buffer
#define RTCA0SEC		0x14 // Second count buffer register
#define RTCA0MIN		0x18 // Minute count buffer register
#define RTCA0HOUR		0x1c // Hour count buffer register
#define RTCA0WEEK		0x20 // Day of the week count buffer register
#define RTCA0DAY		0x24 // Day count buffer register
#define RTCA0MONTH		0x28 // Month count buffer register
#define RTCA0YEAR		0x2c // Year count buffer register
#define RTCA0TIME		0x30 // Time count buffer register
#define RTCA0CAL		0x34 // Calendar count buffer register
#define RTCA0SUBU		0x38 // Clock error correction register
#define RTCA0SCMP		0x3c // Sub-counter compare register
#define RTCA0ALM		0x40 // Alarm minute setting register
#define RTCA0ALH		0x44 // Alarm hour setting register
#define RTCA0ALW		0x48 // Alarm day of the week setting register
#define RTCA0SECC		0x4c // Second count register
#define RTCA0MINC		0x50 // Minute count register
#define RTCA0HOURC		0x54 // Hour count register
#define RTCA0WEEKC		0x58 // Day of the week count register
#define RTCA0DAYC		0x5c // Day count register
#define RTCA0MONC		0x60 // Month count register
#define RTCA0YEARC		0x64 // Year count register
#define RTCA0TIMEC		0x68 // Time count register
#define RTCA0CALC		0x6c // Calendar count register
#define RTCA0TCR		0x70 // Test register
#define RTCA0EMU		0x74 // Emulation register


#define setBF(val,regpos)	((val & regpos##_mask) << regpos##_shift)
#define getBF(val,regpos)	(val >> regpos##_shift & regpos##_mask)


#define CTL0_CE_mask		0x1
#define CTL0_CE_shift		7
#define CTL0_CE(val)		setBF(val,CTL0_CE)
#define CTL0_CEST_mask		0x1
#define CTL0_CEST_shift		6
#define CTL0_CEST(val)		setBF(val,CTL0_CEST)
#define CTL0_AMPM_mask		0x1
#define CTL0_AMPM_shift		5
#define CTL0_AMPM(val)		setBF(val,CTL0_AMPM)
#define CTL0_SLSB_mask		0x1
#define CTL0_SLSB_shift		4
#define CTL0_SLSB(val)		setBF(val,CTL0_SLSB)

#define CTL2_WST_mask		0x1
#define CTL2_WST_shift		1
#define CTL2_WST(val)		setBF(val,CTL2_WST)
#define CTL2_WAIT_mask		0x1
#define CTL2_WAIT_shift		0
#define CTL2_WAIT(val)		setBF(val,CTL2_WAIT)

#define TIME_HOUR_mask		0xff
#define TIME_HOUR_shift		16
#define TIME_HOUR(val)		setBF(val,TIME_HOUR)
#define TIME_MIN_mask		0xff
#define TIME_MIN_shift		8
#define TIME_MIN(val)		setBF(val,TIME_MIN)
#define TIME_SEC_mask		0xff
#define TIME_SEC_shift		0
#define TIME_SEC(val)		setBF(val,TIME_SEC)

#define CAL_YEAR_mask		0xff
#define CAL_YEAR_shift		24
#define CAL_YEAR(val)		setBF(val,CAL_YEAR)
#define CAL_MONTH_mask		0xff
#define CAL_MONTH_shift		16
#define CAL_MONTH(val)		setBF(val,CAL_MONTH)
#define CAL_DAY_mask		0xff
#define CAL_DAY_shift		8
#define CAL_DAY(val)		setBF(val,CAL_DAY)
#define CAL_WEEK_mask		0xff
#define CAL_WEEK_shift		0
#define CAL_WEEK(val)		setBF(val,CAL_WEEK)

struct priv_data {
	void *ba; /* baseaddr */
	struct rtc_device *rtc;
};

static int netx4000_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct priv_data *pdata = dev_get_platdata(dev);
	uint32_t tmp_time, time, date;
	unsigned long timeout;

	timeout = jiffies+(1*HZ);
	while(readl(pdata->ba+RTCA0CTL2) & (CTL2_WST(1) | CTL2_WAIT(1))) {
		if (time_after(jiffies,timeout)) {
			dev_err(dev, "Reading RTC timed out.\n");
			return -ETIMEDOUT;
		};
	};

	tmp_time = readl(pdata->ba+RTCA0TIMEC);
	date = readl(pdata->ba+RTCA0CALC);
	time = readl(pdata->ba+RTCA0TIMEC);
	if (time < tmp_time)
		/* Reload the date counter in case of a day wrap */
		date = readl(pdata->ba+RTCA0CALC);

	tm->tm_sec = bcd2bin(getBF(time, TIME_SEC));
	tm->tm_min = bcd2bin(getBF(time, TIME_MIN));
	tm->tm_hour = bcd2bin(getBF(time, TIME_HOUR));
	tm->tm_mday = bcd2bin(getBF(date, CAL_DAY));
	tm->tm_mon = bcd2bin(getBF(date, CAL_MONTH))-1; /* Jan=0, ... */
	tm->tm_year = bcd2bin(getBF(date, CAL_YEAR))+100; 

	dev_dbg(dev, "%4d-%02d-%02d %02d:%02d:%02d\n",
		1900 + tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	return rtc_valid_tm(tm);
}

static int netx4000_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct priv_data *pdata = dev_get_platdata(dev);
	uint32_t time, date;
	unsigned long timeout;
	
	dev_dbg(dev, "%4d-%02d-%02d %02d:%02d:%02d\n",
		1900 + tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	/* Stop counter-up operation */
	timeout = jiffies+(1*HZ);
	while(readl(pdata->ba+RTCA0CTL2) & (CTL2_WST(1) | CTL2_WAIT(1))) {
		if (time_after(jiffies,timeout)) {
			dev_err(dev, "Writing RTC timed out.\n");
			return -ETIMEDOUT;
		};
	};
	
	writel(CTL2_WAIT(1), pdata->ba+RTCA0CTL2);

	timeout = jiffies+(1*HZ);
	while((readl(pdata->ba+RTCA0CTL2) & CTL2_WST(1)) == 0) {
		if (time_after(jiffies,timeout)) {
			dev_err(dev, "Writing RTC timed out.\n");
			return -ETIMEDOUT;
		};
	};	

	/* Store new time/date values */
	time = TIME_HOUR(bin2bcd(tm->tm_hour)) | TIME_MIN(bin2bcd(tm->tm_min)) | TIME_SEC(bin2bcd(tm->tm_sec));
	date = CAL_YEAR(bin2bcd(tm->tm_year-100)) | CAL_MONTH(bin2bcd(tm->tm_mon+1)) | CAL_DAY(bin2bcd(tm->tm_mday));

	writel(time, pdata->ba+RTCA0TIME);
	writel(date, pdata->ba+RTCA0CAL);


	/* Start counter-up operation */
	writel(CTL2_WAIT(0), pdata->ba+RTCA0CTL2);
		
	return 0;
}

static int netx4000_rtc_chip_init(struct device *dev)
{
	struct priv_data *pdata = dev_get_platdata(dev);
	uint32_t val;

	/* Enable counter-up operation */
	writel(CTL2_WAIT(0), pdata->ba+RTCA0CTL2);
	
	val = readl(pdata->ba+RTCA0CTL0);
	if (val & CTL0_CE(1)) {
		dev_info(dev, "chip is already initialized.\n");
		return 0; /* RTC is already initialized */
	}

	dev_info(dev, "Initializing the RTC.\n");

	/* Enable counter-up clock and 24 hour mode */ 
	writel(CTL0_CE(1) | CTL0_AMPM(1), pdata->ba+RTCA0CTL0);
	
	return 0;
}

static int netx4000_rtc_chip_deinit(struct device *dev)
{
#if (0)
	/* Because the RTC should already run, we disable this code segment. */
	struct priv_data *pdata = dev_get_platdata(dev);

	writel(CTL0_CE(0), pdata->ba+RTCA0CTL0); /* disable counter-up clock */ 
#endif
	return 0;
}

static const struct rtc_class_ops netx4000_rtc_ops = {
	.read_time = netx4000_rtc_read_time,
	.set_time = netx4000_rtc_set_time,
};

static int netx4000_rtc_probe(struct platform_device *pdev)
{
	struct priv_data *pdata;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "failed allocating memory\n");
		return -ENOMEM;
	}

	/* Read the register base address from DT and map it */
	pdata->ba = of_iomap(pdev->dev.of_node, 0);
	if (pdata->ba == NULL) {
		dev_err(&pdev->dev, "of_iomap() failed\n");
		return -EIO;
	}

	pdev->dev.platform_data = pdata;
	platform_set_drvdata(pdev, pdata);

	/* Initialize the chip */
	netx4000_rtc_chip_init(&pdev->dev);

	/* Register a RTC device */
	pdata->rtc = devm_rtc_device_register(&pdev->dev, DRIVER_NAME, &netx4000_rtc_ops, THIS_MODULE);
	if (IS_ERR(pdata->rtc)) {
		dev_err(&pdev->dev, "could not register RTC\n");	
		return PTR_ERR(pdata->rtc);
	}
	
	dev_info(&pdev->dev, "%s successfully initialized!\n", dev_name(&pdata->rtc->dev));

	return 0;
}

static int netx4000_rtc_remove(struct platform_device *pdev)
{
	struct priv_data *pdata = platform_get_drvdata(pdev);
	
	netx4000_rtc_chip_deinit(&pdev->dev);

	dev_info(&pdev->dev, "%s successfully removed!\n", dev_name(&pdata->rtc->dev));

	return 0;
}



static const struct of_device_id netx4000_rtc_match[] = {
	{ .compatible = "hilscher,rtc-netx4000", },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, netx4000_rtc_match);

static struct platform_driver netx4000_rtc_driver = {
	.probe = netx4000_rtc_probe,
	.remove = netx4000_rtc_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = netx4000_rtc_match,
	},
};

static int __init netx4000_rtc_init(void)
{
	pr_info("%s: %s\n", DRIVER_NAME, DRIVER_DESC);
	return platform_driver_register(&netx4000_rtc_driver);
}
module_init(netx4000_rtc_init);

static void __exit netx4000_rtc_exit(void)
{
	platform_driver_unregister(&netx4000_rtc_driver);
}
module_exit(netx4000_rtc_exit);

MODULE_AUTHOR("Hilscher Gesellschaft fuer Systemautomation mbH");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");

