# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifdef LIBXUL_SDK
$(error toolkit-tiers.mk is not compatible with --enable-libxul-sdk=)
endif

include $(topsrcdir)/config/nspr/build.mk
include $(topsrcdir)/config/js/build.mk

TIERS += platform

ifdef NS_TRACE_MALLOC
tier_platform_dirs = tools/trace-malloc/lib
endif

ifdef MOZ_TREE_FREETYPE
tier_platform_staticdirs += modules/freetype2
endif

# this must precede xpcom
ifdef MOZ_DMDV
tier_platform_dirs += tools/dmdv
endif

tier_platform_dirs += xpcom

tier_platform_dirs += \
		modules/libpref \
		intl \
		netwerk \
		$(NULL)

ifdef MOZ_AUTH_EXTENSION
tier_platform_dirs += extensions/auth
endif

#
# "external" - 3rd party individual libraries
#

ifndef MOZ_NATIVE_JPEG
tier_platform_dirs	+= media/libjpeg
endif

ifdef MOZ_UPDATER
ifndef MOZ_NATIVE_BZ2
tier_platform_dirs += modules/libbz2
endif
tier_platform_dirs += other-licenses/bsdiff
endif

tier_platform_dirs	+= gfx/qcms

#
# "gecko" - core components
#

tier_platform_dirs += ipc js/ipc

tier_platform_dirs += \
		hal \
		js/xpconnect \
		intl/chardet \
		$(NULL)

ifdef MOZ_ENABLE_GTK2
ifdef MOZ_X11
tier_platform_dirs     += widget/gtkxtbin
endif
endif

tier_platform_dirs	+= \
		modules/libjar \
		storage \
		$(NULL)

ifdef MOZ_PERMISSIONS
tier_platform_dirs += \
		extensions/cookie \
		extensions/permissions \
		$(NULL)
endif

tier_platform_dirs += rdf

ifdef MOZ_JSDEBUGGER
tier_platform_dirs += js/jsd
endif

ifdef MOZ_VORBIS
tier_platform_dirs += \
		media/libvorbis \
		$(NULL)
endif

ifdef MOZ_TREMOR
tier_platform_dirs += \
		media/libtremor \
		$(NULL)
endif

ifdef MOZ_OPUS
tier_platform_dirs += \
  media/libopus \
  $(NULL)
endif

ifdef MOZ_WEBM
tier_platform_dirs += media/libnestegg
endif

ifdef MOZ_VP8
ifndef MOZ_NATIVE_LIBVPX
tier_platform_dirs += media/libvpx
endif
endif

ifdef MOZ_OGG
tier_platform_dirs += \
		media/libogg \
		media/libtheora \
		$(NULL)
endif

ifdef MOZ_SYDNEYAUDIO
tier_platform_dirs += \
		media/libsydneyaudio \
		$(NULL)
endif

ifdef MOZ_PSM
tier_platform_dirs += \
  security/build \
  $(NULL)
endif

ifdef MOZ_WEBRTC
tier_platform_dirs += \
  media/webrtc \
  media/mtransport/third_party \
  media/mtransport/build \
  media/mtransport/standalone \
  $(NULL)
endif

ifdef MOZ_SPEEX_RESAMPLER
tier_platform_dirs += \
		media/libspeex_resampler \
		$(NULL)
endif

ifdef MOZ_SOUNDTOUCH
tier_platform_dirs += \
		media/libsoundtouch \
		$(NULL)
endif

ifdef MOZ_CUBEB
tier_platform_dirs += \
		media/libcubeb \
		$(NULL)
endif

ifdef MOZ_OMX_PLUGIN
tier_platform_dirs += \
		media/omx-plugin/lib/ics/libutils \
		media/omx-plugin/lib/ics/libstagefright \
		media/omx-plugin/lib/gb/libutils \
		media/omx-plugin/lib/gb/libstagefright \
		media/omx-plugin/lib/gb235/libstagefright \
		media/omx-plugin \
		media/omx-plugin/gb \
		media/omx-plugin/gb235 \
		media/omx-plugin/lib/hc/libstagefright \
		media/omx-plugin/hc \
		$(NULL)
endif

ifndef MOZ_NATIVE_PNG
tier_platform_dirs += media/libpng
endif

ifdef ENABLE_TESTS
tier_platform_dirs += testing/specialpowers
endif

tier_platform_dirs	+= \
		uriloader \
		caps \
		parser \
		gfx \
		image \
		dom \
		view \
		widget \
		content \
		editor \
		layout \
		docshell \
		embedding \
		xpfe/appshell \
		$(NULL)

# This needs to be built after the gfx/ directory
# to ensure all dependencies for skia (e.g. mozalloc, xpcom)
# have been built
ifeq (android,$(MOZ_WIDGET_TOOLKIT))
tier_platform_dirs += other-licenses/skia-npapi
endif

ifdef MOZ_UNIVERSALCHARDET
tier_platform_dirs += extensions/universalchardet
endif

ifdef ACCESSIBILITY
tier_platform_dirs    += accessible
endif

#
# "toolkit" - xpfe & toolkit
#

tier_platform_dirs += profile

# This must precede xpfe
ifdef MOZ_JPROF
tier_platform_dirs        += tools/jprof
endif

tier_platform_dirs  += tools/profiler

tier_platform_dirs	+= xpfe/components

ifdef MOZ_ENABLE_XREMOTE
tier_platform_dirs += widget/xremoteclient
endif

ifdef MOZ_SPELLCHECK
tier_platform_dirs	+= extensions/spellcheck
endif

ifdef MOZ_PSM
tier_platform_dirs	+= security/manager
else
tier_platform_dirs	+= security/manager/boot/public security/manager/ssl/public
endif

ifdef MOZ_UPDATER
tier_platform_dirs += modules/libmar
endif

tier_platform_dirs	+= toolkit

ifdef MOZ_PREF_EXTENSIONS
tier_platform_dirs += extensions/pref
endif

tier_platform_dirs += services/crypto/component

tier_platform_dirs += startupcache

tier_platform_dirs += js/ductwork/debugger

tier_platform_dirs += other-licenses/snappy

ifdef MOZ_GIO_COMPONENT
tier_platform_dirs += extensions/gio
endif

ifdef APP_LIBXUL_STATICDIRS
# Applications can cheat and ask for code to be
# built before libxul so libxul can be linked against it.
tier_platform_staticdirs += $(APP_LIBXUL_STATICDIRS)
endif
ifdef APP_LIBXUL_DIRS
# Applications can cheat and ask for code to be
# built before libxul so it can be linked into libxul.
tier_platform_dirs += $(APP_LIBXUL_DIRS)
endif

tier_platform_dirs += toolkit/library

tier_platform_dirs += xpcom/stub

ifdef MOZ_REPLACE_MALLOC
tier_platform_dirs += memory/replace
endif

ifdef NS_TRACE_MALLOC
tier_platform_dirs += tools/trace-malloc
endif

ifdef MOZ_ENABLE_GNOME_COMPONENT
tier_platform_dirs    += toolkit/system/gnome
endif

ifndef MOZ_ENABLE_LIBCONIC
# if libconic is present, it will do its own network monitoring
ifdef MOZ_ENABLE_DBUS
tier_platform_dirs    += toolkit/system/dbus
endif
endif

ifdef MOZ_MAPINFO
tier_platform_dirs	+= tools/codesighs
endif

ifdef ENABLE_MARIONETTE
tier_platform_dirs += testing/marionette
endif

ifdef ENABLE_TESTS
tier_platform_dirs += testing/mochitest
tier_platform_dirs += testing/xpcshell
tier_platform_dirs += testing/tools/screenshot
tier_platform_dirs += testing/peptest
tier_platform_dirs += testing/mozbase
ifdef MOZ_WEBRTC
tier_platform_dirs += media/webrtc/signaling/test
tier_platform_dirs += media/mtransport/test
endif
endif
