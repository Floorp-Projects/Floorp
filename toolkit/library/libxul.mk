# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

EXTRA_DEPS += $(topsrcdir)/toolkit/library/libxul.mk

ifeq (Linux,$(OS_ARCH))
ifneq (Android,$(OS_TARGET))
OS_LDFLAGS += -Wl,-version-script,symverscript

symverscript: $(topsrcdir)/toolkit/library/symverscript.in
	$(call py_action,preprocessor, \
		-DVERSION='xul$(MOZILLA_SYMBOLVERSION)' $< -o $@)

EXTRA_DEPS += symverscript
endif
endif

ifdef MOZ_WEBRTC
ifeq (WINNT,$(OS_TARGET))
ifndef MOZ_HAS_WINSDK_WITH_D3D
OS_LDFLAGS += \
  -LIBPATH:'$(MOZ_DIRECTX_SDK_PATH)/lib/$(MOZ_D3D_CPU_SUFFIX)' \
  $(NULL)
endif
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

# BFD ld doesn't create multiple PT_LOADs as usual when an unknown section
# exists. Using an implicit linker script to make it fold that section in
# .data.rel.ro makes it create multiple PT_LOADs. That implicit linker
# script however makes gold misbehave, first because it doesn't like that
# the linker script is given after crtbegin.o, and even past that, replaces
# the default section rules with those from the script instead of
# supplementing them. Which leads to a lib with a huge load of sections.
ifneq (OpenBSD,$(OS_TARGET))
ifdef LD_IS_BFD
OS_LDFLAGS += $(topsrcdir)/toolkit/library/StaticXULComponents.ld
endif
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
