ARCH:=arm
SUBTARGET:=an7552
BOARDNAME:=AN7552 / AN7563
CPU_TYPE:=cortex-a7
KERNELNAME:=Image dtbs vmlinuz
FEATURES+=source-only

DEFAULT_PACKAGES += \
	airoha-en7552-npu-firmware

define Target/Description
  Build firmware images for Airoha an7552 and an7563 ARM based boards.
endef
