GCC_VERSION      := $(shell $(CONFIG_SHELL) $(PWD)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)

ccflags-y += -Idrivers/media/platform/msm/camera_v2_j75/jpeg_10
ccflags-y += -Idrivers/media/platform/msm/camera_v2_j75/sensor/io

obj-$(CONFIG_MSMB_JPEG) += msm_jpeg_dev.o msm_jpeg_sync.o msm_jpeg_core.o msm_jpeg_hw.o msm_jpeg_platform.o
