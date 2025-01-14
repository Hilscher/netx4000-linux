/*
* Quad Serial Peripheral Interface (QSPI) driver for Hilscher netX4000 based platforms
*
* drivers/spi/qspi-netx4000.c
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

#define DRIVER_DESC "Quad Serial Peripheral Interface (QSPI) driver for Hilscher netX4000 platforms"
#define DRIVER_NAME "qspi-netx4000"

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>

#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>

#ifdef CONFIG_DMA_ENGINE
	#include <linux/version.h>
	#include <linux/of_dma.h>
	#include <linux/dmaengine.h>
	#include <linux/dma-mapping.h>
#endif /* CONFIG_DMA_ENGINE */

#include <mach/hardware.h>


#define MAX_SPEED_HZ    50000000
#define MIN_SPEED_HZ    24400
#define NUM_CHIPSELECT  1
#define FIFO_DEPTH      16

#define setBF(val,regpos)  (((val) & regpos##_mask) << regpos##_shift)
#define getBF(val,regpos)  (((val) >> regpos##_shift) & regpos##_mask)

/*  Regdef */

#define SQI_CR0             0x00
#define CR0_FilterIn_mask           0x1
#define CR0_FilterIn_shift          27
#define CR0_FilterIn(val)           setBF(val,CR0_FilterIn)
#define CR0_SioCfg_mask             0x3
#define CR0_SioCfg_shift            22
#define CR0_SioCfg(val)             setBF(val,CR0_SioCfg)
#define CR0_SckMuladd_mask          0xfff
#define CR0_SckMuladd_shift         8
#define CR0_SckMuladd(val)          setBF(val,CR0_SckMuladd)
#define CR0_SckPhase_mask           0x1
#define CR0_SckPhase_shift          7
#define CR0_SckPhase(val)           setBF(val,CR0_SckPhase)
#define CR0_SckPol_mask             0x1
#define CR0_SckPol_shift            6
#define CR0_SckPol(val)             setBF(val,CR0_SckPol)
#define CR0_Datasize_mask           0xf
#define CR0_Datasize_shift          0
#define CR0_Datasize(val)           setBF(val,CR0_Datasize)

#define SQI_CR1             0x04
#define CR1_RxFifoClr_mask          0x1
#define CR1_RxFifoClr_shift         28
#define CR1_RxFifoClr(val)          setBF(val,CR1_RxFifoClr)
#define CR1_RxFifoWm_mask           0xf
#define CR1_RxFifoWm_shift          24
#define CR1_RxFifoWm(val)           setBF(val,CR1_RxFifoWm)
#define CR1_TxFifoClr_mask          0x1
#define CR1_TxFifoClr_shift         20
#define CR1_TxFifoClr(val)          setBF(val,CR1_TxFifoClr)
#define CR1_TxFifoWm_mask           0xf
#define CR1_TxFifoWm_shift          16
#define CR1_TxFifoWm(val)           setBF(val,CR1_TxFifoWm)
#define CR1_SpiTransCtrl_mask       0x1
#define CR1_SpiTransCtrl_shift      12
#define CR1_SpiTransCtrl(val)       setBF(val,CR1_SpiTransCtrl)
#define CR1_FssStatic_mask          0x1
#define CR1_FssStatic_shift         11
#define CR1_FssStatic(val)          setBF(val,CR1_FssStatic)
#define CR1_Fss_mask                0x7
#define CR1_Fss_shift               8
#define CR1_Fss(val)                setBF(val,CR1_Fss)
#define CR1_SqiEn_mask              0x1
#define CR1_SqiEn_shift             1
#define CR1_SqiEn(val)              setBF(val,CR1_SqiEn)

#define SQI_DR              0x08

#define SQI_SR              0x0c
#define SR_RxFifoLevel_mask         0x1f
#define SR_RxFifoLevel_shift        24
#define SR_TxFifoLevel_mask         0x1f
#define SR_TxFifoLevel_shift        16
#define SR_Busy_mask                0x1
#define SR_Busy_shift               4
#define SR_Busy(val)                setBF(val,SR_Busy)
#define SR_RxFifoNotEmpty_mask      0x1
#define SR_RxFifoNotEmpty_shift     2
#define SR_RxFifoNotEmpty(val)      setBF(val,SR_RxFifoNotEmpty)
#define SR_TxFifoNotFull_mask       0x1
#define SR_TxFifoNotFull_shift      1
#define SR_TxFifoNotFull(val)       setBF(val,SR_TxFifoNotFull)


#define SQI_TCR             0x10
#define TCR_MsByteFirst_mask        0x1
#define TCR_MsByteFirst_shift       29
#define TCR_MsByteFirst(val)        setBF(val,TCR_MsByteFirst)
#define TCR_MsBitFirst_mask         0x1
#define TCR_MsBitFirst_shift        28
#define TCR_MsBitFirst(val)         setBF(val,TCR_MsBitFirst)
#define TCR_Duplex_mask             0x3
#define TCR_Duplex_shift            26
#define TCR_Duplex(val)             setBF(val,TCR_Duplex)
#define TCR_Mode_mask               0x3
#define TCR_Mode_shift              24
#define TCR_Mode(val)               setBF(val,TCR_Mode)
#define TCR_StartTransfer_mask      0x1
#define TCR_StartTransfer_shift     23
#define TCR_StartTransfer(val)      setBF(val,TCR_StartTransfer)
#define TCR_TxOe_mask               0x1
#define TCR_TxOe_shift              22
#define TCR_TxOe(val)               setBF(val,TCR_TxOe)
#define TCR_TxOut_mask              0x1
#define TCR_TxOut_shift             21
#define TCR_TxOut(val)              setBF(val,TCR_TxOut)
#define TCR_TransferSize_mask       0x7ffff
#define TCR_TransferSize_shift      0
#define TCR_TransferSize(val)       setBF(val,TCR_TransferSize)

#define SQI_IRQ_MASK        0x14
#define SQI_IRQ_RAW         0x18
#define SQI_IRQ_MASKED      0x1c
#define SQI_IRQ_CLEAR       0x20
#define IRQ_SqiRomErr_mask          0x1
#define IRQ_SqiRomErr_shift         8
#define IRQ_SqiRomErr(val)          setBF(val,IRQ_SqiRomErr) // SQIROM error interrupt mask
#define IRQ_TransferEnd_mask        0x1
#define IRQ_TransferEnd_shift       7
#define IRQ_TransferEnd(val)        setBF(val,IRQ_TransferEnd) // Transfer end interrupt mask
#define IRQ_TxFifoEmpty_mask        0x1
#define IRQ_TxFifoEmpty_shift       6
#define IRQ_TxFifoEmpty(val)        setBF(val,IRQ_TxFifoEmpty) // Transmit FIFO empty interrupt mask (for netx100/500 compliance)
#define IRQ_RxFifoFull_mask         0x1
#define IRQ_RxFifoFull_shift        5
#define IRQ_RxFifoFull(val)         setBF(val,IRQ_RxFifoFull) // Receive FIFO full interrupt mask (for netx100/500 compliance)
#define IRQ_RxFifoNotEmpty_mask     0x1
#define IRQ_RxFifoNotEmpty_shift    4
#define IRQ_RxFifoNotEmpty(val)     setBF(val,IRQ_RxFifoNotEmpty) // Receive FIFO not empty interrupt mask (for netx100/500 compliance)
#define IRQ_TxFifoWm_mask           0x1
#define IRQ_TxFifoWm_shift          3
#define IRQ_TxFifoWm(val)           setBF(val,IRQ_TxFifoWm) // Transmit FIFO interrupt mask
#define IRQ_RxFifoWm_mask           0x1
#define IRQ_RxFifoWm_shift          2
#define IRQ_RxFifoWm(val)           setBF(val,IRQ_RxFifoWm) // Receive FIFO interrupt mask
#define IRQ_RxFifoTimeout_mask      0x1
#define IRQ_RxFifoTimeout_shift     1
#define IRQ_RxFifoTimeout(val)      setBF(val,IRQ_RxFifoTimeout) // Receive timeout interrupt mask
#define IRQ_RxFifoOverrun_mask      0x1
#define IRQ_RxFifoOverrun_shift     0
#define IRQ_RxFifoOverrun(val)      setBF(val,IRQ_RxFifoOverrun) // Receive FIFO overrun interrupt mask

#define SQI_DMACR           0x24
#define DMACR_TxDmaEn_mask          0x1
#define DMACR_TxDmaEn_shift         1
#define DMACR_TxDmaEn(val)          setBF(val,DMACR_TxDmaEn)
#define DMACR_RxDmaEn_mask          0x1
#define DMACR_RxDmaEn_shift         0
#define DMACR_RxDmaEn(val)          setBF(val,DMACR_RxDmaEn)

#define SQI_PIO_OUT         0x28
//#define reserved            0x2c
#define SQI_PIO_OE          0x30
#define SQI_PIO_IN          0x34
#define SQI_SQIROM_CFG      0x38

#define SQI_SQIROM_CFG_ENABLE 0x00000001
//#define reserved            0x3c


struct priv_data {
	struct device *dev;
	struct resource *res;
	void *base;
	uint32_t irq;

	uint32_t irq_pended;
	wait_queue_head_t wait_queue;

#ifdef CONFIG_DMA_ENGINE
	struct dma_chan *dma_tx_chan;
	struct dma_chan *dma_rx_chan;
	struct completion dma_done;
#endif /* CONFIG_DMA_ENGINE */
};

/* ------ Help functions --------------------------------------------------- */

/**
 * set_frequency:
 * @spi:       Pointer to the spi_device structure
 * @transfer:  Pointer to the spi_transfer structure which provide information about next transfer parameters
 *
 * Return:  Always 0
 */
static int set_frequency(struct spi_device *spi, struct spi_transfer *transfer)
{
	struct priv_data *priv = spi_master_get_devdata(spi->master);
	uint32_t val32, sck_muladd;

	sck_muladd = (transfer->speed_hz / 100 * 4096) / 1000000;

	val32 = ioread32(priv->base + SQI_CR0);
	val32 &= ~(CR0_SckMuladd(-1)); /* clear affected bits */
	iowrite32(CR0_SckMuladd(sck_muladd) | val32,  priv->base + SQI_CR0);

	return 0;
}

/**
 * set_bits_per_word:
 * @spi:       Pointer to the spi_device structure
 * @transfer:  Pointer to the spi_transfer structure which provide information about next transfer parameters
 *
 * Return:  Always 0
 */
static int set_bits_per_word(struct spi_device *spi, struct spi_transfer *transfer)
{
	struct priv_data *priv = spi_master_get_devdata(spi->master);
	uint32_t val32;

	val32 = ioread32(priv->base + SQI_CR0);
	val32 &= ~(CR0_Datasize(-1)); /* clear affected bits */
	iowrite32(CR0_Datasize(transfer->bits_per_word - 1) | val32,  priv->base + SQI_CR0);

	return 0;
}

/**
 * set_mode:
 * @spi:  Pointer to the spi_device structure
 *
 * Return:  Always 0
 */
static int set_mode(struct spi_device *spi)
{
	struct priv_data *priv = spi_master_get_devdata(spi->master);
	uint32_t val32;

	val32 = ioread32(priv->base + SQI_CR0);
	val32 &= ~(CR0_SckPhase(-1) & CR0_SckPol(-1)); /* clear affected bits */
	iowrite32(CR0_SckPhase(!!(spi->mode & SPI_CPHA)) | CR0_SckPol(!!(spi->mode & SPI_CPOL)) | val32,  priv->base + SQI_CR0);

	return 0;
}

/**
 * netx4000_qspi_enable_irq:
 * @mask:  Mask of IRQs
 * @priv:  Pointer to the private structure of the driver instance
 */
static inline void netx4000_qspi_enable_irq(uint32_t mask, struct priv_data *priv)
{
	uint32_t val32;

	val32 = ioread32(priv->base + SQI_IRQ_MASK);
	val32 |= mask;

	iowrite32(mask, priv->base + SQI_IRQ_CLEAR);
	iowrite32(val32, priv->base + SQI_IRQ_MASK);

	return;
}

/**
 * netx4000_qspi_disable_irq:
 * @mask:  Mask of IRQs
 * @priv:  Pointer to the private structure of the driver instance
 */
static inline void netx4000_qspi_disable_irq(uint32_t mask, struct priv_data *priv)
{
	uint32_t val32;

	val32 = ioread32(priv->base + SQI_IRQ_MASK);
	val32 &= ~mask;
	iowrite32(val32, priv->base + SQI_IRQ_MASK);

	return;
}

/**
 * calc_transfer_timeout:
 * @transfer:  Pointer to the spi_transfer structure which provide information about next transfer parameters
 *
 * Return:  Timeout in jiffies
 */
static unsigned long calc_transfer_timeout(struct spi_transfer *transfer)
{
	unsigned long timeout;

	/* calculate clock cycles per ms */
	timeout = DIV_ROUND_UP(transfer->speed_hz, MSEC_PER_SEC);

	/* calculate ms per transfer */
	if (transfer->tx_nbits)
		timeout = DIV_ROUND_UP(transfer->len * 8 / transfer->tx_nbits, timeout);
	else
		timeout = DIV_ROUND_UP(transfer->len * 8 / transfer->rx_nbits, timeout);

	/* calculate a timeout value in jiffies */
	timeout = 10 * msecs_to_jiffies(timeout) + 1;

	return timeout;
}

/* ------ DMA functions ---------------------------------------------------- */

#ifdef CONFIG_DMA_ENGINE

/**
 * netx4000_qspi_dma_init:
 * @master:  Pointer to the spi_master structure which provides information about the controller.
 *
 * Return:  0 on success; error value otherwise
 */
static int netx4000_qspi_dma_init(struct spi_master *master)
{
	struct priv_data *priv = spi_master_get_devdata(master);
	struct dma_slave_config rx_conf, tx_conf;
	int rc;

	rx_conf.direction = DMA_DEV_TO_MEM;
	rx_conf.src_addr = priv->res->start + SQI_DR;
	rx_conf.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	rx_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES; /* fixes kernel message 'nbpf_xfer_size(): invalid bus width ...' */
	rx_conf.src_maxburst = 1;
	rx_conf.dst_maxburst = 1;

	tx_conf.direction = DMA_MEM_TO_DEV;
	tx_conf.dst_addr = priv->res->start + SQI_DR;
	tx_conf.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES; /* fixes kernel message 'nbpf_xfer_size(): invalid bus width ...' */
	tx_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	tx_conf.src_maxburst = 1;
	tx_conf.dst_maxburst = 1;

	rc = dmaengine_slave_config(master->dma_rx, &rx_conf);
	if (rc) {
		dev_err(priv->dev, "dmaengine_slave_config() failed\n");
		goto err_out;
	}

	rc = dmaengine_slave_config(master->dma_tx, &tx_conf);
	if (rc) {
		dev_err(priv->dev, "dmaengine_slave_config() failed\n");
		goto err_out;
	}

	return 0;

err_out:
	dma_release_channel(master->dma_tx);
	dma_release_channel(master->dma_rx);

	return rc;
}

/**
 * netx4000_qspi_can_dma:  Checks for a possible dma transfer
 * @master:    Pointer to the spi_master structure which provides information about the controller.
 * @spi:       Pointer to the spi_device structure
 * @transfer:  Pointer to the spi_transfer structure which provide information about next transfer parameters
 *
 * Return:  true, if a dma transfer is possible; otherwise false
 */
static bool netx4000_qspi_can_dma(struct spi_master *master, struct spi_device *spi, struct spi_transfer *transfer)
{
	size_t dma_align = dma_get_cache_alignment();

	if (transfer->rx_buf) {
		if (transfer->rx_nbits == SPI_NBITS_SINGLE) /* The FIFO must be written in full DWords */
			return false;
		if (transfer->len % 4 || transfer->len < 64 || IS_ERR_OR_NULL(master->dma_rx) || !IS_ALIGNED((size_t)transfer->rx_buf, dma_align))
			return false;
	}

	if (transfer->tx_buf) {
		if (transfer->tx_nbits == SPI_NBITS_SINGLE) /* The FIFO must be written in full DWords */
			return false;
		if (transfer->len % 4 || transfer->len < 64 || IS_ERR_OR_NULL(master->dma_tx) || !IS_ALIGNED((size_t)transfer->tx_buf, dma_align))
			return false;
	}

	return true;
}

/**
 * netx4000_qspi_dma_callback:  Callback function to finish the DMA transfer
 * @priv:  Pointer to the private structure of the driver instance
 */
static void netx4000_qspi_dma_callback(void *_priv)
{
	struct priv_data *priv = _priv;

	iowrite32(0, priv->base + SQI_DMACR);
	complete(&priv->dma_done);

	return;
}

/**
 * netx4000_qspi_prep_sg:  Prepares a DMA transfer
 * @master:    Pointer to the spi_master structure which provides information about the controller.
 * @transfer:  Pointer to the spi_transfer structure which provide information about next transfer parameters
 *
 * Return:  0 on success; error value otherwise
 */
static int netx4000_qspi_prep_sg(struct spi_master *master, struct spi_transfer *transfer)
{
	struct priv_data *priv = spi_master_get_devdata(master);
	struct dma_chan *chan;
	struct scatterlist *sgl;
	struct dma_async_tx_descriptor *rxdesc = NULL, *txdesc = NULL;
	uint32_t nents, dir;
	dma_cookie_t cookie;
	int rc;

	if (transfer->rx_sg.nents > 0) {
		chan = master->dma_rx;
		sgl = transfer->rx_sg.sgl;
		nents = transfer->rx_sg.nents;
		dir = DMA_DEV_TO_MEM;
		rxdesc = dmaengine_prep_slave_sg(chan, sgl, nents, dir, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
		if (!rxdesc) {
			dev_err(priv->dev, "%s: dmaengine_prep_slave_sg() failed\n", __func__);
			return -EINVAL;
		}

		rxdesc->callback = netx4000_qspi_dma_callback;
		rxdesc->callback_param = priv;

		cookie = dmaengine_submit(rxdesc);
		rc = dma_submit_error(cookie);
		if (rc) {
			dev_err(priv->dev, "%s: dmaengine_submit() failed\n", __func__);
			return rc;
		}
	}

	if (transfer->tx_sg.nents > 0) {
		chan = master->dma_tx;
		sgl = transfer->tx_sg.sgl;
		nents = transfer->tx_sg.nents;
		dir = DMA_MEM_TO_DEV;
		txdesc = dmaengine_prep_slave_sg(chan, sgl, nents, dir, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
		if (!txdesc) {
			dev_err(priv->dev, "%s: dmaengine_prep_slave_sg() failed\n", __func__);
			return -EINVAL;
		}
		if (!rxdesc) {
			txdesc->callback = netx4000_qspi_dma_callback;
			txdesc->callback_param = priv;
		}

		cookie = dmaengine_submit(txdesc);
		rc = dma_submit_error(cookie);
		if (rc) {
			dev_err(priv->dev, "%s: dmaengine_submit() failed\n", __func__);
			return rc;
		}
	}

	return 0;
}

/**
 * netx4000_qspi_do_dma:  Initiates a DMA transfer
 * @master:    Pointer to the spi_master structure which provides information about the controller.
 * @transfer:  Pointer to the spi_transfer structure which provide information about next transfer parameters
 *
 * Return:  0 on success; error value otherwise
 */
static int netx4000_qspi_do_dma(struct spi_master *master, struct spi_transfer *transfer)
{
	struct priv_data *priv = spi_master_get_devdata(master);
	uint32_t timeout /* jiffies */;
	int rc;

	rc = netx4000_qspi_prep_sg(master, transfer);
	if (rc)
		return rc;

	reinit_completion(&priv->dma_done);

	timeout = calc_transfer_timeout(transfer);

	if ((transfer->rx_sg.nents > 0) && (transfer->tx_sg.nents == 0)) {
		iowrite32(DMACR_RxDmaEn(1), priv->base + SQI_DMACR);
		iowrite32(TCR_MsBitFirst(1) | TCR_Duplex(1 /* HD-RX */) | TCR_Mode(transfer->rx_nbits >> 1) | TCR_StartTransfer(1) | TCR_TransferSize(transfer->len-1), priv->base + SQI_TCR);
		dma_async_issue_pending(master->dma_rx);
	}
	else if ((transfer->rx_sg.nents == 0) && (transfer->tx_sg.nents > 0)) {
		iowrite32(DMACR_TxDmaEn(1), priv->base + SQI_DMACR);
		iowrite32(TCR_MsBitFirst(1) | TCR_Duplex(2 /* HD-TX */) | TCR_Mode(transfer->tx_nbits >> 1) | TCR_StartTransfer(1) | TCR_TransferSize(transfer->len-1), priv->base + SQI_TCR);
		dma_async_issue_pending(master->dma_tx);
	}
	else if ((transfer->rx_sg.nents > 0) && (transfer->tx_sg.nents > 0)) {
		iowrite32(DMACR_TxDmaEn(1) | DMACR_RxDmaEn(1), priv->base + SQI_DMACR);
		iowrite32(TCR_Duplex(3 /* FD */) | TCR_Mode(0) | TCR_StartTransfer(1) | TCR_TransferSize(transfer->len-1), priv->base + SQI_TCR);
		dma_async_issue_pending(master->dma_rx);
		dma_async_issue_pending(master->dma_tx);
	}
	else {
		dev_err(priv->dev, "%s: dma buffer error\n", __func__);
		rc = -ENOMEM;
	}

	/* Wait for transfer completed (e.g. dma mode) */
	if (!wait_for_completion_timeout(&priv->dma_done, timeout)) {
		dev_err(priv->dev, "%s: transfer timed out\n", __func__);
		rc = -ETIMEDOUT;
	}

	if (transfer->rx_sg.nents > 0)
		dmaengine_terminate_all(master->dma_rx);
	if (transfer->tx_sg.nents > 0)
		dmaengine_terminate_all(master->dma_tx);

	return (rc) ? rc : 0;
}
#endif /* CONFIG_DMA_ENGINE */

/* ------ API functions ---------------------------------------------------- */

/**
 * netx4000_qspi_setup:  Setup the driver
 * @spi:  Pointer to the spi_device structure
 *
 * Return:  Always 0
 */
static int netx4000_qspi_setup(struct spi_device *spi)
{
	if (spi->master->busy)
		return -EBUSY;

	return 0;
}

/**
 * netx4000_qspi_set_cs:  Select or deselect the chip select line
 * @spi:      Pointer to the spi_device structure
 * @is_high:  level of chip select
 */
static void netx4000_qspi_set_cs(struct spi_device *spi, bool is_high)
{
	struct priv_data *priv = spi_master_get_devdata(spi->master);
	uint32_t val32, cs_val;

	set_mode(spi);

	cs_val = (is_high) ? (0) : (1<<spi->chip_select);

	val32 = ioread32(priv->base + SQI_CR1);
	val32 &= ~(CR1_SpiTransCtrl(-1) | CR1_FssStatic(-1) | CR1_Fss(-1)); /* clear affected bits */
	iowrite32(CR1_SpiTransCtrl(1) | CR1_FssStatic(1) | CR1_Fss(cs_val) | val32, priv->base + SQI_CR1);

	return;
}

/**
 * netx4000_qspi_prepare_transfer_hardware:  Prepares hardware for transfer
 * @master:  Pointer to the spi_master structure which provides information about the controller.
 *
 * This function enables SPI master controller.
 *
 * Return:   0 on success; error value otherwise
 */
static int netx4000_qspi_prepare_transfer_hardware(struct spi_master *master)
{
	struct priv_data *priv = spi_master_get_devdata(master);

	iowrite32(CR0_SioCfg(1 /* all IO pins */), priv->base + SQI_CR0);
	iowrite32(CR1_SqiEn(1), priv->base + SQI_CR1);

	return 0;
}

/**
 * netx4000_qspi_unprepare_transfer_hardware:  Relaxes hardware after transfer
 * @master:  Pointer to the spi_master structure which provides information about the controller.
 *
 * This function disables the SPI master controller.
 *
 * Return:   Always 0
 */
static int netx4000_qspi_unprepare_transfer_hardware(struct spi_master *master)
{
	struct priv_data *priv = spi_master_get_devdata(master);

	iowrite32(CR1_SqiEn(0), priv->base + SQI_CR1);

	return 0;
}

/**
 * netx4000_qspi_transfer_one:  Initiates the QSPI transfer
 * @master:    Pointer to the spi_master structure which provides information about the controller.
 * @spi:       Pointer to the spi_device structure
 * @transfer:  Pointer to the spi_transfer structure which provide information about next transfer parameters
 *
 * Return:     Number of bytes transferred in the last transfer on success; error value otherwise
 */
static int netx4000_qspi_transfer_one(struct spi_master *master, struct spi_device *spi, struct spi_transfer *transfer)
{
	struct priv_data *priv = spi_master_get_devdata(master);
	void *pvTxBuf, *pvRxBuf;
	uint32_t val32, nWords, wordSize /* in bytes */, wm;
	unsigned long timeout, tsTransferTimeout;  /* all in jiffies */
	int rc, nbytes = transfer->len;

	set_frequency(spi, transfer);
	set_bits_per_word(spi, transfer);

	val32 = ioread32(priv->base + SQI_SR);
	if (getBF(val32, SR_RxFifoLevel) || getBF(val32, SR_TxFifoLevel)) {
		dev_warn(priv->dev, "%s: unexpected fifo level found => clearing rx and tx fifo\n", __func__);
		val32 = ioread32(priv->base + SQI_CR1);
		iowrite32(val32 | CR1_RxFifoClr(1) | CR1_TxFifoClr(1), priv->base + SQI_CR1);
	}

#ifdef CONFIG_DMA_ENGINE
	if ((transfer->rx_sg.nents > 0) || (transfer->tx_sg.nents > 0)) {
		rc = netx4000_qspi_do_dma(master, transfer);
		if (rc) {
			dev_err(priv->dev, "%s: netx4000_qspi_do_dma() failed\n", __func__);
			goto err_out;
		}
		nbytes = 0;
	}
#endif /* CONFIG_DMA_ENGINE */

	if ((transfer->rx_sg.nents == 0) && (transfer->tx_sg.nents == 0)) {
		pvTxBuf = (void*)transfer->tx_buf;
		pvRxBuf = transfer->rx_buf;

		timeout = calc_transfer_timeout(transfer);
		tsTransferTimeout = jiffies + timeout;

		/* write only, half duplex transfer */
		if (pvTxBuf && !pvRxBuf) {
			iowrite32(TCR_MsBitFirst(1) | TCR_Duplex(2 /* HD-TX */) | TCR_Mode(transfer->tx_nbits >> 1) | TCR_StartTransfer(1) | TCR_TransferSize(nbytes-1), priv->base + SQI_TCR);
			while (nbytes > 0) {
				nWords = FIFO_DEPTH - getBF(ioread32(priv->base + SQI_SR), SR_TxFifoLevel);
				if (!nWords) {
					/* tx fifo is full */
					if (!priv->irq) {
						if (time_after_eq(jiffies, tsTransferTimeout)) {
							dev_err(priv->dev, "%s: tx fifo timed out\n", __func__);
							rc = -ETIMEDOUT;
							goto err_out;
						}
						cpu_relax();
						continue;
					}
					val32 = ioread32(priv->base + SQI_CR1);
					val32 &= ~(CR1_TxFifoWm(-1)); /* clear affected bits */
					wm = max((int)(FIFO_DEPTH - ((nbytes + transfer->tx_nbits - 1) / transfer->tx_nbits)), FIFO_DEPTH / 2); /* new watermark */
					iowrite32(CR1_TxFifoWm(wm) | val32, priv->base + SQI_CR1);
					netx4000_qspi_enable_irq(IRQ_TxFifoWm(1), priv);
					rc = wait_event_timeout(priv->wait_queue, priv->irq_pended, timeout);
					if (!rc) {
						netx4000_qspi_disable_irq(IRQ_TxFifoWm(1), priv);
						dev_err(priv->dev, "%s: tx fifo timed out\n", __func__);
						rc = -ETIMEDOUT;
						goto err_out;
					}
					priv->irq_pended = 0;
					nWords = FIFO_DEPTH - wm;
				}

				if (nWords > nbytes)
					nWords = nbytes;

				while (nWords--) {
					wordSize = (nbytes >= transfer->tx_nbits) ? transfer->tx_nbits : nbytes; // tx_bits= 1, 2 or 4
					memcpy(&val32, pvTxBuf, wordSize);
					iowrite32(val32, priv->base + SQI_DR);

					nbytes -= wordSize;
					pvTxBuf += wordSize;
				}
			}
		}

		/* read only, half duplex transfer */
		else if (!pvTxBuf && pvRxBuf) {
			iowrite32(TCR_MsBitFirst(1) | TCR_Duplex(1 /* HD-RX */) | TCR_Mode(transfer->rx_nbits >> 1) | TCR_StartTransfer(1) | TCR_TransferSize(nbytes-1), priv->base + SQI_TCR);
			while (nbytes > 0) {
				nWords = getBF(ioread32(priv->base + SQI_SR), SR_RxFifoLevel);
				if (!nWords) {
					/* fifo is emtpy */
					if (!priv->irq || (nbytes == transfer->len) /* <= prevents the first IRQ */) {
						if (time_after_eq(jiffies, tsTransferTimeout)) {
							dev_err(priv->dev, "%s: rx fifo timed out\n", __func__);
							rc = -ETIMEDOUT;
							goto err_out;
						}
						cpu_relax();
						continue;
					}
					val32 = ioread32(priv->base + SQI_CR1);
					val32 &= ~(CR1_RxFifoWm(-1)); /* clear affected bits */
					wm = min((int)(nbytes + transfer->rx_nbits - 1) / transfer->rx_nbits, FIFO_DEPTH / 2) - 1; /* new watermark */
					iowrite32(CR1_RxFifoWm(wm) | val32, priv->base + SQI_CR1);
					netx4000_qspi_enable_irq(IRQ_RxFifoWm(1), priv);
					rc = wait_event_timeout(priv->wait_queue, priv->irq_pended, timeout);
					if (!rc) {
						netx4000_qspi_disable_irq(IRQ_RxFifoWm(1), priv);
						dev_err(priv->dev, "%s: rx fifo timed out\n", __func__);
						rc = -ETIMEDOUT;
						goto err_out;
					}
					priv->irq_pended = 0;
					nWords = wm + 1;
				}

				if (nWords > nbytes)
					nWords = nbytes;

				while (nWords--) {
					wordSize = (nbytes >= transfer->rx_nbits) ? transfer->rx_nbits : nbytes; // rx_bits= 1, 2 or 4
					val32 = ioread32(priv->base + SQI_DR);
					memcpy(pvRxBuf, &val32, wordSize);

					nbytes -= wordSize;
					pvRxBuf += wordSize;
				}
			}
		}

		/* read/write, full duplex transfer */
		else if (pvTxBuf && pvRxBuf) {
			uint32_t nRxWords = 0, nTxWords = 0;

			wordSize = (transfer->bits_per_word > 8) ? 2 : 1;

			iowrite32(TCR_Duplex(3 /* FD */) | TCR_Mode(0) | TCR_StartTransfer(1) | TCR_TransferSize((nbytes / wordSize) - 1), priv->base + SQI_TCR);
			while (nbytes > 0) {
				nWords = min(((transfer->len / wordSize) - nTxWords), FIFO_DEPTH - (nTxWords - nRxWords));
				while (nWords--) {
					memcpy(&val32, pvTxBuf, wordSize);
					iowrite32(val32, priv->base + SQI_DR);

					pvTxBuf += wordSize;
					nTxWords++;
				}

				nWords = getBF(ioread32(priv->base + SQI_SR), SR_RxFifoLevel);
				if (!nWords) {
					/* fifo is emtpy */
					if (!priv->irq || (nbytes == transfer->len) /* <= prevents the first IRQ */) {
						if (time_after_eq(jiffies, tsTransferTimeout)) {
							dev_err(priv->dev, "%s: rx fifo timed out\n", __func__);
							rc = -ETIMEDOUT;
							goto err_out;
						}
						cpu_relax();
						continue;
					}
					val32 = ioread32(priv->base + SQI_CR1);
					val32 &= ~(CR1_RxFifoWm(-1)); /* clear affected bits */
					wm = (nTxWords - nRxWords) / 2; /* new watermark */
					iowrite32(CR1_RxFifoWm(wm) | val32, priv->base + SQI_CR1);
					netx4000_qspi_enable_irq(IRQ_RxFifoWm(1), priv);
					rc = wait_event_timeout(priv->wait_queue, priv->irq_pended, timeout);
					if (!rc) {
						netx4000_qspi_disable_irq(IRQ_RxFifoWm(1), priv);
						dev_err(priv->dev, "%s: rx fifo timed out\n", __func__);
						rc = -ETIMEDOUT;
						goto err_out;
					}
					priv->irq_pended = 0;
					nWords = wm + 1;
				}

				if (nWords > nbytes)
					nWords = nbytes;

				while (nWords--) {
					val32 = ioread32(priv->base + SQI_DR);
					memcpy(pvRxBuf, &val32, wordSize);

					nbytes -= wordSize;
					pvRxBuf += wordSize;
					nRxWords++;
				}
			}
		}

		/* Wait for transfer finished */
		while (ioread32(priv->base + SQI_SR) & SR_Busy(1)) {
			if (time_after_eq(jiffies, tsTransferTimeout)) {
				dev_err(priv->dev, "%s: transfer timed out\n", __func__);
				rc = -ETIMEDOUT;
				goto err_out;
			}
			cpu_relax();
		}
	}

	spi_finalize_current_transfer(master);

	return transfer->len - nbytes;

err_out:
	return rc;
}

/* ------ IRQ function ----------------------------------------------------- */

/**
 * netx4000_qspi_isr:
 * @irq:     IRQ number
 * @dev_id:  Pointer to the private structure of the driver instance
 *
 * Return:  0 on success; error value otherwise
 */
static irqreturn_t netx4000_qspi_isr(int irq, void *dev_id)
{
	struct priv_data *priv = dev_id;
	uint32_t status;

	status = ioread32(priv->base + SQI_IRQ_MASKED);

	dev_dbg(priv->dev, "%s: status = 0x%08x\n", __func__, status);

	if (status & IRQ_TxFifoWm(1)) {
		dev_dbg(priv->dev, "Transmit FIFO interrupt occurred\n");
		netx4000_qspi_disable_irq(IRQ_TxFifoWm(1), priv);
		priv->irq_pended = 1;
		wake_up(&priv->wait_queue);
	}
	if (status & IRQ_RxFifoWm(1)) {
		dev_dbg(priv->dev, "Receive FIFO interrupt occurred\n");
		netx4000_qspi_disable_irq(IRQ_RxFifoWm(1), priv);
		priv->irq_pended = 1;
		wake_up(&priv->wait_queue);
	}

	iowrite32(status, priv->base + SQI_IRQ_CLEAR);

	return IRQ_HANDLED;
}

static void netx4000_qspi_reset_controller(struct priv_data *priv, struct spi_master *master)
{
	u32 cr1, tcr;

	if (!ioread32(priv->base + SQI_SQIROM_CFG) & SQI_SQIROM_CFG_ENABLE)
		return;

	/* Reset in case ROM Loader left flash in 4 Bit mode */
	dev_info(priv->dev, "SQIROM was enabled at startup. Disabling now!");
	iowrite32(0, priv->base + SQI_SQIROM_CFG);

	/* ROM Loader keeps it in 4 Bit TX mode, so use it as it is */

	/* Assert CS */
	cr1 = ioread32(priv->base + SQI_CR1);
	iowrite32(cr1 | CR1_FssStatic(1) | CR1_Fss(1), priv->base + SQI_CR1);

	/* Send 8 clocks with 0xf to exit fast read mode */
	iowrite32(0xFFFFFFFF, priv->base + SQI_DR);

	tcr = ioread32(priv->base + SQI_TCR);
	tcr &= ~(TCR_TransferSize(-1));
	tcr |= (TCR_TransferSize(3) | TCR_StartTransfer(1));
	iowrite32(tcr, priv->base + SQI_TCR);
	while(ioread32(priv->base + SQI_SR) & SR_Busy(1)) ;

	/* De-assert CS */
	cr1 &= ~(CR1_Fss(-1));
	iowrite32(cr1, priv->base + SQI_CR1);

	netx4000_qspi_unprepare_transfer_hardware(master);
}

/* ------ Driver functions ------------------------------------------------- */

/**
 * netx4000_qspi_probe:
 * @pdev:  Pointer to the platform device of spi master
 *
 * Return:  0 on success; error value otherwise
 */
static int netx4000_qspi_probe (struct platform_device *pdev)
{
	struct priv_data *priv;
	struct spi_master *master;
	struct dma_chan *dma_chan;
	int rc, irq;

	master = spi_alloc_master(&pdev->dev, sizeof(*priv));
	if (!master) {
		dev_err(&pdev->dev, "spi_alloc_master() failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, master);

	priv = spi_master_get_devdata(master);
	priv->dev = &pdev->dev;

	/* Read the register base address from DT and map it */
	priv->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->base = devm_ioremap_resource(priv->dev, priv->res);
	if (IS_ERR(priv->base)) {
		dev_err(priv->dev, "devm_ioremap_resource() failed\n");
		rc = PTR_ERR(priv->base);
		goto err_out;
	}

	/* Read the IRQ number from DT and register it */
	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	rc = devm_request_irq(priv->dev, irq, netx4000_qspi_isr, 0, dev_name(&pdev->dev), priv);
	if (rc) {
		dev_warn(priv->dev, "devm_request_irq() failed\n");
		dev_info(priv->dev, "Smart polling/irq mode disabled => Falling back to polling mode\n");
	}
	else {
		priv->irq = irq;
		init_waitqueue_head(&priv->wait_queue);
		dev_info(priv->dev, "Smart polling/irq mode enabled\n");
	}

#ifdef CONFIG_DMA_ENGINE
	/* Read the DMA channels from DT and allocate these */
	dma_chan = of_dma_request_slave_channel(priv->dev->of_node, "rx");
	if (IS_ERR(dma_chan))
		dev_warn(priv->dev, "of_dma_request_slave_channel() failed\n");
	else {
		master->dma_rx = dma_chan;
		dev_info(priv->dev, "RX DMA mode enabled\n");
	}

	dma_chan = of_dma_request_slave_channel(priv->dev->of_node, "tx");
	if (IS_ERR(dma_chan))
		dev_warn(priv->dev, "of_dma_request_slave_channel() failed\n");
	else {
		master->dma_tx = dma_chan;
		dev_info(priv->dev, "TX DMA mode enabled\n");
	}

	if (master->dma_rx || master->dma_tx) {
		master->dma_alignment = dma_get_cache_alignment();
		master->can_dma = netx4000_qspi_can_dma;
		master->max_dma_len = 512*1024;
		netx4000_qspi_dma_init(master);
	}

	init_completion(&priv->dma_done);
#endif /* CONFIG_DMA_ENGINE */

	/* Configure the SPI master structure */
	master->dev.of_node = pdev->dev.of_node;
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_RX_DUAL | SPI_RX_QUAD | SPI_TX_DUAL | SPI_TX_QUAD;
	master->bits_per_word_mask = SPI_BPW_RANGE_MASK(4,16);
	master->max_speed_hz = MAX_SPEED_HZ;
	master->min_speed_hz = MIN_SPEED_HZ;
	master->num_chipselect = NUM_CHIPSELECT;

	master->setup = netx4000_qspi_setup;
	master->set_cs = netx4000_qspi_set_cs;
	master->transfer_one = netx4000_qspi_transfer_one;
	master->prepare_transfer_hardware = netx4000_qspi_prepare_transfer_hardware;
	master->unprepare_transfer_hardware = netx4000_qspi_unprepare_transfer_hardware;

	netx4000_qspi_reset_controller(priv, master);

	rc = devm_spi_register_master(priv->dev, master);
	if (rc) {
		dev_err(priv->dev, "devm_spi_register_master() failed\n");
		goto err_out;
	}

	dev_info(priv->dev, "successfully initialized!\n");

	return 0;

err_out:
	spi_master_put(master);
	return rc;
}

/**
 * netx4000_qspi_remove:
 * @pdev:  Pointer to the platform device of spi master
 *
 * Return:   Always 0
 */
static int netx4000_qspi_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);

	if (!IS_ERR_OR_NULL(master->dma_rx))
		dma_release_channel(master->dma_rx);
	if (!IS_ERR_OR_NULL(master->dma_tx))
		dma_release_channel(master->dma_tx);

    dev_info(&pdev->dev, "successfully removed!\n");

    return 0;
}

/* ------------------------------------------------------------------------- */

static struct of_device_id netx4000_qspi_match[] = {
	{ .compatible = "hilscher,qspi-netx4000", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, netx4000_qspi_match);

static struct platform_driver netx4000_qspi_driver = {
	.probe  = netx4000_qspi_probe,
	.remove	= netx4000_qspi_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = netx4000_qspi_match,
	}
};

static int __init netx4000_qspi_init(void)
{
	pr_info("%s: %s\n", DRIVER_NAME, DRIVER_DESC);
	return platform_driver_register(&netx4000_qspi_driver);
}
module_init(netx4000_qspi_init);

static void __exit netx4000_qspi_exit(void)
{
	platform_driver_unregister(&netx4000_qspi_driver);
}
module_exit(netx4000_qspi_exit);

MODULE_AUTHOR("Hilscher Gesellschaft fuer Systemautomation mbH");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");

