##
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##



PORTS_SRCS-yes += aom_ports.mk

PORTS_SRCS-yes += bitops.h
PORTS_SRCS-yes += mem.h
PORTS_SRCS-yes += msvc.h
PORTS_SRCS-yes += system_state.h
PORTS_SRCS-yes += aom_timer.h

ifeq ($(ARCH_X86)$(ARCH_X86_64),yes)
PORTS_SRCS-yes += emms.asm
PORTS_SRCS-yes += x86.h
PORTS_SRCS-yes += x86_abi_support.asm
endif

PORTS_SRCS-$(ARCH_ARM) += arm_cpudetect.c
PORTS_SRCS-$(ARCH_ARM) += arm.h
