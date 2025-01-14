/*
* Framebuffer driver for Hilscher netx4000 based platforms
*
* drivers/video/fbdev/fb-netx4000.c
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

#define DRIVER_DESC  "Framebuffer driver for Hilscher netx4000 based platforms"
#define DRIVER_NAME  "fb-netx4000"

//#define TEST_PICTURE  1

#define setBF(val,regpos)  (((val) & regpos##_mask) << regpos##_shift)
#define getBF(val,regpos)  (((val) >> regpos##_shift) & regpos##_mask)

/* Regdef */
#define RAP_SYSCTRL_PIXCLKCFG                  (void __iomem*)0xf8000020
#define ExtClock_mask                            0x1
#define ExtClock_shift                           8
#define sExtClock(val)                           setBF(val,ExtClock)
#define Divider_mask                             0xff
#define Divider_shift                            0
#define sDivider(val)                            setBF(val,Divider)

#define HW_Version_Register                    0x00
#define Layer_Count_Register                   0x04

#define Sync_Size_Register                     0x08
#define VSync_mask                               0x3ff
#define VSync_shift                              0
#define sVSync(val)                              setBF(val,VSync)
#define HSync_mask                               0x3ff
#define HSync_shift                              16
#define sHSync(val)                              setBF(val,HSync)

#define Back_Porch_Register                    0x0c
#define VBackPorch_mask                          0x3ff
#define VBackPorch_shift                         0
#define sVBackPorch(val)                         setBF(val,VBackPorch)
#define HBackPorch_mask                          0x3ff
#define HBackPorch_shift                         16
#define sHBackPorch(val)                         setBF(val,HBackPorch)

#define Active_Width_Register                  0x10
#define Height_mask                              0x3ff
#define Height_shift                             0
#define sHeight(val)                             setBF(val,Height)
#define Width_mask                               0x3ff
#define Width_shift                              16
#define sWidth(val)                              setBF(val,Width)

#define Total_Width_Register                   0x14
#define TotalHeight_mask                         0x3ff
#define TotalHeight_shift                        0
#define sTotalHeight(val)                        setBF(val,TotalHeight)
#define TotalWidth_mask                          0x3ff
#define TotalWidth_shift                         16
#define sTotalWidth(val)                         setBF(val,TotalWidth)

#define Global_Control_Register                0x18
#define HSyncPolarity_mask                       0x1
#define HSyncPolarity_shift                      31
#define sHSyncPolarity(val)                      setBF(val,HSyncPolarity)
#define VSyncPolarity_mask                       0x1
#define VSyncPolarity_shift                      30
#define sVSyncPolarity(val)                      setBF(val,VSyncPolarity)
#define BlankPolarity_mask                       0x1
#define BlankPolarity_shift                      29
#define sBlankPolarity(val)                      setBF(val,BlankPolarity)
#define SlaveTimingModeOn_mask                   0x1
#define SlaveTimingModeOn_shift                  18
#define sSlaveTimingModeOn(val)                  setBF(val,SlaveTimingModeOn)
#define DitheringOn_mask                         0x1
#define DitheringOn_shift                        16
#define sDitheringOn(val)                        setBF(val,DitheringOn)
#define GlobalEnable_mask                        0x1
#define GlobalEnable_shift                       0
#define sGlobalEnable(val)                       setBF(val,GlobalEnable)

#define Global_Configuration_1_Register        0x1c
#define Global_Configuration_2_Register        0x20

#define Shadow_Reload_Control_Register         0x24
#define VerticalBlankingReload_mask              0x1
#define VerticalBlankingReload_shift             1
#define sVerticalBlankingReload(val)             setBF(val,VerticalBlankingReload)
#define ImmediateReload_mask                     0x1
#define ImmediateReload_shift                    0
#define sImmediateReload(val)                    setBF(val,ImmediateReload)

#define Gamma_Correction_Register              0x28 /* reserved */
#define Background_Color_Register              0x2c
#define IRQ_Polarity_Register                  0x30 /* reserved */
#define IRQ_Enable_Register                    0x34
#define IRQ_Status_Register                    0x38
#define IRQ_Clear_Register                     0x3c
#define Line_IRQ_Position_Control_Register     0x40
#define Position_Status_Register               0x44
#define Sync_Blank_Status_Register             0x48
#define Background_Layer_base_Register         0x4c /* reserved */
#define Background_Layer_Increments_Register   0x50 /* reserved */
#define Background_Layer_RAM_Address_Register  0x54 /* reserved */
#define Background_Layer_Data_Register         0x58 /* reserved */
#define Slave_Timing_Mode_Status_Register      0x5c
#define External_Display_Control_Register      0x60 /* reserved */
#define Layer_Configuration_Register           0x80

#define Layer_Control_Register                 0x84
#define ColorKeyReplaceOn_mask                   0x1
#define ColorKeyReplaceOn_shift                  5
#define sColorKeyReplaceOn(val)                  setBF(val,ColorKeyReplaceOn)
#define ClutLookupOn_mask                        0x1
#define ClutLookupOn_shift                       4
#define sClutLookupOn(val)                       setBF(val,ClutLookupOn)
#define ColorKeyFeatureOn_mask                   0x1
#define ColorKeyFeatureOn_shift                  1
#define sColorKeyFeatureOn(val)                  setBF(val,ColorKeyFeatureOn)
#define LayerOn_mask                             0x1
#define LayerOn_shift                            0
#define sLayerOn(val)                            setBF(val,LayerOn)

#define Window_Horizontal_Position_Register    0x88
#define HStop_mask                               0x3ff
#define HStop_shift                              16
#define sHStop(val)                              setBF(val,HStop)
#define HStart_mask                              0x3ff
#define HStart_shift                             0
#define sHStart(val)                             setBF(val,HStart)

#define Window_Vertical_Position_Register      0x8c
#define VStop_mask                               0x3ff
#define VStop_shift                              16
#define sVStop(val)                              setBF(val,VStop)
#define VStart_mask                              0x3ff
#define VStart_shift                             0
#define sVStart(val)                             setBF(val,VStart)

#define Color_Key_Register                     0x90

#define Pixel_Format_Register                  0x94
#define PixelFormat_mask                         0x3
#define PixelFormat_shift                        0
#define sPixelFormat(val)                        setBF(val,PixelFormat)

#define Constant_Alpha_Register                0x98
#define Default_Color_Register                 0x9c
#define Blending_Factors_Register              0xa0
#define Alpha_FB_Configuration_Register        0xa4 /* reserved */
#define Alpha_FB_Control_Register              0xa8 /* reserved */
#define Color_FB_Address_Register              0xac

#define Color_FB_Length_Register               0xb0
#define PitchOfColorFB_mask                      0x1fff
#define PitchOfColorFB_shift                     16
#define sPitchOfColorFB(val)                     setBF(val,PitchOfColorFB)
#define LineLengthOfColorFB_mask                 0x1fff
#define LineLengthOfColorFB_shift                0
#define sLineLengthOfColorFB(val)                setBF(val,LineLengthOfColorFB)

#define FB_Lines_Register                      0xb4
#define NumberOfLines_mask                       0x3ff
#define NumberOfLines_shift                      0
#define sNumberOfLines(val)                      setBF(val,NumberOfLines)

#define Alpha_FB_Address_Register              0xb8 /* reserved */
#define Alpha_FB_Length_Register               0xbc /* reserved */
#define Layer_Reserved_Register                0xc0 /* reserved */
#define CLUT_Write_Access_Register             0xc4


#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>

#include <linux/fb.h>

#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

#include <linux/clk.h>
#include <linux/dma-mapping.h>

#include <mach/hardware.h>

/* TODO: Move it to dt-bindings */
#ifndef _DT_BINDINGS_FB_NETX4000_H
#define _DT_BINDINGS_FB_NETX4000_H

/* Pixel Mode */
#define ARGB8888  0
#define RGBA8888  1
#define RGB565    2
#define AL88      3  /* currently not supported */
#define AL44      4  /* currently not supported */
#define AL8       5  /* currently not supported */
#define L8        6  /* currently not supported */

#endif /* _DT_BINDINGS_FB_NETX4000_H */


#define PALETTE_ENTRIES_NO  16

struct priv_data {
	struct device *dev;
	void *regbase;
	struct clk *clk;
	uint32_t clk_div;
	uint32_t pixel_format;
	uint32_t msb_right;

	uint32_t fb_size;
	void *vaFB;       /* virt address of frame buffer */
	dma_addr_t paFB;  /* phys address of frame buffer */

	struct fb_videomode videomode;
	uint32_t pseudo_palette[PALETTE_ENTRIES_NO];
};

static int netx4000_fb_set_par(struct fb_info *fbinfo)
{
	struct fb_var_screeninfo *var = &fbinfo->var;
	struct priv_data *priv = fbinfo->par;
	uint32_t clk_rate_khz, x=0, y=0;

	/* Configure the pixclock */
	clk_rate_khz =  clk_get_rate(priv->clk) / 1000;
	priv->clk_div = (fbinfo->var.pixclock / KHZ2PICOS(clk_rate_khz)) + !!(fbinfo->var.pixclock % KHZ2PICOS(clk_rate_khz));
	iowrite32(sDivider(priv->clk_div), RAP_SYSCTRL_PIXCLKCFG);

	/* Sync_Size_Register */
	x += var->hsync_len;
	y += var->vsync_len;
	iowrite32(sHSync(x-1) | sVSync(y-1), priv->regbase + Sync_Size_Register);

	/* Back_Porch_Register */
	x += var->left_margin;
	y += var->upper_margin;
	iowrite32(sHBackPorch(x-1) | sVBackPorch(y-1), priv->regbase + Back_Porch_Register);

	/* Window_Horizontal_Position_Register */
	iowrite32(sHStart(x) | sHStop(x+var->xres-1), priv->regbase + Window_Horizontal_Position_Register);

	/* Window_Vertcal_Position_Register */
	iowrite32(sVStart(y) | sVStop(y+var->yres-1), priv->regbase + Window_Vertical_Position_Register);

	/* Active_Width_Register */
	x += var->xres;
	y += var->yres;
	iowrite32(sWidth(x-1) | sHeight(y-1), priv->regbase + Active_Width_Register);

	/* Total_Width_Register */
	x += var->right_margin;
	y += var->lower_margin;
	iowrite32(sTotalWidth(x-1) | sTotalHeight(y-1), priv->regbase + Total_Width_Register);

	/* Color_FB_Address_Register */
	iowrite32(priv->paFB, priv->regbase + Color_FB_Address_Register);

	/* Color_FB_Length_Register */
	iowrite32(sPitchOfColorFB(var->xres * var->bits_per_pixel / 8) | sLineLengthOfColorFB((var->xres * var->bits_per_pixel / 8) + 7), priv->regbase + Color_FB_Length_Register);

	/* FB Lines_Register */
	iowrite32(sNumberOfLines(var->yres), priv->regbase + FB_Lines_Register);

	/* Pixel_Format_Register */
	iowrite32(sPixelFormat(priv->pixel_format), priv->regbase + Pixel_Format_Register);

	/* Shadow_Reload_Control_Register */
	iowrite32(sVerticalBlankingReload(1), priv->regbase + Shadow_Reload_Control_Register);

	/* Global_Control_Register */
	iowrite32(sHSyncPolarity(var->sync & FB_SYNC_HOR_HIGH_ACT) | sVSyncPolarity(var->sync & FB_SYNC_VERT_HIGH_ACT) | sBlankPolarity(0) |
		sSlaveTimingModeOn(0) | sDitheringOn(0) | sGlobalEnable(1), priv->regbase + Global_Control_Register);

	/* Layer_Control_Register */
	iowrite32(sLayerOn(1), priv->regbase + Layer_Control_Register);

	fbinfo->fix.line_length = var->xres_virtual * var->bits_per_pixel / 8;

	return 0;
}

static uint32_t chan_to_bf(uint32_t chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;

	if (bf->msb_right) {
		uint32_t n, swap_chan = 0;

		for (n = 0; n < bf->length; n++)
			swap_chan |= (chan & (1<<n)) ? (1<<(bf->length-1-n)) : 0;
		chan = swap_chan; 
	}

	return chan << bf->offset;
}

static int netx4000_fb_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue, unsigned transp, struct fb_info *fbinfo)
{
	struct priv_data *priv = fbinfo->par;
	uint32_t *palette = fbinfo->pseudo_palette;
	uint32_t val32;

	dev_dbg(priv->dev, "%s: regno=%d, red=0x%x, green=0x%x, blue=0x%x, transp=0x%x\n", __func__, regno, red, green, blue, transp);

	if (fbinfo->var.grayscale) {
		/* Convert color to grayscale.
		 * grayscale = 0.30*R + 0.59*G + 0.11*B */
		red = green = blue = (red * 77 + green * 151 + blue * 28 + 127) >> 8;
	}

	switch (fbinfo->fix.visual) {
		case FB_VISUAL_TRUECOLOR:
			if (regno < PALETTE_ENTRIES_NO) {
				val32  = chan_to_bf(red, &fbinfo->var.red);
				val32 |= chan_to_bf(green, &fbinfo->var.green);
				val32 |= chan_to_bf(blue, &fbinfo->var.blue);
				palette[regno] = val32;
			}
			else
				return -EINVAL;
			break;
		default:
			break;
	}

	return 0;
}

static struct fb_ops netx4000_fb_ops =
{
	.owner        = THIS_MODULE,

//	.fb_set_par   = netx4000_fb_set_par,
	.fb_setcolreg = netx4000_fb_setcolreg,

	.fb_fillrect  = sys_fillrect,
	.fb_copyarea  = sys_copyarea,
	.fb_imageblit = sys_imageblit,
};

/* ------------------------------------------------------------------------- */

static int netx4000_fb_probe (struct platform_device *pdev)
{
	struct fb_info *fbinfo;
	struct priv_data *priv;
	int rc;

	fbinfo = framebuffer_alloc(sizeof(*priv), &pdev->dev);
	if (!fbinfo) {
		dev_err(&pdev->dev, "framebuffer_alloc() failed\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, fbinfo);

	priv = fbinfo->par;
	priv->dev = &pdev->dev;

	/* Read the register base address from DT and map it */
	priv->regbase = of_iomap(pdev->dev.of_node, 0);
	if (priv->regbase == NULL) {
		dev_err(&pdev->dev, "of_iomap() failed\n");
		return -EIO;
	}

	/* Read the clock source from DT */
	priv->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->clk)) {
		rc = PTR_ERR(priv->clk);
		goto err_out;
	}

	/* Enable clocks */
	rc = clk_prepare_enable(priv->clk);
	if (rc) {
		dev_err(&pdev->dev, "Enabling clock failed\n");
		return rc;
	}

	/* Read the display settings from DT */
	rc = of_get_fb_videomode(pdev->dev.of_node, &priv->videomode, OF_USE_NATIVE_MODE);
	if (rc) {
		dev_err(priv->dev, "of_get_fb_videomode() failed\n");
		goto err_out;
	}
	fb_videomode_to_var(&fbinfo->var, &priv->videomode);

	/* Read the pixel mode from DT */
	of_property_read_u32(pdev->dev.of_node, "pixel-format", &priv->pixel_format);
	switch (priv->pixel_format) {
		case ARGB8888:
			fbinfo->var.red.offset = 16;
			fbinfo->var.red.length = 8;
			fbinfo->var.green.offset = 8;
			fbinfo->var.green.length = 8;
			fbinfo->var.blue.offset = 0;
			fbinfo->var.blue.length = 8;
			fbinfo->var.transp.offset = 24;
			fbinfo->var.transp.length = 8;
			fbinfo->var.bits_per_pixel = 32;
			break;
		case RGBA8888:
			fbinfo->var.red.offset = 24;
			fbinfo->var.red.length = 8;
			fbinfo->var.green.offset = 16;
			fbinfo->var.green.length = 8;
			fbinfo->var.blue.offset = 8;
			fbinfo->var.blue.length = 8;
			fbinfo->var.transp.offset = 0;
			fbinfo->var.transp.length = 8;
			fbinfo->var.bits_per_pixel = 32;
			break;
		case RGB565:
			fbinfo->var.red.offset = 11;
			fbinfo->var.red.length = 5;
			fbinfo->var.green.offset = 5;
			fbinfo->var.green.length = 6;
			fbinfo->var.blue.offset = 0;
			fbinfo->var.blue.length = 5;
			fbinfo->var.transp.offset = 0;
			fbinfo->var.transp.length = 0;
			fbinfo->var.bits_per_pixel = 16;
			break;
		case AL88:
		case AL44:
		case AL8:
		case L8:
			dev_err(priv->dev, "Unsupported pixel-format in DT (%d)\n", priv->pixel_format);
			rc =EINVAL;
			goto err_out;
		default:
			dev_err(priv->dev, "Invalid pixel-format in DT (%d)\n", priv->pixel_format);
			rc =EINVAL;
			goto err_out;
	}

	/* Read the msb_right flag from DT */
	if (of_property_read_bool(pdev->dev.of_node, "msb_right")) {
		fbinfo->var.red.msb_right = 1;
		fbinfo->var.green.msb_right = 1;
		fbinfo->var.blue.msb_right = 1;
		fbinfo->var.transp.msb_right = 1;
	}
	
	/* Allocate a zero initialized video buffer (black screen) */
	priv->fb_size = fbinfo->var.xres_virtual * fbinfo->var.yres_virtual * fbinfo->var.bits_per_pixel / 8;
	priv->vaFB = dma_alloc_coherent(priv->dev, PAGE_ALIGN(priv->fb_size), &priv->paFB, GFP_KERNEL);
	if (!priv->vaFB) {
		dev_err(priv->dev, "dma_alloc_coherent() failed\n");
		goto err_out;
	}

	/* XXX */
	fbinfo->fbops = &netx4000_fb_ops;
	fbinfo->mode = &priv->videomode;
	fbinfo->screen_base = priv->vaFB;
	fbinfo->screen_size = priv->fb_size;
	fbinfo->pseudo_palette = priv->pseudo_palette;

	/* XXX */
	strncpy(fbinfo->fix.id, DRIVER_NAME, sizeof(fbinfo->fix.id));
	fbinfo->fix.type = FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.visual = FB_VISUAL_TRUECOLOR;
	fbinfo->fix.accel = FB_ACCEL_NONE;
	fbinfo->fix.smem_start = priv->paFB;
	fbinfo->fix.smem_len = priv->fb_size;

	netx4000_fb_set_par(fbinfo);

#ifdef TEST_PICTURE
	{
		/* Draw a white outer frame and three colored dots in the middle of the screen. */
		struct fb_fillrect rec1 = {0,0,fbinfo->var.xres-1,fbinfo->var.yres-1,15,0};
		struct fb_fillrect rec2 = {1,1,fbinfo->var.xres-3,fbinfo->var.yres-3,0,0};
		struct fb_fillrect rec3 = {fbinfo->var.xres/2-10,fbinfo->var.yres/2,1,1,1,0};
		struct fb_fillrect rec4 = {fbinfo->var.xres/2,fbinfo->var.yres/2,1,1,2,0};
		struct fb_fillrect rec5 = {fbinfo->var.xres/2+10,fbinfo->var.yres/2,1,1,3,0};

		netx4000_fb_setcolreg(0, 0, 0, 0, 0, fbinfo); /* black */
		netx4000_fb_setcolreg(1, 0xffff, 0, 0, 0, fbinfo); /* red */
		netx4000_fb_setcolreg(2, 0, 0xffff, 0, 0, fbinfo); /* green */
		netx4000_fb_setcolreg(3, 0, 0, 0xffff, 0, fbinfo); /* blue */
		netx4000_fb_setcolreg(15, 0xffff, 0xffff, 0xffff, 0, fbinfo); /* white */

		fbinfo->fbops->fb_fillrect(fbinfo, &rec1);
		fbinfo->fbops->fb_fillrect(fbinfo, &rec2);
		fbinfo->fbops->fb_fillrect(fbinfo, &rec3);
		fbinfo->fbops->fb_fillrect(fbinfo, &rec4);
		fbinfo->fbops->fb_fillrect(fbinfo, &rec5);
	}
#endif

	rc = register_framebuffer(fbinfo);
	if (rc < 0) {
		dev_err(priv->dev, "register_framebuffer() failed\n");
		goto err_out;
	}

	dev_info(priv->dev, "mode: %ux%u@%uHz (%ukHz) %u %u %u %u %u %u %u %u %u, memory: %uKiB\n",
		priv->videomode.xres, priv->videomode.yres,priv->videomode.refresh, 1000000000/priv->videomode.pixclock, priv->videomode.left_margin,
		priv->videomode.right_margin, priv->videomode.upper_margin, priv->videomode.lower_margin,
		priv->videomode.hsync_len, priv->videomode.vsync_len, priv->videomode.sync, priv->videomode.vmode, priv->videomode.flag,
		priv->fb_size/1024);

	dev_info(priv->dev, "successfully initialized!\n");

	return 0;

err_out:
	framebuffer_release(fbinfo);

	return rc;
}

static int netx4000_fb_remove(struct platform_device *pdev)
{
	struct fb_info *fbinfo = platform_get_drvdata(pdev);
	struct priv_data *priv = fbinfo->par;

	unregister_framebuffer(fbinfo);
	framebuffer_release(fbinfo);

	clk_disable_unprepare(priv->clk);

	dev_info(&pdev->dev, "successfully removed!\n");

	return 0;
}

/* ------------------------------------------------------------------------- */

static struct of_device_id netx4000_fb_match[] = {
	{ .compatible = "hilscher,fb-netx4000", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, netx4000_fb_match);

static struct platform_driver netx4000_fb_driver = {
	.probe  = netx4000_fb_probe,
	.remove = netx4000_fb_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = netx4000_fb_match,
	}
};

static int __init netx4000_fb_init(void)
{
	pr_info("%s: %s\n", DRIVER_NAME, DRIVER_DESC);
	return platform_driver_register(&netx4000_fb_driver);
}
module_init(netx4000_fb_init);

static void __exit netx4000_fb_exit(void)
{
	platform_driver_unregister(&netx4000_fb_driver);
}
module_exit(netx4000_fb_exit);

MODULE_AUTHOR("Hilscher Gesellschaft fuer Systemautomation mbH");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");

