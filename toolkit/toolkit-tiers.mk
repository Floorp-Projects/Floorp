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
# The Original Code is the Mozilla build system.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation <http://www.mozilla.org/>.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Benjamin Smedberg <benjamin@smedbergs.us> (Initial Code)
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

ifdef LIBXUL_SDK
$(error toolkit-tiers.mk is not compatible with --enable-libxul-sdk=)
endif

include $(topsrcdir)/config/nspr/build.mk
include $(topsrcdir)/js/src/build.mk
include $(topsrcdir)/xpcom/build.mk
include $(topsrcdir)/netwerk/build.mk

TIERS += \
	external \
	gecko \
	toolkit \
	$(NULL)

#
# tier "external" - 3rd party individual libraries
#

ifndef MOZ_NATIVE_JPEG
tier_external_dirs	+= jpeg
endif

# Installer needs standalone libjar, hence standalone zlib
ifdef MOZ_INSTALLER
tier_external_dirs	+= modules/zlib/standalone
endif

ifdef MOZ_UPDATER
tier_external_dirs += modules/libbz2
tier_external_dirs += modules/libmar
endif

#
# tier "gecko" - core components
#

ifdef NS_TRACE_MALLOC
tier_gecko_dirs += tools/trace-malloc/lib
endif

tier_gecko_dirs += \
		js/src/xpconnect \
		intl/chardet \
		$(NULL)

ifdef MOZ_ENABLE_XLIB
tier_gecko_dirs	+= gfx/src/xlibrgb widget/src/xlibxtbin
endif

ifdef MOZ_ENABLE_GTK
tier_gecko_dirs	+= widget/src/gtksuperwin widget/src/gtkxtbin
endif

ifdef MOZ_ENABLE_GTK2
tier_gecko_dirs     += widget/src/gtkxtbin
endif

ifdef MOZ_IPCD
tier_gecko_dirs += ipc/ipcd
endif

tier_gecko_dirs	+= \
		modules/libutil \
		modules/libjar \
		db \
		$(NULL)

ifdef MOZ_PERMISSIONS
tier_gecko_dirs += \
		extensions/cookie \
		extensions/permissions \
		$(NULL)
endif

ifdef MOZ_STORAGE
tier_gecko_dirs += storage
endif

ifdef MOZ_XUL
tier_gecko_dirs += rdf
endif

ifdef MOZ_JSDEBUGGER
tier_gecko_dirs += js/jsd
endif

tier_gecko_dirs	+= \
		uriloader \
		modules/libimg \
		caps \
		parser/expat \
		parser/xml \
		parser/htmlparser \
		gfx \
		modules/libpr0n \
		sun-java \
		modules/plugin \
		dom \
		view \
		widget \
		content \
		editor \
		layout \
		docshell \
		webshell \
		embedding \
		xpfe/appshell \
		$(NULL)

# Java Embedding Plugin
ifneq (,$(filter mac cocoa,$(MOZ_WIDGET_TOOLKIT)))
tier_gecko_dirs += plugin/oji/JEP
endif

ifdef MOZ_XMLEXTRAS
tier_gecko_dirs += extensions/xmlextras
endif

ifdef MOZ_WEBSERVICES
tier_gecko_dirs += extensions/webservices
endif

ifdef MOZ_UNIVERSALCHARDET
tier_gecko_dirs += extensions/universalchardet
endif

ifdef MOZ_OJI
tier_gecko_dirs	+= \
		js/src/liveconnect \
		modules/oji \
		$(NULL)
endif

ifdef ACCESSIBILITY
tier_gecko_dirs    += accessible
endif

# 
# tier "toolkit" - xpfe & toolkit
#
# The division of "gecko" and "toolkit" is somewhat arbitrary, and related
# to history where "gecko" wasn't forked between seamonkey/firefox but
# "toolkit" was.
#

ifdef MOZ_XUL_APP
tier_toolkit_dirs += chrome
else
ifdef MOZ_XUL
tier_toolkit_dirs += rdf/chrome
else
tier_toolkit_dirs += embedding/minimo/chromelite
endif
endif

tier_toolkit_dirs += profile

# This must preceed xpfe
ifdef MOZ_JPROF
tier_toolkit_dirs        += tools/jprof
endif

ifneq (,$(filter mac cocoa,$(MOZ_WIDGET_TOOLKIT)))
tier_toolkit_dirs       += xpfe/bootstrap/appleevents
endif

tier_toolkit_dirs	+= \
	xpfe \
	toolkit/components \
	$(NULL)

ifndef MOZ_XUL_APP
tier_toolkit_dirs += themes
endif

ifdef MOZ_ENABLE_XREMOTE
tier_toolkit_dirs += widget/src/xremoteclient
endif

ifdef MOZ_SPELLCHECK
tier_toolkit_dirs	+= extensions/spellcheck
endif

ifdef MOZ_XUL_APP
tier_toolkit_dirs	+= toolkit
endif

ifdef MOZ_XPINSTALL
tier_toolkit_dirs     +=  xpinstall
endif

ifdef MOZ_PSM
tier_toolkit_dirs	+= security/manager
else
tier_toolkit_dirs	+= security/manager/boot/public security/manager/ssl/public
endif

ifdef MOZ_PREF_EXTENSIONS
tier_toolkit_dirs += extensions/pref
endif

ifdef MOZ_JAVAXPCOM
tier_toolkit_dirs += extensions/java
endif

ifndef BUILD_STATIC_LIBS
ifdef MOZ_XUL_APP
ifneq (,$(MOZ_ENABLE_GTK)$(MOZ_ENABLE_GTK2))
tier_toolkit_dirs += embedding/browser/gtk
endif
endif
endif

ifdef MOZ_XUL_APP
ifndef BUILD_STATIC_LIBS
tier_toolkit_dirs += toolkit/library
endif
endif

ifdef MOZ_ENABLE_LIBXUL
tier_toolkit_dirs += xpcom/stub
endif

ifdef NS_TRACE_MALLOC
tier_toolkit_dirs += tools/trace-malloc
endif

ifdef MOZ_LDAP_XPCOM
tier_toolkit_staticdirs += directory/c-sdk
tier_toolkit_dirs	+= directory/xpcom
endif

ifdef MOZ_ENABLE_GNOME_COMPONENT
tier_toolkit_dirs    += toolkit/system/gnome
endif

ifdef MOZ_ENABLE_DBUS
tier_toolkit_dirs    += toolkit/system/dbus
endif

ifdef MOZ_LEAKY
tier_toolkit_dirs        += tools/leaky
endif

ifdef MOZ_MAPINFO
tier_toolkit_dirs	+= tools/codesighs
endif

ifdef MOZ_MOCHITEST
tier_toolkit_dirs	+= testing/mochitest
endif
