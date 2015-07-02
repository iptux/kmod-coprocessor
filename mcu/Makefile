include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=mcu
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/mcu
  SUBMENU:=Other modules
  TITLE:=Use MCU as coprocessor
#  DEPENDS:=@GPIO_SUPPORT +kmod-i2c-core +kmod-i2c-gpio
  FILES:=$(PKG_BUILD_DIR)/mcu/mcu.ko
  MENU:=1
  KCONFIG:=
  AUTOLOAD:=$(call AutoLoad,50,mcu)
endef

define KernelPackage/mcu/config
 source "$(SOURCE)/Config.in"
endef

EXTRA_KCONFIG:= \
	MCU_LDISC \
	MCU_CORE \
	MCU_GPIO \
	MCU_OLED \
	MCU_BATTERY \

MAKE_OPTS:= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)" \
	SUBDIRS="$(PKG_BUILD_DIR)" \
	EXTRA_CFLAGS="-I$(PKG_BUILD_DIR)/include $(foreach c, $(EXTRA_KCONFIG),$(if $(CONFIG_$c),-DCONFIG_$(c)=y))" \
	$(foreach c, $(EXTRA_KCONFIG),$(if $(CONFIG_$c),CONFIG_$(c)=y)) \
	CONFIG_MCU=m

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
	true
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

$(eval $(call KernelPackage,mcu))
