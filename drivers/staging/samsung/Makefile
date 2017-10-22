# drivers/staging/samsung/Makefile
#
# Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

obj-y				:=
obj-m				:=
obj-n				:=
obj-				:=

# Samsung Battery
obj-$(CONFIG_BATTERY_SAMSUNG)	+= sec_batt.o

# Samsung Logging Feature
obj-$(CONFIG_SEC_EXT)		+= sec_log.o
# Samsung sec sysfs Feature
obj-$(CONFIG_SEC_SYSFS)		+= sec_sysfs.o
# Samsung Reset Feature
obj-$(CONFIG_SEC_REBOOT)	+= sec_reboot.o
# Samsung Debugging Feature
obj-$(CONFIG_SEC_DEBUG)		+= sec_debug.o sec_getlog.o sec_gaf.o sec_data_param.o
obj-$(CONFIG_SEC_UPLOAD)	+= sec_upload.o
# Samsung Misc Feature
obj-$(CONFIG_SEC_MISC)         += sec_misc.o

obj-$(CONFIG_SEC_HOTPLUG)	+= sec_hotplug.o

# Samsung sec NAD Feature
obj-$(CONFIG_SEC_NAD)		+= sec_nad.o
