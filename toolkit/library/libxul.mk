# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

EXTRA_DEPS += $(topsrcdir)/toolkit/library/libxul.mk

# dependent libraries
ifdef MOZ_B2G_BT_BLUEZ #{
ifeq (gonk,$(MOZ_WIDGET_TOOLKIT))
OS_LIBS += -ldbus
endif
endif #}

ifdef MOZ_B2G_CAMERA #{
OS_LIBS += -lstagefright -lstagefright_omx
endif #}

ifeq (Linux,$(OS_ARCH))
ifneq (Android,$(OS_TARGET))
OS_LIBS += -lrt
OS_LDFLAGS += -Wl,-version-script,symverscript

symverscript: $(topsrcdir)/toolkit/library/symverscript.in
	$(call py_action,preprocessor, \
		-DVERSION='xul$(MOZILLA_SYMBOLVERSION)' $< -o $@)

EXTRA_DEPS += symverscript
endif
endif

ifeq ($(MOZ_WIDGET_TOOLKIT),cocoa)
OS_LIBS += -lcups
endif

EXTRA_LIBS += \
  $(NSS_LIBS) \
  $(NULL)

OS_LIBS += \
  $(MOZ_CAIRO_OSLIBS) \
  $(MOZ_WEBRTC_X11_LIBS) \
  $(MOZ_APP_EXTRA_LIBS) \
  $(SQLITE_LIBS) \
  $(NULL)

ifdef ENABLE_INTL_API
ifneq (,$(JS_SHARED_LIBRARY)$(MOZ_NATIVE_ICU))
OS_LIBS += $(MOZ_ICU_LIBS)
endif
endif

ifdef MOZ_NATIVE_FFI
OS_LIBS += $(MOZ_FFI_LIBS)
endif

ifdef MOZ_NATIVE_JPEG
OS_LIBS += $(MOZ_JPEG_LIBS)
endif

ifdef MOZ_NATIVE_PNG
OS_LIBS += $(MOZ_PNG_LIBS)
endif

ifdef MOZ_NATIVE_ZLIB
OS_LIBS += $(MOZ_ZLIB_LIBS)
endif

ifdef MOZ_NATIVE_HUNSPELL
OS_LIBS += $(MOZ_HUNSPELL_LIBS)
endif

ifdef MOZ_NATIVE_LIBEVENT
OS_LIBS += $(MOZ_LIBEVENT_LIBS)
endif

ifdef MOZ_NATIVE_LIBVPX
OS_LIBS += $(MOZ_LIBVPX_LIBS)
endif

ifndef MOZ_TREE_PIXMAN
OS_LIBS += $(MOZ_PIXMAN_LIBS)
endif

ifdef MOZ_WEBRTC
ifeq (WINNT,$(OS_TARGET))
ifndef MOZ_HAS_WINSDK_WITH_D3D
OS_LDFLAGS += \
  -LIBPATH:'$(MOZ_DIRECTX_SDK_PATH)/lib/$(MOZ_D3D_CPU_SUFFIX)' \
  $(NULL)
endif
OS_LIBS += $(call EXPAND_LIBNAME,secur32 crypt32 iphlpapi strmiids dmoguids wmcodecdspuuid amstrmid msdmo wininet)
endif
endif

ifdef MOZ_ALSA
OS_LIBS += $(MOZ_ALSA_LIBS)
endif

ifdef HAVE_CLOCK_MONOTONIC
OS_LIBS += $(REALTIME_LIBS)
endif

ifeq (android,$(MOZ_WIDGET_TOOLKIT))
OS_LIBS += -lGLESv2
endif

ifeq (gonk,$(MOZ_WIDGET_TOOLKIT))
OS_LIBS += \
  -lui \
  -lmedia \
  -lhardware_legacy \
  -lhardware \
  -lutils \
  -lcutils \
  -lsysutils \
  -lcamera_client \
  -lsensorservice \
  -lstagefright \
  -lstagefright_foundation \
  -lstagefright_omx \
  -lbinder \
  -lgui \
  -lmtp \
  $(NULL)
endif

ifneq (,$(filter rtsp,$(NECKO_PROTOCOLS)))
OS_LIBS += -lstagefright_foundation
endif

ifdef MOZ_WMF
OS_LIBS += $(call EXPAND_LIBNAME,mfuuid wmcodecdspuuid strmiids)
endif

ifdef MOZ_DIRECTSHOW
OS_LIBS += $(call EXPAND_LIBNAME,dmoguids wmcodecdspuuid strmiids msdmo)
endif

OS_LIBS += $(ICONV_LIBS)

EXTRA_LIBS += $(NSPR_LIBS)

ifeq ($(MOZ_WIDGET_TOOLKIT),cocoa)
OS_LIBS += \
  $(TK_LIBS) \
  $(NULL)
endif

ifeq (OpenBSD,$(OS_ARCH))
OS_LIBS += -lsndio
endif

ifdef MOZ_ENABLE_DBUS
OS_LIBS += $(MOZ_DBUS_GLIB_LIBS)
endif

ifdef MOZ_WIDGET_GTK
ifdef MOZ_ENABLE_GTK3
OS_LIBS += $(filter-out -lgtk-3 -lgdk-3,$(TK_LIBS))
else
OS_LIBS += $(TK_LIBS)
endif
OS_LIBS += $(XLDFLAGS) $(XLIBS) $(XEXT_LIBS) $(XCOMPOSITE_LIBS) $(MOZ_PANGO_LIBS) $(XT_LIBS) -lgthread-2.0
OS_LIBS += $(FT2_LIBS)
endif

ifeq (qt,$(MOZ_WIDGET_TOOLKIT))
OS_LIBS += $(XLDFLAGS) $(XLIBS) $(XT_LIBS) $(MOZ_QT_LIBS)
OS_LIBS += $(FT2_LIBS) $(MOZ_PANGO_LIBS)
endif

ifdef MOZ_TREE_FREETYPE
OS_LIBS += $(FT2_LIBS)
endif

ifdef MOZ_ENABLE_STARTUP_NOTIFICATION
OS_LIBS += $(MOZ_STARTUP_NOTIFICATION_LIBS)
endif

ifdef MOZ_ENABLE_LIBPROXY
OS_LIBS += $(MOZ_LIBPROXY_LIBS)
endif

ifeq ($(OS_ARCH),SunOS)
ifdef GNU_CC
OS_LIBS += -lelf
else
OS_LIBS += -lelf -ldemangle
endif
endif

ifeq ($(OS_ARCH),FreeBSD)
OS_LIBS += $(call EXPAND_LIBNAME,util)
endif

ifeq ($(OS_ARCH),WINNT)
OS_LIBS += $(call EXPAND_LIBNAME,shell32 ole32 version winspool comdlg32 imm32 msimg32 shlwapi psapi ws2_32 dbghelp rasapi32 rasdlg iphlpapi uxtheme setupapi secur32 sensorsapi portabledeviceguids windowscodecs wininet wbemuuid wintrust wtsapi32)
ifdef ACCESSIBILITY
OS_LIBS += $(call EXPAND_LIBNAME,oleacc)
endif
ifdef MOZ_METRO
OS_LIBS += $(call EXPAND_LIBNAME,uiautomationcore runtimeobject)
endif
endif # WINNT

ifdef MOZ_ENABLE_QT
OS_LIBS += $(MOZ_QT_LDFLAGS) $(XEXT_LIBS)
endif

ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
ifdef MOZ_GSTREAMER
OS_LIBS += $(GSTREAMER_LIBS)
endif
endif

# Generate GDB pretty printer-autoload files only on Linux. OSX's GDB is
# too old to support Python pretty-printers; if this changes, we could make
# this 'ifdef GNU_CC'.
ifeq (Linux,$(OS_ARCH))
# Create a GDB Python auto-load file alongside the libxul shared library in
# the build directory.
PP_TARGETS += LIBXUL_AUTOLOAD
LIBXUL_AUTOLOAD = $(topsrcdir)/toolkit/library/libxul.so-gdb.py.in
LIBXUL_AUTOLOAD_FLAGS := -Dtopsrcdir=$(abspath $(topsrcdir))
endif

OS_LIBS += $(LIBICONV)

ifeq ($(MOZ_WIDGET_TOOLKIT),windows)
OS_LIBS += $(call EXPAND_LIBNAME,usp10 oleaut32)
endif

# BFD ld doesn't create multiple PT_LOADs as usual when an unknown section
# exists. Using an implicit linker script to make it fold that section in
# .data.rel.ro makes it create multiple PT_LOADs. That implicit linker
# script however makes gold misbehave, first because it doesn't like that
# the linker script is given after crtbegin.o, and even past that, replaces
# the default section rules with those from the script instead of
# supplementing them. Which leads to a lib with a huge load of sections.
ifdef LD_IS_BFD
OS_LDFLAGS += $(topsrcdir)/toolkit/library/StaticXULComponents.ld
endif

ifeq (WINNT,$(OS_TARGET))
get_first_and_last = dumpbin -exports $1 | grep _NSModule@@ | sort -k 3 | sed -n 's/^.*?\([^@]*\)@@.*$$/\1/;1p;$$p'
else
get_first_and_last = $(TOOLCHAIN_PREFIX)nm -g $1 | grep _NSModule$$ | sort | sed -n 's/^.* _*\([^ ]*\)$$/\1/;1p;$$p'
endif

LOCAL_CHECKS = test "$$($(get_first_and_last) | xargs echo)" != "start_kPStaticModules_NSModule end_kPStaticModules_NSModule" && echo "NSModules are not ordered appropriately" && exit 1 || exit 0

ifeq (Linux,$(OS_ARCH))
LOCAL_CHECKS += ; test "$$($(TOOLCHAIN_PREFIX)readelf -l $1 | awk '$1 == "LOAD" { t += 1 } END { print t }')" -le 1 && echo "Only one PT_LOAD segment" && exit 1 || exit 0
endif
