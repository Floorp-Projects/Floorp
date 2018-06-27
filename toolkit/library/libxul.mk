# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

EXTRA_DEPS += $(topsrcdir)/toolkit/library/libxul.mk

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
