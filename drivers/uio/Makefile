# SPDX-License-Identifier: GPL-2.0

uio_netx-flags-$(CONFIG_UIO_NETX_DMA_SUPPORT) \
			:= -DDMA_SUPPORT
CFLAGS_uio_netx.o	+= $(uio_netx-flags-y)

obj-$(CONFIG_UIO)	+= uio.o
obj-$(CONFIG_UIO_CIF)	+= uio_cif.o
obj-$(CONFIG_UIO_PDRV_GENIRQ)	+= uio_pdrv_genirq.o
obj-$(CONFIG_UIO_DMEM_GENIRQ)	+= uio_dmem_genirq.o
obj-$(CONFIG_UIO_AEC)	+= uio_aec.o
obj-$(CONFIG_UIO_SERCOS3)	+= uio_sercos3.o
obj-$(CONFIG_UIO_PCI_GENERIC)	+= uio_pci_generic.o
obj-$(CONFIG_UIO_NETX)	+= uio_netx.o
obj-$(CONFIG_UIO_PRUSS)         += uio_pruss.o
obj-$(CONFIG_UIO_MF624)         += uio_mf624.o
obj-$(CONFIG_UIO_FSL_ELBC_GPCM)	+= uio_fsl_elbc_gpcm.o
obj-$(CONFIG_UIO_HV_GENERIC)	+= uio_hv_generic.o
