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
	nsGFXDeps.cpp \
	nsDllMain.cpp \
	$(NULL)

RESFILE = xulrunner.res

ifndef MOZ_NATIVE_ZLIB
CPPSRCS += dlldeps-zlib.cpp
DEFINES += -DZLIB_INTERNAL
endif

LOCAL_INCLUDES += -I$(topsrcdir)/widget/src/windows
endif

ifneq (,$(filter WINNT OS2,$(OS_ARCH)))
DEFINES	+= -DZLIB_DLL=1
endif

ifeq ($(OS_ARCH),OS2)
REQUIRES += libreg widget gfx

CPPSRCS += \
	dlldeps.cpp \
	nsGFXDeps.cpp \
	$(NULL)

ifndef MOZ_NATIVE_ZLIB
CPPSRCS += dlldeps-zlib.cpp
DEFINES += -DZLIB_INTERNAL
endif

LOCAL_INCLUDES += -I$(topsrcdir)/widget/src/os2
endif

# dependent libraries
STATIC_LIBS += \
	xpcom_core \
	mozreg_s \
	ucvutil_s \
	gkgfx \
	gfxshared_s \
	$(NULL)

# component libraries
COMPONENT_LIBS += \
	xpconnect \
	necko \
	uconv \
	i18n \
	chardet \
	jar$(VERSION_NUMBER) \
	pref \
	caps \
	htmlpars \
	imglib2 \
	gklayout \
	xmlextras \
	docshell \
	embedcomponents \
	webbrwsr \
	nsappshell \
	txmgr \
	chrome \
	commandlines \
	toolkitcomps \
	pipboot \
	pipnss \
	$(NULL)

ifdef MOZ_PLUGINS
DEFINES += -DMOZ_PLUGINS
COMPONENT_LIBS += \
	gkplugin \
	$(NULL)
endif

ifdef MOZ_XPFE_COMPONENTS
DEFINES += -DMOZ_XPFE_COMPONENTS
COMPONENT_LIBS += \
	mozfind \
	appcomps \
	$(NULL)
endif

ifdef MOZ_XPINSTALL
DEFINES += -DMOZ_XPINSTALL
COMPONENT_LIBS += \
	xpinstall \
	$(NULL)
endif

ifdef MOZ_JSDEBUGGER
DEFINES += -DMOZ_JSDEBUGGER
COMPONENT_LIBS += \
	jsd \
	$(NULL)
endif

ifdef MOZ_PREF_EXTENSIONS
DEFINES += -DMOZ_PREF_EXTENSIONS
COMPONENT_LIBS += \
	autoconfig \
	$(NULL)
endif

ifdef MOZ_WEBSERVICES
DEFINES += -DMOZ_WEBSERVICES
COMPONENT_LIBS += \
	websrvcs \
	$(NULL)
endif

ifdef MOZ_AUTH_EXTENSION
COMPONENT_LIBS += auth
DEFINES += -DMOZ_AUTH_EXTENSION
endif

ifdef MOZ_PERMISSIONS
COMPONENT_LIBS += cookie permissions
DEFINES += -DMOZ_PERMISSIONS
endif

ifdef MOZ_UNIVERSALCHARDET
COMPONENT_LIBS += universalchardet
DEFINES += -DMOZ_UNIVERSALCHARDET
endif

ifndef MOZ_PLAINTEXT_EDITOR_ONLY
COMPONENT_LIBS += composer
else
DEFINES += -DMOZ_PLAINTEXT_EDITOR_ONLY
endif

ifdef MOZ_RDF
COMPONENT_LIBS += \
	rdf \
	$(NULL)
ifdef MOZ_XPFE_COMPONENTS
COMPONENT_LIBS += \
	windowds \
	intlapp \
	$(NULL)
endif
endif

ifeq (,$(filter beos os2 mac photon cocoa windows,$(MOZ_WIDGET_TOOLKIT)))
ifdef MOZ_RDF
ifdef MOZ_XPFE_COMPONENTS
COMPONENT_LIBS += fileview
DEFINES += -DMOZ_FILEVIEW
endif
endif
endif

ifdef MOZ_STORAGE
COMPONENT_LIBS += storagecomps
endif

ifdef MOZ_PLACES
STATIC_LIBS += morkreader_s

COMPONENT_LIBS += \
	places \
	$(NULL)
else
ifdef MOZ_MORK
ifdef MOZ_XUL
COMPONENT_LIBS += \
	mork \
	tkhstory \
	$(NULL)
endif
endif
endif

ifdef MOZ_XUL
COMPONENT_LIBS += \
	tkautocomplete \
	satchel \
	pippki \
	$(NULL)
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
ifdef MOZ_PREF_EXTENSIONS
COMPONENT_LIBS += system-pref
endif
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
endif
ifneq (,$(filter icon,$(MOZ_IMG_DECODERS)))
ifndef MOZ_ENABLE_GNOMEUI
DEFINES += -DICON_DECODER
COMPONENT_LIBS += imgicon
endif
endif

ifdef MOZ_ENABLE_CAIRO_GFX
STATIC_LIBS += thebes
COMPONENT_LIBS += gkgfxthebes

else # Platform-specific GFX layer
  ifeq (windows,$(MOZ_WIDGET_TOOLKIT))
  COMPONENT_LIBS += gkgfxwin
  endif
  ifeq (beos,$(MOZ_WIDGET_TOOLKIT))
  COMPONENT_LIBS += gfx_beos
  endif
  ifeq (os2,$(MOZ_WIDGET_TOOLKIT))
  COMPONENT_LIBS += gfx_os2
  endif
  ifneq (,$(filter mac cocoa,$(MOZ_WIDGET_TOOLKIT)))
  COMPONENT_LIBS += gfx_mac
  endif
  ifeq (qt,$(MOZ_WIDGET_TOOLKIT))
  COMPONENT_LIBS += widget_qt
  endif
  ifneq (,$(filter gtk gtk2,$(MOZ_WIDGET_TOOLKIT)))
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

ifeq (windows,$(MOZ_WIDGET_TOOLKIT))
COMPONENT_LIBS += gkwidget
endif
ifeq (beos,$(MOZ_WIDGET_TOOLKIT))
COMPONENT_LIBS += widget_beos
endif
ifeq (os2,$(MOZ_WIDGET_TOOLKIT))
COMPONENT_LIBS += wdgtos2
endif
ifneq (,$(filter mac cocoa,$(MOZ_WIDGET_TOOLKIT)))
COMPONENT_LIBS += widget_mac
endif
ifeq (qt,$(MOZ_WIDGET_TOOLKIT))
COMPONENT_LIBS += widget_qt
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

ifdef MOZ_SPELLCHECK
DEFINES += -DMOZ_SPELLCHECK
COMPONENT_LIBS += spellchecker
endif

ifdef GC_LEAK_DETECTOR
EXTRA_DSO_LIBS += boehm
endif

ifdef NS_TRACE_MALLOC
EXTRA_DSO_LIBS += tracemalloc
endif
