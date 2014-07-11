# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifeq ($(MOZ_WIDGET_TOOLKIT),cocoa)
# This is going to be a framework named "XUL", not an ordinary library named
# "libxul.dylib"
SHARED_LIBRARY_NAME=XUL
# Setting MAKE_FRAMEWORK makes DLL_PREFIX and DLL_SUFFIX be ignored when
# setting SHARED_LIBRARY; we need to leave DLL_PREFIX and DLL_SUFFIX
# as-is so that dependencies of the form -ltracemalloc still work.
MAKE_FRAMEWORK=1
else
SHARED_LIBRARY_NAME=xul
endif

SHARED_LIBRARY_LIBS += $(call EXPAND_LIBNAME_PATH,xul,$(DEPTH)/toolkit/library)

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
EXTRA_DSO_LDOPTS += -Wl,-version-script,symverscript

symverscript: $(topsrcdir)/toolkit/library/symverscript.in
	$(call py_action,preprocessor, \
		-DVERSION='$(SHARED_LIBRARY_NAME)$(MOZILLA_SYMBOLVERSION)' $< -o $@)

EXTRA_DEPS += symverscript
endif
endif

ifeq ($(MOZ_WIDGET_TOOLKIT),cocoa)
OS_LIBS += -lcups
endif

EXTRA_DSO_LDOPTS += \
  $(LIBS_DIR) \
  $(MOZ_JS_LIBS) \
  $(NSS_LIBS) \
  $(MOZ_CAIRO_OSLIBS) \
  $(MOZ_APP_EXTRA_LIBS) \
  $(SQLITE_LIBS) \
  $(NULL)

ifdef ENABLE_INTL_API
ifdef JS_SHARED_LIBRARY
EXTRA_DSO_LDOPTS += $(MOZ_ICU_LIBS)
endif
endif

ifdef MOZ_NATIVE_JPEG
EXTRA_DSO_LDOPTS += $(MOZ_JPEG_LIBS)
endif

ifdef MOZ_NATIVE_PNG
EXTRA_DSO_LDOPTS += $(MOZ_PNG_LIBS)
endif

ifndef ZLIB_IN_MOZGLUE
EXTRA_DSO_LDOPTS += $(MOZ_ZLIB_LIBS)
endif

ifdef MOZ_NATIVE_HUNSPELL
EXTRA_DSO_LDOPTS += $(MOZ_HUNSPELL_LIBS)
endif

ifdef MOZ_NATIVE_LIBEVENT
EXTRA_DSO_LDOPTS += $(MOZ_LIBEVENT_LIBS)
endif

ifdef MOZ_NATIVE_LIBVPX
EXTRA_DSO_LDOPTS += $(MOZ_LIBVPX_LIBS)
endif

ifndef MOZ_TREE_PIXMAN
EXTRA_DSO_LDOPTS += $(MOZ_PIXMAN_LIBS)
endif

ifdef MOZ_DMD
EXTRA_DSO_LDOPTS += $(call EXPAND_LIBNAME_PATH,dmd,$(DIST)/lib)
endif

EXTRA_DSO_LDOPTS += $(call EXPAND_LIBNAME_PATH,gkmedias,$(DEPTH)/layout/media)

ifdef MOZ_WEBRTC
ifeq (WINNT,$(OS_TARGET))
ifndef MOZ_HAS_WINSDK_WITH_D3D
EXTRA_DSO_LDOPTS += \
  -LIBPATH:'$(MOZ_DIRECTX_SDK_PATH)/lib/$(MOZ_D3D_CPU_SUFFIX)' \
  $(NULL)
endif
OS_LIBS += $(call EXPAND_LIBNAME,secur32 crypt32 iphlpapi strmiids dmoguids wmcodecdspuuid amstrmid msdmo wininet)
endif
endif

ifdef MOZ_ALSA
EXTRA_DSO_LDOPTS += $(MOZ_ALSA_LIBS)
endif

ifdef HAVE_CLOCK_MONOTONIC
EXTRA_DSO_LDOPTS += $(REALTIME_LIBS)
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

EXTRA_DSO_LDOPTS += $(LIBS_DIR)

EXTRA_DSO_LDOPTS += $(NSPR_LIBS) $(MOZALLOC_LIB)

ifeq ($(MOZ_WIDGET_TOOLKIT),cocoa)
OS_LIBS += \
  $(TK_LIBS) \
  $(NULL)
endif

ifeq (OpenBSD,$(OS_ARCH))
EXTRA_DSO_LDOPTS += -lsndio
endif

ifdef MOZ_ENABLE_DBUS
EXTRA_DSO_LDOPTS += $(MOZ_DBUS_GLIB_LIBS)
endif

ifdef MOZ_WIDGET_GTK
ifdef MOZ_ENABLE_GTK3
EXTRA_DSO_LDOPTS += $(filter-out -lgtk-3 -lgdk-3,$(TK_LIBS)) -lmozgtk_stub
else
EXTRA_DSO_LDOPTS += $(TK_LIBS)
endif
EXTRA_DSO_LDOPTS += $(XLDFLAGS) $(XLIBS) $(XEXT_LIBS) $(XCOMPOSITE_LIBS) $(MOZ_PANGO_LIBS) $(XT_LIBS) -lgthread-2.0
EXTRA_DSO_LDOPTS += $(FT2_LIBS)
endif

ifeq (qt,$(MOZ_WIDGET_TOOLKIT))
EXTRA_DSO_LDOPTS += $(XLDFLAGS) $(XLIBS) $(XT_LIBS) $(MOZ_QT_LIBS)
EXTRA_DSO_LDOPTS += $(FT2_LIBS) $(MOZ_PANGO_LIBS)
endif

ifdef MOZ_TREE_FREETYPE
EXTRA_DSO_LDOPTS += $(FT2_LIBS)
endif

ifdef MOZ_ENABLE_STARTUP_NOTIFICATION
EXTRA_DSO_LDOPTS += $(MOZ_STARTUP_NOTIFICATION_LIBS)
endif

ifdef MOZ_ENABLE_LIBPROXY
EXTRA_DSO_LDOPTS += $(MOZ_LIBPROXY_LIBS)
endif

ifeq ($(OS_ARCH),SunOS)
ifdef GNU_CC
EXTRA_DSO_LDOPTS += -lelf
else
EXTRA_DSO_LDOPTS += -lelf -ldemangle
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

ifdef MOZ_JPROF
EXTRA_DSO_LDOPTS += -ljprof
endif

ifdef MOZ_ENABLE_QT
EXTRA_DSO_LDOPTS += $(MOZ_QT_LDFLAGS) $(XEXT_LIBS)
endif

ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
ifdef MOZ_GSTREAMER
EXTRA_DSO_LDOPTS += $(GSTREAMER_LIBS)
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

EXTRA_DSO_LDOPTS += $(call EXPAND_LIBNAME_PATH,StaticXULComponentsEnd,$(DEPTH)/toolkit/library/StaticXULComponentsEnd)

# BFD ld doesn't create multiple PT_LOADs as usual when an unknown section
# exists. Using an implicit linker script to make it fold that section in
# .data.rel.ro makes it create multiple PT_LOADs. That implicit linker
# script however makes gold misbehave, first because it doesn't like that
# the linker script is given after crtbegin.o, and even past that, replaces
# the default section rules with those from the script instead of
# supplementing them. Which leads to a lib with a huge load of sections.
ifdef LD_IS_BFD
EXTRA_DSO_LDOPTS += $(topsrcdir)/toolkit/library/StaticXULComponents.ld
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
