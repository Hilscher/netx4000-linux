/*
* USB UDC driver for Hilscher netX4000 based platforms
*
* drivers/usb/gadget/udc/udc-netx4000.c
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
* GNU General Public License for more details.test
*
*/

#define DRIVER_DESC "USB UDC driver for Hilscher netX4000 based platforms"
#define DRIVER_NAME "udc-netx4000"

#include <linux/usb.h>
#include <linux/usb/gadget.h>

#include <linux/module.h>
#include <linux/platform_device.h>

#include <linux/interrupt.h>
#include <linux/clk.h>

#include <linux/io.h>
#include <linux/of_address.h>

#include <mach/hardware.h>

#define ioset32(setmask, addr)	{uint32_t val32; val32 = ioread32(addr); iowrite32(val32 | setmask, addr);}
#define ioclear32(clearmask, addr)	{uint32_t val32; val32 = ioread32(addr); iowrite32(val32 & ~clearmask, addr);}

#define setBF(val,regpos)	((val & regpos##_mask) << regpos##_shift)
#define getBF(val,regpos)	(val >> regpos##_shift & regpos##_mask)

/* Regdef of UDC */

/* USB Register */

#define USB_CONTROL(ba)		(ba+0x0)
#define USBTESTMODE_mask	0x7
#define USBTESTMODE_shift	16
#define RSUM_IN				(1<<7)
#define SUSPEND				(1<<6)
#define CONF				(1<<5)
#define CONNECTB			(1<<3)
#define PUE2				(1<<2)

#define USB_STATUS(ba)		(ba+0x4)
#define SPEED_MODE_HS		(1<<6)

#define ADDR_FRAME(ba)		(ba+0x8)
#define USB_ADDR_mask		0x7f
#define USB_ADDR_shift		16
#define FRAME_mask			0x7ff
#define FRAME_shift			0

#define TEST(ba)			(ba+0x10)
#define CS_TestModeEn		(1<<1)

#define DATA0(ba)			(ba+0x18)
#define DATA1(ba)			(ba+0x1c)
#define STATUS(ba)			(ba+0x20)

#define INT_ENA(ba)			(ba+0x24)
#define EPn_INT(n)			(1<<(8+n))
#define SPEED_MODE_INT		(1<<6)
#define SOF_ERROR_INT		(1<<5)
#define SOF_INT				(1<<4)
#define USB_RST_INT			(1<<3)
#define SPND_INT			(1<<2)
#define RESUM_INT			(1<<1)

/* EPn Register */

#define EPn_CONTROL(ba,n)	((n<0 || n>15) ? ((void*)-1) : ((n==0) ? (ba+0x28) : (ba+0x20+n*0x20)))
#define EP0_DEND			(1<<7)
#define EP0_INAK_EN			(1<<4)
#define EP0_STL				(1<<2)
#define EP0_INAK			(1<<1)
#define EP0_ONAK			(1<<0)
#define EPn_EN				(1<<31)
#define EPn_DIR0_mask		0x1
#define EPn_DIR0_shift		26
#define IN					0
#define OUT					1
#define EPn_DIR0(val)		setBF(val, EPn_DIR0)
#define EPn_MODE_mask		0x3
#define EPn_MODE_shift		24
#define BULK				0
#define INTERRUPT			1
#define ISOCHRONOUS			2
#define EPn_MODE(val)		setBF(val, EPn_MODE)
#define EPn_CBCLR			(1<<8)
#define EPn_DEND			(1<<7)
#define EPn_DW_mask			0x3
#define EPn_DW_shift		5
#define EPn_DW(val)			setBF(val, EPn_DW)

#define EPn_OSTL_EN			(1<<4)
#define EPn_ISTL			(1<<3)
#define EPn_OSTL			(1<<2)
#define EPn_ONAK			(1<<0)

#define EPn_STATUS(ba,n)	((n<0 || n>15) ? ((void*)-1) : ((n==0) ? (ba+0x2c) : (ba+0x24+n*0x20)))
#define EPn_INT_ENA(ba,n)	((n<0 || n>15) ? ((void*)-1) : ((n==0) ? (ba+0x30) : (ba+0x28+n*0x20)))
#define EP0_OUT_INT			(1<<5)
#define EP0_IN_INT			(1<<4)
#define EP0_STG_END_INT		(1<<2)
#define EP0_SETUP_INT		(1<<0)
#define EPn_OUT_INT			(1<<19)
#define EPn_OUT_NULL_INT	(1<<18)
#define EPn_IN_INT			(1<<3)

#define EPn_DMA_CTRL(ba,n)	((n<1 || n>15) ? ((void*)-1) : (ba+0x2c+n*0x20))
#define EPn_PCKT_ADRS(ba,n)	((n<1 || n>15) ? ((void*)-1) : (ba+0x30+n*0x20))
#define EPn_BASEAD_mask		0x1fff
#define EPn_BASEAD_shift	16
#define EPn_BASEAD(val)		setBF(val, EPn_BASEAD)
#define EPn_MPKT_mask		0x7ff
#define EPn_MPKT_shift		0
#define EPn_MPKT(val)		setBF(val, EPn_MPKT)

#define EPn_LEN_DCNT(ba,n)	((n<0 || n>15) ? ((void*)-1) : ((n==0) ? (ba+0x34) : (ba+0x34+n*0x20)))
#define EPn_READ(ba,n)		((n<0 || n>15) ? ((void*)-1) : ((n==0) ? (ba+0x38) : (ba+0x38+n*0x20)))
#define EPn_WRITE(ba,n)		((n<0 || n>15) ? ((void*)-1) : ((n==0) ? (ba+0x3c) : (ba+0x3c+n*0x20)))

/* AHB-EPC Bridge Register */

#define AHBBINT(ba)			(ba+0x1008)
#define AHBBINTEN(ba)		(ba+0x100c)
#define DMAn_END_INT(n)		(1<<(17+n))
#define VBUS_INT			(1<<13)
#define MBUS_ERR_INT		(1<<6)
#define SBUS_ERR_INT		(1<<4)

#define EPCTR(ba)			(ba+0x1010)
#define PLL_LOCK			(1<<4)
#define PLL_RST				(1<<2)
#define EPC_RST				(1<<0)


static struct {
	char *name;
	struct usb_ep_caps caps;
	uint32_t maxpacket_limit;
	uint32_t buf_type; /* 0=single, 1=double */
} ep_info[] = {
#define EP_INFO(_name, _caps, _maxpacket_limit, _buf_type) \
	{ \
		.name = _name, \
		.caps = _caps, \
		.maxpacket_limit = _maxpacket_limit, \
		.buf_type = _buf_type, \
	}

	 /* ep0 is not doubled buffered, but it is bidirectional */
	EP_INFO("ep0", USB_EP_CAPS(USB_EP_CAPS_TYPE_CONTROL, USB_EP_CAPS_DIR_ALL), 64, 1),

	EP_INFO("ep1-bulk", USB_EP_CAPS(USB_EP_CAPS_TYPE_BULK, USB_EP_CAPS_DIR_ALL), 512, 1),
	EP_INFO("ep2-bulk", USB_EP_CAPS(USB_EP_CAPS_TYPE_BULK, USB_EP_CAPS_DIR_ALL), 512, 1),
	EP_INFO("ep3-bulk", USB_EP_CAPS(USB_EP_CAPS_TYPE_BULK, USB_EP_CAPS_DIR_ALL), 512, 0),
	EP_INFO("ep4-bulk", USB_EP_CAPS(USB_EP_CAPS_TYPE_BULK, USB_EP_CAPS_DIR_ALL), 512, 0),
	EP_INFO("ep5-bulk", USB_EP_CAPS(USB_EP_CAPS_TYPE_BULK, USB_EP_CAPS_DIR_ALL), 512, 0),

	EP_INFO("ep6-int", USB_EP_CAPS(USB_EP_CAPS_TYPE_INT, USB_EP_CAPS_DIR_IN), 1024, 0),
	EP_INFO("ep7-int", USB_EP_CAPS(USB_EP_CAPS_TYPE_INT, USB_EP_CAPS_DIR_IN), 1024, 0),
	EP_INFO("ep8-int", USB_EP_CAPS(USB_EP_CAPS_TYPE_INT, USB_EP_CAPS_DIR_IN), 1024, 0),
	EP_INFO("ep9-int", USB_EP_CAPS(USB_EP_CAPS_TYPE_INT, USB_EP_CAPS_DIR_IN), 1024, 0),

	EP_INFO("ep10-iso", USB_EP_CAPS(USB_EP_CAPS_TYPE_ISO, USB_EP_CAPS_DIR_ALL), 1024, 1),
	EP_INFO("ep11-iso", USB_EP_CAPS(USB_EP_CAPS_TYPE_ISO, USB_EP_CAPS_DIR_ALL), 1024, 1),
	EP_INFO("ep12-iso", USB_EP_CAPS(USB_EP_CAPS_TYPE_ISO, USB_EP_CAPS_DIR_ALL), 1024, 1),
	EP_INFO("ep13-iso", USB_EP_CAPS(USB_EP_CAPS_TYPE_ISO, USB_EP_CAPS_DIR_ALL), 1024, 1),

	/* The last two EP are available in HW but we have no buffer memory */
//	EP_INFO("ep14-iso", USB_EP_CAPS(USB_EP_CAPS_TYPE_ISO, USB_EP_CAPS_DIR_ALL), 1024, 1),
//	EP_INFO("ep15-iso", USB_EP_CAPS(USB_EP_CAPS_TYPE_ISO, USB_EP_CAPS_DIR_ALL), 1024, 1),
#undef EP_INFO
};

struct priv_usb_request {
	struct usb_request	req;
	struct list_head	queue;
};

struct priv_usb_ep {
	uint32_t			num;
	uint32_t			is_in;
	uint32_t			is_stall;
	struct priv_data	*pdata;
	struct usb_gadget	*gadget;
	struct usb_ep		ep;
	struct list_head	queue;
};

struct priv_data {
	void				*ba; /* baseaddr */
	struct clk			*clk;
	int					irq_u2f;
	int					irq_u2fepc;
	spinlock_t			lock;
	uint16_t					devstatus;
	uint16_t					ep0_data;
	struct priv_usb_request		ep0_req;
	struct priv_usb_ep			pep[ARRAY_SIZE(ep_info)];
	struct usb_gadget			gadget;
	struct usb_gadget_driver	*driver;
};
#define gadget_to_pdata(_gadget)	container_of(_gadget, struct priv_data, gadget)



#if defined(DEBUG) || defined(CONFIG_DYNAMIC_DEBUG)
static void dump_ep_config(struct priv_data *_pdata)
{
	uint32_t n, val32;
	char name[16], buftype[16], mode[16];

	for (n=1; n<16; n++) {
		val32 = ioread32(EPn_CONTROL(_pdata->ba, n));
		strncpy(name, (n<ARRAY_SIZE(ep_info)) ? ep_info[n].name : "unsupported", sizeof(name));
		strncpy(buftype, (val32 & 0x40000000) ? "double" : "single", sizeof(buftype));
		switch (getBF(val32, EPn_MODE)) {
		case BULK:
			strcpy(mode, "bulk");
			break;
		case INTERRUPT:
			strcpy(mode, "interrupt");
			break;
		case ISOCHRONOUS:
			strcpy(mode, "isochronous");
			break;
		default:
			strcpy(mode, "unknown");
			break;
		}
		pr_debug("ep%d: %s, %s buffered, %s\n", n, name, buftype, mode);
	}
}
#endif

static int netx4000_udc_chip_init(struct priv_data *_pdata);
static int netx4000_udc_chip_deinit(struct priv_data *_pdata);

/* ---------- EPn Handshake --------- */

static int netx4000_udc_epn_stall(struct priv_usb_ep *_pep, int _stall)
{
	struct priv_data *pdata = _pep->pdata;
	uint32_t val32;

	if (_pep->num == 0) {
		val32 = ioread32(EPn_CONTROL(pdata->ba, 0));
		(_stall) ? (val32 |= EP0_STL) : (val32 &= ~EP0_STL);
		iowrite32(val32, EPn_CONTROL(pdata->ba, 0));
	}
	else {
		val32 = ioread32(EPn_CONTROL(pdata->ba, _pep->num));
		(_stall) ? (val32 |= (EPn_OSTL_EN | EPn_OSTL | EPn_ISTL)) : (val32 &= ~(EPn_OSTL_EN | EPn_OSTL | EPn_ISTL));
		iowrite32(val32, EPn_CONTROL(pdata->ba, _pep->num));
	}

	return 0;
}

static int netx4000_udc_epn_is_stall(struct priv_usb_ep *_pep)
{
	struct priv_data *pdata = _pep->pdata;
	uint32_t val32;

	val32 = ioread32(EPn_CONTROL(pdata->ba, _pep->num));

	if (_pep->num == 0)
		return !!(val32 & EP0_STL);
	else
		return !!(val32 & (EPn_OSTL | EPn_ISTL));
}


static int netx4000_udc_handle_epn_zlp(struct priv_usb_ep *_pep)
{
	struct priv_data *pdata = _pep->pdata;
	uint32_t val32;

	if (_pep->num == 0) {
		val32 = ioread32(EPn_CONTROL(pdata->ba, 0));
		iowrite32((val32 | EP0_DEND | EP0_INAK_EN) & ~EP0_INAK, EPn_CONTROL(pdata->ba, 0));
	}
	else {
		pr_err("%s: failed at line=%d\n", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}

/* ---------- Functions for EP IRQ handling --------- */
/*
static void enable_ep_irq(struct priv_usb_ep *_pep, uint32_t mask)
{
	struct priv_data *pdata = _pep->pdata;
	uint32_t val32;

	val32 = ioread32(EPn_INT_ENA(pdata->ba, _pep->num));
	val32 |= mask;
	iowrite32(val32, EPn_INT_ENA(pdata->ba, _pep->num));

	if (val32) {
		val32 = ioread32(INT_ENA(pdata->ba));
		iowrite32(val32 | EPn_INT(_pep->num), INT_ENA(pdata->ba));
	}
}

static void disable_ep_irq(struct priv_usb_ep *_pep, uint32_t mask)
{
	struct priv_data *pdata = _pep->pdata;
	uint32_t val32;

	val32 = ioread32(EPn_INT_ENA(pdata->ba, _pep->num));
	val32 &= ~mask;
	iowrite32(val32, EPn_INT_ENA(pdata->ba, _pep->num));

	if (val32 == 0) {
		val32 = ioread32(INT_ENA(pdata->ba));
		iowrite32(val32 & ~EPn_INT(_pep->num), INT_ENA(pdata->ba));
	}
}
*/
static void set_ep_irq(struct priv_usb_ep *_pep, uint32_t mask)
{
	struct priv_data *pdata = _pep->pdata;
	uint32_t val32;

	iowrite32(mask, EPn_INT_ENA(pdata->ba, _pep->num));

	val32 = ioread32(INT_ENA(pdata->ba));
	if (mask)
		iowrite32(val32 | EPn_INT(_pep->num), INT_ENA(pdata->ba));
	else
		iowrite32(val32 & ~EPn_INT(_pep->num), INT_ENA(pdata->ba));
}

/* ---------- Request done handling ---------- */

/**
 * is_done - Check if the current request of IN transfer is completed.
 *
 * @_pep:	Pointer to the private usb endpoint structure
 *
 * Return:	1 if the current request is completed; otherwise 0
 */
static int is_done(struct priv_usb_ep *_pep)
{
	struct priv_usb_request *preq;

	if (list_empty(&_pep->queue)) {
		return 0;
	}

	preq = list_entry(_pep->queue.next, struct priv_usb_request, queue);

	if (preq->req.actual != preq->req.length)
		return 0;

	return 1;
}

static int done(struct priv_usb_ep *_pep)
{
	struct priv_data *pdata = _pep->pdata;
	struct priv_usb_request *preq;

	if (list_empty(&_pep->queue)) {
		pr_err("%s: failed at line=%d\n", __func__, __LINE__);
		return -EINVAL;
	}
	preq = list_entry(_pep->queue.next, struct priv_usb_request, queue);

	list_del_init(&preq->queue);

	preq->req.status = 0;

	spin_unlock(&pdata->lock);
	usb_gadget_giveback_request(&_pep->ep, &preq->req);
	spin_lock(&pdata->lock);

	pr_debug("preq %p for ep%d is given back (actual=%d)\n", preq, _pep->num, preq->req.actual);

	return 0;
}

/* ---------- EP IN/OUT handling ---------- */

static int netx4000_udc_handle_ep_in(struct priv_usb_ep *_pep)
{
	struct priv_data *pdata = _pep->pdata;
	struct priv_usb_request *preq;
	void *pvBuf;
	uint32_t nbytes, dw, val32=0;

	if (list_empty(&_pep->queue)) {
		return -EINVAL;
	}
	preq = list_entry(_pep->queue.next, struct priv_usb_request, queue);

	nbytes = preq->req.length - preq->req.actual;
	pvBuf = preq->req.buf + preq->req.actual;

	if (nbytes > _pep->ep.maxpacket)
		nbytes = _pep->ep.maxpacket;

	preq->req.actual += nbytes;

	pr_debug("Handle an ep%d preq %p (%d of %d bytes)\n", _pep->num, preq, nbytes, preq->req.length);

	for (; nbytes>=4; nbytes-=4, pvBuf+=4)
		iowrite32(*(uint32_t*)pvBuf, EPn_WRITE(pdata->ba, _pep->num));

	dw = nbytes;
	if (nbytes) {
		for (nbytes=0; nbytes<dw; nbytes++)
			val32 |= *((uint8_t*)pvBuf++)<<(8*nbytes);
		iowrite32(val32, EPn_WRITE(pdata->ba, _pep->num));
	}

	/* send IN frame */
	val32 = ioread32(EPn_CONTROL(pdata->ba, _pep->num));
	if (_pep->num == 0)
		iowrite32((val32 | EPn_DEND | EPn_DW(dw) | EP0_INAK_EN) & ~(EP0_INAK | EP0_ONAK), EPn_CONTROL(pdata->ba, _pep->num));
	else
		iowrite32(val32 | EPn_DEND | EPn_DW(dw), EPn_CONTROL(pdata->ba, _pep->num));

	return preq->req.length - preq->req.actual;
}

static int netx4000_udc_handle_ep_out(struct priv_usb_ep *_pep)
{
	struct priv_data *pdata = _pep->pdata;
	struct priv_usb_request *preq;
	void *pvBuf;
	uint32_t nbytes=0, val32, is_done=0;

	if (list_empty(&_pep->queue)) {
		pr_err("%s: failed at line=%d\n", __func__, __LINE__);
		return -EINVAL;
	}
	preq = list_entry(_pep->queue.next, struct priv_usb_request, queue);

	val32 = ioread32(EPn_LEN_DCNT(pdata->ba, _pep->num));
	if (_pep->num == 0)
		nbytes = val32 & 0x7f;
	else
		nbytes = val32 & 0x7ff;

	if ((nbytes < _pep->ep.maxpacket) || (preq->req.actual + nbytes >= preq->req.length)) {
		if (list_next_entry(preq, queue) == NULL) {
			val32 = ioread32(EPn_CONTROL(pdata->ba, _pep->num));
			iowrite32(val32 | EPn_ONAK, EPn_CONTROL(pdata->ba, _pep->num));
		}
		is_done = 1;
	}

	pvBuf = preq->req.buf + preq->req.actual;
	preq->req.actual += nbytes;

	pr_debug("Handle an ep%d preq %p (%d of %d bytes)\n", _pep->num, preq, nbytes, preq->req.length);

	/* read OUT frame */
	for (; nbytes>4; nbytes-=4, pvBuf+=4)
		*(uint32_t*)pvBuf = ioread32(EPn_READ(pdata->ba, _pep->num));

	if (nbytes) {
		char *p8 = (char*)&val32;
		val32 = ioread32(EPn_READ(pdata->ba, _pep->num));
		for (;nbytes;nbytes--) {
			*(uint8_t*)pvBuf++ = *p8++;
		}
	}

	return (is_done) ? 0 : 1;
}

/* ---------- gadget ops  ---------> */

static int netx4000_udc_get_frame(struct usb_gadget *_gadget)
{
	struct priv_data *pdata = gadget_to_pdata(_gadget);
	uint32_t val32;

	pr_debug("%s\n", _gadget->name);

	/* Frame number & USB address register */
	val32 = ioread32(ADDR_FRAME(pdata->ba));

	return getBF(val32, FRAME);
}

static int netx4000_udc_wakeup(struct usb_gadget *_gadget)
{
	struct priv_data *pdata = gadget_to_pdata(_gadget);
	uint32_t val32;

	pr_debug("%s\n", _gadget->name);

	if ((pdata->devstatus & (1 << USB_DEVICE_REMOTE_WAKEUP)) == 0)
		return -EINVAL;

	/* USB Control register */
	val32 = ioread32(USB_CONTROL(pdata->ba));
	iowrite32(val32 | RSUM_IN, USB_CONTROL(pdata->ba));
	mdelay(10); /* 10ms delay */
	iowrite32(val32, USB_CONTROL(pdata->ba)); /* Restore the register value */

	return 0;
}

static int netx4000_udc_pullup(struct usb_gadget *_gadget, int _is_on)
{
	struct priv_data *pdata = gadget_to_pdata(_gadget);
	uint32_t val32;

	pr_debug("%s, is_on=%d\n", _gadget->name, _is_on);

	val32 = ioread32(USB_CONTROL(pdata->ba));
	if (_is_on) {
		val32 |= PUE2;
		val32 &= ~CONNECTB;
	}
	else {
		val32 &= ~PUE2;
		val32 |= CONNECTB;
	}
	iowrite32(val32, USB_CONTROL(pdata->ba));

	return 0;
}

static int netx4000_udc_start(struct usb_gadget *_gadget, struct usb_gadget_driver *_driver)
{
	struct priv_data *pdata = gadget_to_pdata(_gadget);

	pr_debug("%s, %s\n", _gadget->name, _driver->driver.name);

	if (pdata->driver) {
		pr_err("%s: %s is already bound to %s\n", __func__, pdata->gadget.name, pdata->driver->driver.name);
		return -EBUSY;
	}

	pdata->driver = _driver;

	if (netx4000_udc_chip_init(pdata)) {
		pr_err("%s: netx4000_udc_chip_init() failed\n", __func__);
		return -ENODEV;
	}

	/* Enable IRQs */
	iowrite32(VBUS_INT | MBUS_ERR_INT | SBUS_ERR_INT, AHBBINTEN(pdata->ba));
	iowrite32(SPEED_MODE_INT | USB_RST_INT | SPND_INT | RESUM_INT, INT_ENA(pdata->ba));
	set_ep_irq(&pdata->pep[0], EP0_SETUP_INT);

#if defined(DEBUG) || defined(CONFIG_DYNAMIC_DEBUG)
	dump_ep_config(pdata);
#endif

	return 0;
}

static int netx4000_udc_stop(struct usb_gadget *_gadget)
{
	struct priv_data *pdata = gadget_to_pdata(_gadget);

	pr_debug("%s\n", _gadget->name);

	netx4000_udc_chip_deinit(pdata);

	pdata->driver = NULL;

	return 0;
}

/**
 * netx4000_udc_match_ep - Check for an appropriated endpoint for the gadget descriptor.
 *
 * @_gadget:		Pointer to the usb gadget
 * @ _desc:			Pointer to the endpoint descriptor
 * @ _comp_desc:	Pointer to the endpoint companion descriptor
 *
 * Because we have dedicated interrupt endpoints, we like to use such one for interrupt descriptors.
 * In cases of unavailable dedicated interrupt endpoints, the framework will use a bulk endpoint instead.
 *
 * Return: If available, a pointer to an appropriated EP is returned; otherwise NULL
 */
static struct usb_ep *netx4000_udc_match_ep(struct usb_gadget *_gadget, struct usb_endpoint_descriptor *_desc, struct usb_ss_ep_comp_descriptor *_comp_desc)
{
	struct usb_ep *ep;

    if (usb_endpoint_type(_desc) == USB_ENDPOINT_XFER_INT) {
	    list_for_each_entry (ep, &_gadget->ep_list, ep_list) {
   			if (ep->caps.type_int == 0)
				continue;
    		if (ep && usb_gadget_ep_match_desc(_gadget, ep, _desc, _comp_desc))
				return ep;
		}
    }

	return NULL;
}

static const struct usb_gadget_ops netx4000_udc_gadget_ops = {
	.get_frame			= netx4000_udc_get_frame,
	.wakeup				= netx4000_udc_wakeup,
	.pullup				= netx4000_udc_pullup,
	.udc_start			= netx4000_udc_start,
	.udc_stop			= netx4000_udc_stop,
	.match_ep			= netx4000_udc_match_ep,
};

/* ---------- EP ops  ---------> */

static int netx4000_udc_enable(struct usb_ep *_ep, const struct usb_endpoint_descriptor *_desc)
{
	struct priv_usb_ep *pep = container_of(_ep, struct priv_usb_ep, ep);
	struct priv_data *pdata = pep->pdata;
	uint32_t val32;

	pr_debug("%s, desc=%p\n", _ep->name, _desc);

	_ep->desc = _desc;
	_ep->enabled = 1;

	/* Configure the max packet size */
	val32 = ioread32(EPn_PCKT_ADRS(pdata->ba, pep->num));
	val32 &= ~EPn_MPKT_mask;
	val32 |= EPn_MPKT(_desc->wMaxPacketSize);
	iowrite32(val32, EPn_PCKT_ADRS(pdata->ba, pep->num));

	val32 = 0;

	/* Enable the EP */
	val32 |= EPn_EN;

	/* Configure the EP direction */
	if (usb_endpoint_dir_out(_desc)) {
		val32 |= EPn_DIR0(OUT);
		pep->is_in = 0;
		/* Set ONAK */
		val32 |= EPn_ONAK;
	}
	else {
		val32 |= EPn_DIR0(IN);
		pep->is_in = 1;
		/* Enable EP IRQs */
		set_ep_irq(pep, EPn_IN_INT);
	}

	/* Configure the EP type (Note: These bits are read only.) */
	switch (usb_endpoint_type(_desc)) {
	case USB_ENDPOINT_XFER_BULK:
		val32 |= setBF(BULK, EPn_MODE);
		break;
	case USB_ENDPOINT_XFER_INT:
		val32 |= setBF(INTERRUPT, EPn_MODE);
		break;
	case USB_ENDPOINT_XFER_ISOC:
		val32 |= setBF(ISOCHRONOUS, EPn_MODE);
		break;
	default:
		pr_err("%s: failed at line=%d\n", __func__, __LINE__);
		return -EINVAL;
	}

	/* Clear the EP write and read register */
	val32 |= EPn_CBCLR;

	/* Set EP configuration */
	iowrite32(val32, EPn_CONTROL(pdata->ba, pep->num));

	if (pep->is_in) {
		/* Enable IN EP IRQs */
		set_ep_irq(pep, EPn_IN_INT);
	}
	else {
		/* Enable OUT EP IRQs */
		set_ep_irq(pep, EPn_OUT_INT | EPn_OUT_NULL_INT);
	}

	return 0;
}

static int netx4000_udc_disable(struct usb_ep *_ep)
{
	struct priv_usb_ep *pep = container_of(_ep, struct priv_usb_ep, ep);
	struct priv_data *pdata = pep->pdata;
	unsigned long flags;

	pr_debug("%s\n", _ep->name);

	_ep->desc = NULL;
	_ep->enabled = 0;

	spin_lock_irqsave(&pdata->lock, flags);
	while (!list_empty(&pep->queue)) {
		done(pep);
	}
	spin_unlock_irqrestore(&pdata->lock, flags);

	/* Disable EP IRQs */
	set_ep_irq(pep, 0);

	/* Disable EP */
	iowrite32(0, EPn_CONTROL(pdata->ba, pep->num));

	return 0;
}

static struct usb_request *netx4000_udc_alloc_request(struct usb_ep *_ep, gfp_t _flags)
{
	struct priv_usb_request *preq;

	preq = kzalloc(sizeof(*preq), _flags);
	if (!preq) {
		pr_err("%s: failed at line=%d\n", __func__, __LINE__);
		return NULL;
	}

	INIT_LIST_HEAD(&preq->queue);

	pr_debug("%s, preq=%p, flags=0x%08x\n", _ep->name, preq, _flags);

	return &preq->req;
}

static void netx4000_udc_free_request(struct usb_ep *_ep, struct usb_request *_req)
{
	struct priv_usb_request *preq;

	preq = container_of(_req, struct priv_usb_request, req);

	pr_debug("%s, preq=%p\n", _ep->name, preq);

	BUG_ON(!list_empty(&preq->queue));
	kfree(preq);

	return;
}

static int netx4000_udc_queue(struct usb_ep *_ep, struct usb_request *_req, gfp_t _flags)
{
	struct priv_usb_ep *pep;
	struct priv_usb_request *preq;
	uint32_t val32, handle=0;
	unsigned long flags;

	pr_debug("%s, req=%p, flags=0x%08x\n", _ep->name, _req, _flags);

	pep = container_of(_ep, struct priv_usb_ep, ep);
	preq = container_of(_req, struct priv_usb_request, req);

	preq->req.actual = 0;
	preq->req.status = -EINPROGRESS;

	spin_lock_irqsave(&pep->pdata->lock, flags);

	if (list_empty(&pep->queue))
		handle = 1;

	list_add_tail(&preq->queue, &pep->queue);

	if (pep->is_in) {
		/* in */
		if (handle)
			netx4000_udc_handle_ep_in(pep);
	}
	else {
		/* out */
		val32 = ioread32(EPn_CONTROL(pep->pdata->ba, pep->num));
		iowrite32(val32 & ~EPn_ONAK, EPn_CONTROL(pep->pdata->ba, pep->num));
	}

	spin_unlock_irqrestore(&pep->pdata->lock, flags);

	return 0;
}

static int netx4000_udc_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct priv_usb_ep *pep = container_of(_ep, struct priv_usb_ep, ep);
	struct priv_usb_request *preq = container_of(_req, struct priv_usb_request, req);
	unsigned long flags;

	pr_debug("%s, req=%p\n", _ep->name, _req);

	spin_lock_irqsave(&pep->pdata->lock, flags);

	if (list_empty(&pep->queue)) {
		goto err_out;
	}

	list_del_init(&preq->queue);

	spin_unlock_irqrestore(&pep->pdata->lock, flags);

	preq->req.status = 0;

	usb_gadget_giveback_request(&pep->ep, &preq->req);

	return 0;

err_out:
	spin_unlock_irqrestore(&pep->pdata->lock, flags);
	return -EINVAL;
}

static int netx4000_udc_set_halt(struct usb_ep *_ep, int _val)
{
	struct priv_usb_ep *pep;

	pr_debug("%s, val=%d\n", _ep->name, _val);

	pep = container_of(_ep, struct priv_usb_ep, ep);

	netx4000_udc_epn_stall(pep, _val);

	return 0;
}

static const struct usb_ep_ops netx4000_udc_ep_ops = {
	.enable		= netx4000_udc_enable,
	.disable	= netx4000_udc_disable,
	.alloc_request	= netx4000_udc_alloc_request,
	.free_request	= netx4000_udc_free_request,
	.queue		= netx4000_udc_queue,
	.dequeue	= netx4000_udc_dequeue,
	.set_halt	= netx4000_udc_set_halt,
};

/* ---------- Interrupt handling ---------> */

static irqreturn_t netx4000_udc_u2f_isr(int irq, void *_pdata)
{
	struct priv_data *pdata = _pdata;
	uint32_t status, n;

	/* Read the status register */
	status = ioread32(AHBBINT(pdata->ba)) & ioread32(AHBBINTEN(pdata->ba));

	/* Ack the IRQs */
	iowrite32(status, AHBBINT(pdata->ba));

	if (status & SBUS_ERR_INT) {
		pr_debug("An error was returned in response to an access in 32-bit or larger units\n");
	}
	if (status & MBUS_ERR_INT) {
		pr_debug("An error response was received\n");
	}
	if (status & VBUS_INT) {
		pr_debug("VBUS value has changed\n");
	}
	for (n=1; n<ARRAY_SIZE(ep_info); n++) {
		if (status & DMAn_END_INT(n)) {
			pr_debug("A DMA transfer at an endpoint %d has completed\n", n);
		}
	}

	return IRQ_HANDLED;
}

/* ---------- Endpoint interrupt handling ---------> */

static void ep0_req_complete(struct usb_ep *_ep, struct usb_request *_req)
{
	/* Nothing to do */
}

static int get_status(struct priv_data *_pdata, struct usb_ctrlrequest *_ctrl)
{
	struct priv_data *pdata = _pdata;
	uint8_t epn;

	pdata->ep0_data = 0;

	switch (_ctrl->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		pdata->ep0_data = pdata->devstatus;
		break;
	case USB_RECIP_INTERFACE:
		break;
	case USB_RECIP_ENDPOINT:
		epn = _ctrl->wIndex & USB_ENDPOINT_NUMBER_MASK;
		if (netx4000_udc_epn_is_stall(&pdata->pep[epn]))
			pdata->ep0_data |= 1 << USB_ENDPOINT_HALT;
		break;
	default:
		goto err_out;
	}

	pdata->ep0_req.req.buf = &pdata->ep0_data;
	pdata->ep0_req.req.length = 2;
	pdata->ep0_req.req.complete = ep0_req_complete;

	spin_unlock(&pdata->lock);
	netx4000_udc_queue(pdata->gadget.ep0, &pdata->ep0_req.req, GFP_KERNEL);
	spin_lock(&pdata->lock);

	return 0;

err_out:
	return -EINVAL;
}

static int clear_feature(struct priv_data *_pdata, struct usb_ctrlrequest *_ctrl)
{
	struct priv_data *pdata = _pdata;
	uint8_t epn;

	switch (_ctrl->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		if (_ctrl->wValue & (1 << USB_DEVICE_REMOTE_WAKEUP))
			pdata->devstatus &= ~(1 << USB_DEVICE_REMOTE_WAKEUP);
		if (_ctrl->wValue & (1 << USB_DEVICE_TEST_MODE))
			goto err_out;
		break;
	case USB_RECIP_INTERFACE:
		break;
	case USB_RECIP_ENDPOINT:
		epn = _ctrl->wIndex & USB_ENDPOINT_NUMBER_MASK;
		if (_ctrl->wValue & (1 << USB_ENDPOINT_HALT))
			netx4000_udc_epn_stall(&pdata->pep[epn], 0);
		break;
	default:
		goto err_out;
	}

	netx4000_udc_handle_epn_zlp(&pdata->pep[0]);

	return 0;

err_out:
	return -EINVAL;
}

static int set_feature(struct priv_data *_pdata, struct usb_ctrlrequest *_ctrl)
{
	struct priv_data *pdata = _pdata;
	uint32_t val32;
	uint8_t epn;

	switch (_ctrl->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		if (_ctrl->wValue & (1 << USB_DEVICE_REMOTE_WAKEUP))
			pdata->devstatus |= (1 << USB_DEVICE_REMOTE_WAKEUP);
		if (_ctrl->wValue & (1 << USB_DEVICE_TEST_MODE)) {
			pdata->devstatus |= (1 << USB_DEVICE_TEST_MODE);
			/* Read out the test selector */
			val32 = _ctrl->wIndex >> 8;
			if (val32 > 4)
				goto err_out; /* unsupported */
			iowrite32(setBF(val32, USBTESTMODE), USB_CONTROL(pdata->ba));
		}
		break;
	case USB_RECIP_INTERFACE:
		break;
	case USB_RECIP_ENDPOINT:
		epn = _ctrl->wIndex & USB_ENDPOINT_NUMBER_MASK;
		if (_ctrl->wValue & (1 << USB_ENDPOINT_HALT))
			netx4000_udc_epn_stall(&pdata->pep[epn], 1);
		break;
	default:
		goto err_out;
	}

	netx4000_udc_handle_epn_zlp(&pdata->pep[0]);

	if (pdata->devstatus & (1 << USB_DEVICE_TEST_MODE)) {
		uint32_t timeout = 3000; /* 3ms (see USB spec. 2.0) */
		/* Wait for the completion of status stage */
		do {
			udelay(1);
			val32 = ioread32(EPn_STATUS(pdata->ba,0));
		} while ((val32 & EP0_STG_END_INT) && (timeout-- > 0));

		if (timeout) {
			/* Activate test mode */
			val32 = ioread32(TEST(pdata->ba));
			iowrite32(val32 | CS_TestModeEn, TEST(pdata->ba));
		}
	}

	return 0;

err_out:
	return -EINVAL;
}

static int set_address(struct priv_data *_pdata, struct usb_ctrlrequest *_ctrl)
{
	struct priv_data *pdata = _pdata;
	uint32_t val32;

	switch (_ctrl->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		val32 = ioread32(ADDR_FRAME(pdata->ba));
		val32 &= ~setBF(USB_ADDR_mask, USB_ADDR);
		val32 |= setBF(_ctrl->wValue, USB_ADDR);
		iowrite32(val32, ADDR_FRAME(pdata->ba));
		break;
	default:
		goto err_out;
	}

	netx4000_udc_handle_epn_zlp(&pdata->pep[0]);

	return 0;

err_out:
	return -EINVAL;
}

static int handle_setup_token(struct priv_data *_pdata)
{
	struct priv_data *pdata = _pdata;
	struct usb_ctrlrequest ctrl;
	uint32_t *p32 = (uint32_t*)&ctrl, is_done = 0;
	char *p8 = (char*)&ctrl;
	int rc;

	*p32++ = ioread32(DATA0(pdata->ba)); /* setup data byte[3..0] */
	*p32 = ioread32(DATA1(pdata->ba)); /* setup data byte[7..4] */

	pr_debug("%02x %02x %02x%02x %02x%02x %02x%02x\n", p8[0], p8[1], p8[2], p8[3], p8[4], p8[5], p8[6], p8[7]);

	/* Handle the endpoint direction of ep0 */
	if ((ctrl.wLength > 0) && ((ctrl.bRequestType & USB_DIR_IN) == 0)) {
		/* Disable ONAK */
		int32_t val32 = ioread32(EPn_CONTROL(pdata->ba, 0));
		iowrite32(val32 & ~EPn_ONAK, EPn_CONTROL(pdata->ba, 0));

		pdata->pep[0].is_in = 0; /* out */
		set_ep_irq(&pdata->pep[0], EP0_OUT_INT | EP0_SETUP_INT);
	}
	else {
		pdata->pep[0].is_in = 1; /* in */
		set_ep_irq(&pdata->pep[0], EP0_IN_INT | EP0_SETUP_INT);
	}

	/* Handle the standard device requests */
	if ((ctrl.bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD) {
		switch (ctrl.bRequest) {
		case USB_REQ_GET_STATUS:
			get_status(pdata, &ctrl);
			is_done = 1;
			break;
		case USB_REQ_CLEAR_FEATURE:
			rc = clear_feature(pdata, &ctrl);
			if (rc < 0)
				goto err_out;
			is_done = 1;
			break;
		case USB_REQ_SET_FEATURE:
			rc = set_feature(pdata, &ctrl);
			if (rc < 0)
				goto err_out;
			is_done = 1;
			break;
		case USB_REQ_SET_ADDRESS:
			set_address(pdata, &ctrl);
			is_done = 1;
			break;
		}
	}
	if (is_done)
		return 0;

	/* Pass all other request up to the gadget driver */
	if (pdata->driver) {
		spin_unlock(&pdata->lock);
		rc = pdata->driver->setup(&pdata->gadget, &ctrl);
		spin_lock(&pdata->lock);
		if (rc < 0) {
			goto err_out;
		}
		else {
			if (ctrl.bRequest == USB_REQ_SET_CONFIGURATION) {
				uint32_t val32 = ioread32(USB_CONTROL(pdata->ba));
				iowrite32(val32 | CONF, USB_CONTROL(pdata->ba));
			}
		}

		is_done = 1;
	}
	if (is_done)
		return 0;

err_out:
	netx4000_udc_epn_stall(&pdata->pep[0], 1);
	return rc;
}

static int handle_ep0_irq(struct priv_data *_pdata)
{
	struct priv_data *pdata = _pdata;
	uint32_t status, ack = 0;

	/* Read the status register */
	status = ioread32(EPn_STATUS(pdata->ba, 0)) & ioread32(EPn_INT_ENA(pdata->ba, 0));

	/* Ack the IRQs */
	iowrite32(~status, EPn_STATUS(pdata->ba, 0));

	if (status & EP0_IN_INT) {
		pr_debug("EP0_IN_INT occurred\n");
		if (is_done(&pdata->pep[0])) {
			done(&pdata->pep[0]);
		}
		netx4000_udc_handle_ep_in(&pdata->pep[0]);
		ack |= EP0_IN_INT;
	}
	if (status & EP0_OUT_INT) {
		pr_debug("EP0_OUT_INT occurred\n");
		if (netx4000_udc_handle_ep_out(&pdata->pep[0]) == 0)
			done(&pdata->pep[0]);
		netx4000_udc_handle_epn_zlp(&pdata->pep[0]);
		ack |= EP0_OUT_INT;
	}
	if (status & EP0_SETUP_INT) {
		handle_setup_token(pdata);
		ack |= EP0_SETUP_INT;
	}

	if (status & ~ack) {
		pr_warn("%s: Unsupported IRQs for ep%d (status=0x%08x)\n", __func__, 0, status & ~ack);
		ack |= status & ~ack;
	}

	return 0;
}

static int handle_epn_irq(struct priv_data *_pdata, uint32_t n)
{
	struct priv_data *pdata = _pdata;
	uint32_t status, ack = 0;

	/* Read the status register */
	status = ioread32(EPn_STATUS(pdata->ba, n)) & ioread32(EPn_INT_ENA(pdata->ba, n));

	/* Ack the IRQs */
	iowrite32(~status, EPn_STATUS(pdata->ba, n));

	if (status & EPn_IN_INT) {
		pr_debug("EPn_IN_INT occurred\n");
		if (is_done(&pdata->pep[n]))
			done(&pdata->pep[n]);
		netx4000_udc_handle_ep_in(&pdata->pep[n]);

		ack |= EPn_IN_INT;
	}
	if (status & (EPn_OUT_INT | EPn_OUT_NULL_INT)) {
		pr_debug("EPn_OUT_INT or EPn_OUT_NULL_INT occurred\n");
		if (netx4000_udc_handle_ep_out(&pdata->pep[n]) == 0)
			done(&pdata->pep[n]);
		ack |= EPn_OUT_INT;
	}

	if (status & ~ack) {
		pr_warn("%s: Unsupported IRQs for ep%d (status=0x%08x)\n", __func__, n, status & ~ack);
		ack |= status & ~ack;
	}

	return 0;
}

static irqreturn_t netx4000_udc_u2f_epc_isr(int irq, void *_pdata)
{
	struct priv_data *pdata = _pdata;
	uint32_t status, ack = 0, n;

	pr_debug("++++++++++\n");

	/* Read the status register */
	status = ioread32(STATUS(pdata->ba));

	/* Ack the IRQs */
	iowrite32(~status, STATUS(pdata->ba));

	if (status & RESUM_INT) {
		pr_debug("The resume signal has been received\n");
		ack |= RESUM_INT;
	}
	if (status & SPND_INT) {
		pr_debug("The USB device is in the Suspend state\n");
		ack |= SPND_INT;
	}
	if (status & USB_RST_INT) {
		pr_debug("A bus reset has been issued\n");
		ack |= USB_RST_INT;
	}
	if (status & SOF_INT) {
		pr_debug("An SOF or µSOF has been received\n");
		ack |= SOF_INT;
	}
	if (status & SOF_ERROR_INT) {
		pr_debug("An SOF or µSOF reception error has occurred\n");
		ack |= SOF_ERROR_INT;
	}
	if (status & SPEED_MODE_INT) {
		pr_debug("The speed mode has changed from FS to HS\n");
		pdata->gadget.speed = (ioread32(USB_STATUS(pdata->ba)) & SPEED_MODE_HS) ? (USB_SPEED_HIGH) : (USB_SPEED_FULL);
		ack |= SPEED_MODE_INT;
	}
	for (n=0; n<ARRAY_SIZE(ep_info); n++) {
		if (status & EPn_INT(n)) {
			/* The ack will be done by the EP related ISR */
			pr_debug("An interrupt related to ep%d has occurred\n", n);

			spin_lock(&pdata->lock);

			if (n==0)
				handle_ep0_irq(pdata);
			else
				handle_epn_irq(pdata, n);

			spin_unlock(&pdata->lock);
		}
	}

	pr_debug("----------\n");

	return IRQ_HANDLED;
}

/* ---------- Chip/Driver initialization ---------> */

/* TODO: Perhaps we should move this macros to the platform specific code. */
#define USB2CFG							(NETX4000_SYSTEMCTRL_VIRT_BASE + 0x10)
#define netx4000_usb_power_up()			(ioclear32(0x1, (void*)USB2CFG))
#define netx4000_usb_power_down()		(ioset32(0x1, (void*)USB2CFG))
#define is_func_port_enabled()			(!(ioread32((void*)USB2CFG) & 0x2))

static int netx4000_udc_chip_turn_on(struct priv_data *_pdata)
{
	struct priv_data *pdata = _pdata;
	uint32_t val32, timeout = 10000; /* 10ms */

	/* Enable the EPC */
	val32 = ioread32(EPCTR(pdata->ba));
	iowrite32(val32 & ~(PLL_RST | EPC_RST), EPCTR(pdata->ba)); /* Note: The PLL reset is shared/anded with the host controller */

	/* Wait for PLL lock */
	while((ioread32(EPCTR(pdata->ba)) & PLL_LOCK) == 0) {
		udelay(1);
		if (timeout--)
			continue;
		return -1;
	}

	return 0;
}

static int netx4000_udc_chip_turn_off(struct priv_data *_pdata)
{
	struct priv_data *pdata = _pdata;
	uint32_t val32;

	/* Disable the EPC */
	val32 = ioread32(EPCTR(pdata->ba));
	iowrite32(val32 | (PLL_RST | EPC_RST), EPCTR(pdata->ba)); /* Note: The PLL reset is shared/anded with the host controller */

	return 0;
}

static int netx4000_udc_chip_init(struct priv_data *_pdata)
{
	struct priv_data *pdata = _pdata;
	uint32_t n, ba=0 /* base address of internal RAM */;
	int rc;

	netx4000_usb_power_up();

	/* Resetting the chip */
	netx4000_udc_chip_turn_off(pdata);
	udelay(10);
	rc = netx4000_udc_chip_turn_on(pdata);
	if (rc) {
		pr_err("%s: Resetting the chip failed\n", __func__);
		return -EIO;
	}

	 /* Allocate internal memory for all EPs */
	for (n=0; n<ARRAY_SIZE(ep_info); n++) {
		if (n != 0)
			iowrite32(EPn_BASEAD(ba>>2), EPn_PCKT_ADRS(pdata->ba, n)); /* 32bit-word offset */
		ba += ep_info[n].maxpacket_limit;
		if (ep_info[n].buf_type)
			ba += ep_info[n].maxpacket_limit;
	}

	return 0;
}

static int netx4000_udc_chip_deinit(struct priv_data *_pdata)
{
	struct priv_data *pdata = _pdata;
	uint32_t n;

	/* Disable all IRQs */
	iowrite32(0, AHBBINTEN(pdata->ba));
	iowrite32(0, INT_ENA(pdata->ba));

	for (n=0; n<ARRAY_SIZE(ep_info); n++) {
		set_ep_irq(&pdata->pep[n], 0);
	}
	/**
	 * Attention:
	 * We only turn off the chip, because a clock disable or a usb power pown
	 * would also influence the host controller.
	 */
	netx4000_udc_chip_turn_off(pdata);

	return 0;
}

static int init_pdata(struct priv_data *pdata)
{
	uint32_t n;

	pdata->devstatus |= 1 << USB_DEVICE_SELF_POWERED;
	pdata->devstatus |= 1 << USB_DEVICE_REMOTE_WAKEUP;

	pdata->gadget.ops = &netx4000_udc_gadget_ops;
	pdata->gadget.is_selfpowered = 1;
	pdata->gadget.max_speed = USB_SPEED_HIGH;
	pdata->gadget.speed = USB_SPEED_UNKNOWN;
	pdata->gadget.ep0 = &pdata->pep[0].ep;
	pdata->gadget.name = DRIVER_NAME;
	pdata->gadget.dev.init_name	= "gadget";

	INIT_LIST_HEAD(&pdata->gadget.ep_list);
	INIT_LIST_HEAD(&pdata->gadget.ep0->ep_list);

	for (n=0; n<ARRAY_SIZE(ep_info); n++) {
		pdata->pep[n].num = n;
		pdata->pep[n].pdata = pdata;
		pdata->pep[n].ep.name = ep_info[n].name;
		pdata->pep[n].ep.ops = &netx4000_udc_ep_ops;
		pdata->pep[n].ep.maxpacket = ep_info[n].maxpacket_limit;
		pdata->pep[n].ep.maxpacket_limit = ep_info[n].maxpacket_limit;
		pdata->pep[n].ep.caps = ep_info[n].caps;
		if (n != 0)
			list_add_tail(&pdata->pep[n].ep.ep_list, &pdata->gadget.ep_list);
		INIT_LIST_HEAD(&pdata->pep[n].queue);
	}

	return 0;
}

static int netx4000_udc_probe(struct platform_device *pdev)
{
	struct priv_data *pdata;
	int rc = 0;

	if (!is_func_port_enabled())
		return -ENODEV;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: devm_kzalloc() failed\n", __func__);
		return -ENOMEM;
	}

	/* Read the register base address from DT and map it */
	pdata->ba = of_iomap(pdev->dev.of_node, 0);
	if (pdata->ba == NULL) {
		pr_err("%s: of_iomap() failed\n", __func__);
		return -EIO;
	}

	/* Read clock and enable it */
	pdata->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(pdata->clk)) {
		dev_err(&pdev->dev, "Getting clock failed\n");
		return PTR_ERR(pdata->clk);
	}

	rc = clk_prepare_enable(pdata->clk);
	if (rc) {
		dev_err(&pdev->dev, "Enabling clock failed\n");
		return rc;
	}

	pdev->dev.platform_data = pdata;
	platform_set_drvdata(pdev, pdata);

	init_pdata(pdata);

	/* Register the IRQs */
	pdata->irq_u2f = platform_get_irq_byname(pdev, "irq_u2f");
	rc = devm_request_irq(&pdev->dev, pdata->irq_u2f, netx4000_udc_u2f_isr, 0, DRIVER_NAME, pdata);
	if (rc) {
		pr_err("%s: devm_request_irq() failed\n", __func__);
		goto err_out;
	}
	pdata->irq_u2fepc = platform_get_irq_byname(pdev, "irq_u2fepc");
	rc = devm_request_irq(&pdev->dev, pdata->irq_u2fepc, netx4000_udc_u2f_epc_isr, 0, DRIVER_NAME, pdata);
	if (rc) {
		pr_err("%s: devm_request_irq() failed\n", __func__);
		goto err_out;
	}

	spin_lock_init(&pdata->lock);

	/* Register a UDC device */
	rc = usb_add_gadget_udc(&pdev->dev, &pdata->gadget);
	if (rc) {
		pr_err("%s: usb_add_gadget_udc() failed\n", __func__);
		goto err_out;
	}

	pr_info("%s: successfully initialized!\n", DRIVER_NAME);

	return 0;

err_out:
	return rc;
}

static int netx4000_udc_remove(struct platform_device *pdev)
{
	struct priv_data *pdata = platform_get_drvdata(pdev);

	WARN(pdata->driver, "Error: %s is still bound by %s\n", pdata->gadget.name, pdata->driver->driver.name);

	usb_del_gadget_udc(&pdata->gadget);

	clk_disable_unprepare(pdata->clk);

	pr_info("%s: successfully removed!\n", DRIVER_NAME);

	return 0;
}



static const struct of_device_id netx4000_udc_match[] = {
	{ .compatible = "hilscher,udc-netx4000", },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, netx4000_udc_match);

static struct platform_driver netx4000_udc_driver = {
	.probe = netx4000_udc_probe,
	.remove = netx4000_udc_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = netx4000_udc_match,
	},
};

static int __init netx4000_udc_init(void)
{
	pr_info("%s: %s\n", DRIVER_NAME, DRIVER_DESC);
	return platform_driver_register(&netx4000_udc_driver);
}
module_init(netx4000_udc_init);

static void __exit netx4000_udc_exit(void)
{
	platform_driver_unregister(&netx4000_udc_driver);
}
module_exit(netx4000_udc_exit);

MODULE_AUTHOR("Hilscher Gesellschaft fuer Systemautomation mbH");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");

