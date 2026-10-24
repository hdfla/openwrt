# SPDX-License-Identifier: GPL-2.0-only

include $(TOPDIR)/rules.mk

PKG_VERSION:=1.0.0
PKG_RELEASE:=1
LUCI_TITLE:=LuCI support for Linux xPON, OMCI and EPON
LUCI_PKGARCH:=all
PKG_MAINTAINER:=Matheus Sampaio Queiroga <srherobrine20@gmail.com>
PKG_LICENSE:=GPL-2.0-only
LUCI_DEPENDS:=+luci-base +xpon-agent @TARGET_airoha

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
