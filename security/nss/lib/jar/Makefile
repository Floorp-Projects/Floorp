#! gmake
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include manifest.mn

include $(CORE_DEPTH)/coreconf/config.mk

# NSS_X86 means the target is a 32-bits x86 CPU architecture
# NSS_X64 means the target is a 64-bits x64 CPU architecture
# NSS_X86_OR_X64 means the target is either x86 or x64
ifeq (,$(filter-out i386 x386 x86 x86_64,$(CPU_ARCH)))
        DEFINES += -DNSS_X86_OR_X64
ifdef USE_64
        DEFINES += -DNSS_X64
else
        DEFINES += -DNSS_X86
endif
endif

include $(CORE_DEPTH)/coreconf/rules.mk
