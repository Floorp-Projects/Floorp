#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla libxul
#
# The Initial Developer of the Original Code is
# Benjamin Smedberg <benjamin@smedbergs.us>
#
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

CPPSRCS += \
	nsStaticXULComponents.cpp \
	$(NULL)

ifeq ($(OS_ARCH)_$(GNU_CC),WINNT_)
REQUIRES += libreg widget gfx

CPPSRCS += \
	dlldeps.cpp \
	dlldeps-obs.cpp \
	nsGFXDeps.cpp \
	nsDllMain.cpp \
	$(NULL)

ifndef MOZ_NATIVE_ZLIB
CPPSRCS += dlldeps-zlib.cpp
DEFINES += -DZLIB_INTERNAL
endif

LOCAL_INCLUDES += -I$(topsrcdir)/widget/src/windows
endif

ifeq ($(OS_ARCH),WINNT)
DEFINES	+= -DZLIB_DLL=1
endif

# dependent libraries
STATIC_LIBS += \
	xpcom_core \
	xpcom_compat \
	unicharutil_s \
	ucvutil_s \
	gkgfx \
	gfxshared_s \
	$(NULL)

# component libraries
COMPONENT_LIBS += \
	xpcom_compat_c \
	xpconnect \
	necko \
	uconv \
	i18n \
	universalchardet \
	jar$(VERSION_NUMBER) \
	pref \
	caps \
	rdf \
	htmlpars \
	imglib2 \
	gkplugin \
	gklayout \
	xmlextras \
	docshell \
	embedcomponents \
	webbrwsr \
	editor \
	nsappshell \
	txmgr \
	composer \
	chrome \
	mork \
	mozfind \
	appcomps \
	commandlines \
	tkautocomplete \
	toolkitcomps \
	xpinstall \
	jsd \
	pippki \
	pipboot \
	pipnss \
	autoconfig \
	$(NULL)

ifeq ($(OS_ARCH),WINNT)
COMPONENT_LIBS += intlcmpt
endif

ifdef MOZ_MATHML
COMPONENT_LIBS += ucvmath
endif

ifneq (,$(filter xlib,$(MOZ_WIDGET_TOOLKIT))$(MOZ_ENABLE_XLIB)$(MOZ_ENABLE_XPRINT))
STATIC_LIBS += xlibrgb
endif

ifdef MOZ_ENABLE_XPRINT
DEFINES += -DMOZ_ENABLE_XPRINT
STATIC_LIBS += xprintutil
COMPONENTS_LIBS += gfxxprint
endif

ifdef MOZ_ENABLE_XLIB
STATIC_LIBS += xlibxtbin
endif

ifdef MOZ_ENABLE_GTK
STATIC_LIBS += gtksuperwin
COMPONENT_LIBS += widget_gtk
endif

ifdef MOZ_ENABLE_GTK2
COMPONENT_LIBS += widget_gtk2
COMPONENT_LIBS += system-pref
endif

ifneq (,$(MOZ_ENABLE_GTK)$(MOZ_ENABLE_GTK2))
STATIC_LIBS += gtkxtbin
endif

ifdef MOZ_IPCD
DEFINES += -DMOZ_IPCD
COMPONENT_LIBS += ipcdc
endif

ifdef MOZ_ENABLE_POSTSCRIPT
DEFINES += -DMOZ_ENABLE_POSTSCRIPT
STATIC_LIBS += gfxpsshar
COMPONENT_LIBS += gfxps
endif
ifneq (,$(filter icon,$(MOZ_IMG_DECODERS)))
ifndef MOZ_ENABLE_GNOMEUI
DEFINES += -DICON_DECODER
COMPONENT_LIBS += imgicon
endif
endif
ifeq (windows,$(MOZ_WIDGET_TOOLKIT))
COMPONENT_LIBS += gkgfxwin gkwidget
endif
ifeq (beos,$(MOZ_WIDGET_TOOLKIT))
COMPONENT_LIBS += gfx_beos widget_beos
endif
ifeq (os2,$(MOZ_WIDGET_TOOLKIT))
COMPONENT_LIBS += gfx_os2 wdgtos2
endif
ifneq (,$(filter mac cocoa,$(MOZ_WIDGET_TOOLKIT)))
COMPONENT_LIBS += gfx_mac widget_mac
endif
ifeq (qt,$(MOZ_WIDGET_TOOLKIT))
COMPONENT_LIBS += widget_qt
endif

ifdef MOZ_ENABLE_CAIRO_GFX
DEFINES += -DMOZ_ENABLE_CAIRO_GFX
COMPONENT_LIBS += gkgfxcairo
else
  ifneq (,$(MOZ_ENABLE_GTK)$(MOZ_ENABLE_GTK2))
  COMPONENT_LIBS += gfx_gtk
  endif
  ifdef MOZ_ENABLE_QT
  COMPONENT_LIBS += gfx_qt
  endif
  ifdef MOZ_ENABLE_XLIB
  COMPONENT_LIBS += gfx_xlib
  endif
  ifdef MOZ_ENABLE_PHOTON
  COMPONENT_LIBS += gfx_photon
  endif
endif

ifdef MOZ_ENABLE_XLIB
COMPONENT_LIBS += widget_xlib
endif
ifdef MOZ_ENABLE_PHOTON
COMPONENT_LIBS += widget_photon
endif

ifdef MOZ_OJI
STATIC_LIBS += jsj
COMPONENT_LIBS += oji
endif

ifdef ACCESSIBILITY
COMPONENT_LIBS += accessibility
endif

ifdef MOZ_ENABLE_XREMOTE
COMPONENT_LIBS += remoteservice
endif

ifdef GC_LEAK_DETECTOR
EXTRA_DSO_LIBS += boehm
endif

ifdef NS_TRACE_MALLOC
EXTRA_DSO_LIBS += tracemalloc
endif

ifneq (,$(filter mac cocoa,$(MOZ_WIDGET_TOOLKIT)))
EXTRA_DSO_LIBS += macmorefiles_s
EXTRA_DEPS += $(DIST)/lib/$(LIB_PREFIX)macmorefiles_s.$(LIB_SUFFIX)
endif

ifdef MOZ_JAVAXPCOM
LOCAL_INCLUDES += \
		-I$(topsrcdir)/extensions/java/xpcom/src \
		-I$(JAVA_INCLUDE_PATH) \
		$(NULL)
ifeq ($(OS_ARCH),WINNT)
CPPSRCS += dlldeps-javaxpcom.cpp
LOCAL_INCLUDES += -I$(JAVA_INCLUDE_PATH)/win32
else
LOCAL_INCLUDES += -I$(JAVA_INCLUDE_PATH)/linux
endif
STATIC_LIBS += javaxpcom
endif

