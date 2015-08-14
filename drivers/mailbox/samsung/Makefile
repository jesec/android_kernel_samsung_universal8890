# SAMSUNG MAILBOX API
ifeq ($(CONFIG_SOC_EXYNOS8890), y)
obj-y		+= apm-exynos.o
ifeq ($(CONFIG_SOC_EXYNOS8890_EVT1), y)
obj-y		+= mailbox-exynos8.o apm-exynos8890.o
else
obj-y		+= mailbox-exynos8_evt0.o apm-exynos8890_evt0.o
endif
endif
