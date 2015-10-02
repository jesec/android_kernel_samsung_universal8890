# SAMSUNG MAILBOX API
ifeq ($(CONFIG_SOC_EXYNOS8890), y)
obj-$(CONFIG_EXYNOS_MBOX)		+= apm-exynos.o
ifeq ($(CONFIG_SOC_EXYNOS8890_EVT1), y)
obj-$(CONFIG_EXYNOS_MBOX)		+= mailbox-exynos8.o apm-exynos8890.o
else
obj-$(CONFIG_EXYNOS_MBOX)		+= mailbox-exynos8_evt0.o apm-exynos8890_evt0.o
endif
endif
