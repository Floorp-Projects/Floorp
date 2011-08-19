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
# The Original Code is the MPI Arbitrary Precision Integer Arithmetic library.
#
# The Initial Developer of the Original Code is
# Michael J. Fromberger <sting@linguist.dartmouth.edu>.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Netscape Communications Corporation
#   Richard C. Swift  (swift@netscape.com)
#   Douglas Stebila <douglas@stebila.ca>, Sun Microsystems Laboratories
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

##
## Define CFLAGS to contain any local options your compiler
## setup requires.
##
## Conditional compilation options are no longer here; see
## the file 'mpi-config.h' instead.
##
MPICMN = -I. -DMP_API_COMPATIBLE -DMP_IOFUNC
CFLAGS= -O $(MPICMN)
#CFLAGS=-ansi -fullwarn -woff 1521 -O3 $(MPICMN)
#CFLAGS=-ansi -pedantic -Wall -O3 $(MPICMN)
#CFLAGS=-ansi -pedantic -Wall -g -O2 -DMP_DEBUG=1 $(MPICMN)

ifeq ($(TARGET),mipsIRIX)
#IRIX
#MPICMN += -DMP_MONT_USE_MP_MUL 
MPICMN += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE
MPICMN += -DMP_USE_UINT_DIGIT
#MPICMN += -DMP_NO_MP_WORD
AS_OBJS = mpi_mips.o
#ASFLAGS = -O -OPT:Olimit=4000 -dollar -fullwarn -xansi -n32 -mips3 -exceptions
ASFLAGS = -O -OPT:Olimit=4000 -dollar -fullwarn -xansi -n32 -mips3 
#CFLAGS=-ansi -n32 -O3 -fullwarn -woff 1429 -D_SGI_SOURCE $(MPICMN)
CFLAGS=-ansi -n32 -O2 -fullwarn -woff 1429 -D_SGI_SOURCE $(MPICMN)
#CFLAGS=-ansi -n32 -g -fullwarn -woff 1429 -D_SGI_SOURCE $(MPICMN)
#CFLAGS=-ansi -64 -O2 -fullwarn -woff 1429 -D_SGI_SOURCE -DMP_NO_MP_WORD \
 $(MPICMN)
endif

ifeq ($(TARGET),alphaOSF1)
#Alpha/OSF1
MPICMN += -DMP_ASSEMBLY_MULTIPLY
AS_OBJS+= mpvalpha.o
#CFLAGS= -O -Olimit 4000 -ieee_with_inexact -std1 -DOSF1 -D_REENTRANT $(MPICMN)
CFLAGS= -O -Olimit 4000 -ieee_with_inexact -std1 -DOSF1 -D_REENTRANT \
 -DMP_NO_MP_WORD $(MPICMN)
endif

ifeq ($(TARGET),v9SOLARIS)
#Solaris 64
SOLARIS_FPU_FLAGS = -fast -xO5 -xrestrict=%all -xchip=ultra -xarch=v9a -KPIC -mt
#SOLARIS_FPU_FLAGS = -fast -xO5 -xrestrict=%all -xdepend -xchip=ultra -xarch=v9a -KPIC -mt
SOLARIS_ASM_FLAGS = -xchip=ultra -xarch=v9a -KPIC -mt 
AS_OBJS += montmulfv9.o 
AS_OBJS += mpi_sparc.o mpv_sparcv9.o
MPICMN += -DMP_USE_UINT_DIGIT 
#MPICMN += -DMP_NO_MP_WORD 
MPICMN += -DMP_ASSEMBLY_MULTIPLY 
MPICMN += -DMP_USING_MONT_MULF
CFLAGS= -O -KPIC -DSVR4 -DSYSV -D__svr4 -D__svr4__ -DSOLARIS -D_REENTRANT \
 -DSOLARIS2_8 -xarch=v9 -DXP_UNIX $(MPICMN)
#CFLAGS= -g -KPIC -DSVR4 -DSYSV -D__svr4 -D__svr4__ -DSOLARIS -D_REENTRANT \
 -DSOLARIS2_8 -xarch=v9 -DXP_UNIX $(MPICMN)
endif

ifeq ($(TARGET),v8plusSOLARIS)
#Solaris 32
SOLARIS_FPU_FLAGS = -fast -xO5 -xrestrict=%all -xdepend -xchip=ultra -xarch=v8plusa -KPIC -mt
SOLARIS_ASM_FLAGS = -xchip=ultra -xarch=v8plusa -KPIC -mt 
AS_OBJS += montmulfv8.o 
AS_OBJS += mpi_sparc.o mpv_sparcv8.o
#AS_OBJS = montmulf.o
MPICMN += -DMP_ASSEMBLY_MULTIPLY 
MPICMN += -DMP_USING_MONT_MULF 
MPICMN += -DMP_USE_UINT_DIGIT
MPICMN += -DMP_NO_MP_WORD
CFLAGS=-O -KPIC -DSVR4 -DSYSV -D__svr4 -D__svr4__ -DSOLARIS -D_REENTRANT \
 -DSOLARIS2_6 -xarch=v8plus -DXP_UNIX $(MPICMN)
endif

ifeq ($(TARGET),v8SOLARIS)
#Solaris 32
#SOLARIS_FPU_FLAGS = -fast -xO5 -xrestrict=%all -xdepend -xchip=ultra -xarch=v8 -KPIC -mt
#SOLARIS_ASM_FLAGS = -xchip=ultra -xarch=v8plusa -KPIC -mt 
#AS_OBJS = montmulfv8.o mpi_sparc.o mpv_sparcv8.o
#AS_OBJS = montmulf.o
#MPICMN += -DMP_USING_MONT_MULF
#MPICMN += -DMP_ASSEMBLY_MULTIPLY 
MPICMN += -DMP_USE_LONG_LONG_MULTIPLY -DMP_USE_UINT_DIGIT
MPICMN += -DMP_NO_MP_WORD
CFLAGS=-O -KPIC -DSVR4 -DSYSV -D__svr4 -D__svr4__ -DSOLARIS -D_REENTRANT \
 -DSOLARIS2_6 -xarch=v8 -DXP_UNIX $(MPICMN)
endif

ifeq ($(TARGET),ia64HPUX)
#HPUX 32 on ia64  -- 64 bit digits SCREAM.
# This one is for DD32 which is the 32-bit ABI with 64-bit registers.
CFLAGS= +O3 -DHPUX10 -D_POSIX_C_SOURCE=199506L -Aa +Z -DHPUX -Dhppa \
 -D_HPUX_SOURCE -Aa +e -z +p +DD32 -DHPUX11 -DXP_UNIX -Wl,+k $(MPICMN)
#CFLAGS= -O -DHPUX10 -D_POSIX_C_SOURCE=199506L -Aa +Z -DHPUX -Dhppa \
 -D_HPUX_SOURCE -Aa +e -z +p +DD32 -DHPUX11 -DXP_UNIX -Wl,+k $(MPICMN)
#CFLAGS= -g -DHPUX10 -D_POSIX_C_SOURCE=199506L -Ae +Z -DHPUX -Dhppa \
 -D_HPUX_SOURCE -Aa +e -z +p +DD32 -DHPUX11 -DXP_UNIX -Wl,+k $(MPICMN)
endif

ifeq ($(TARGET),ia64HPUX64)
#HPUX 32 on ia64
# This one is for DD64 which is the 64-bit ABI 
CFLAGS= +O3 -DHPUX10 -D_POSIX_C_SOURCE=199506L -Aa +Z -DHPUX -Dhppa \
 -D_HPUX_SOURCE -Aa +e -z +p +DD64 -DHPUX11 -DXP_UNIX -Wl,+k $(MPICMN)
#CFLAGS= -g -DHPUX10 -D_POSIX_C_SOURCE=199506L -Ae +Z -DHPUX -Dhppa \
 -D_HPUX_SOURCE -Aa +e -z +p +DD64 -DHPUX11 -DXP_UNIX -Wl,+k $(MPICMN)
endif

ifeq ($(TARGET),PA2.0WHPUX)
#HPUX64 (HP PA 2.0 Wide) using MAXPY and 64-bit digits
MPICMN += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE
AS_OBJS = mpi_hp.o hpma512.o hppa20.o 
CFLAGS= -O -DHPUX10 -D_POSIX_C_SOURCE=199506L -Ae +Z -DHPUX -Dhppa \
 -D_HPUX_SOURCE -Aa +e -z +DA2.0W +DS2.0 +O3 +DChpux -DHPUX11  -DXP_UNIX \
 $(MPICMN)
#CFLAGS= -g -DHPUX10 -D_POSIX_C_SOURCE=199506L -Ae +Z -DHPUX -Dhppa \
 -D_HPUX_SOURCE -Aa +e -z +DA2.0W +DS2.0 +DChpux -DHPUX11  -DXP_UNIX \
 $(MPICMN)
AS = $(CC) $(CFLAGS) -c
endif

ifeq ($(TARGET),PA2.0NHPUX)
#HPUX32 (HP PA 2.0 Narrow) hybrid model, using 32-bit digits
# This one is for DA2.0 (N) which is the 32-bit ABI with 64-bit registers.
MPICMN += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE
AS_OBJS = mpi_hp.o hpma512.o hppa20.o 
CFLAGS= +O3 -DHPUX10 -D_POSIX_C_SOURCE=199506L -Ae +Z -DHPUX -Dhppa \
 -D_HPUX_SOURCE -Aa +e -z +DA2.0 +DS2.0 +DChpux -DHPUX11  -DXP_UNIX \
 -Wl,+k $(MPICMN)
#CFLAGS= -g -DHPUX10 -D_POSIX_C_SOURCE=199506L -Ae +Z -DHPUX -Dhppa \
 -D_HPUX_SOURCE -Aa +e -z +DA2.0 +DS2.0 +DChpux -DHPUX11  -DXP_UNIX \
 -Wl,+k $(MPICMN)
AS = $(CC) $(CFLAGS) -c
endif

ifeq ($(TARGET),PA1.1HPUX)
#HPUX32 (HP PA 1.1) Pure 32 bit
MPICMN += -DMP_USE_UINT_DIGIT -DMP_NO_MP_WORD
#MPICMN += -DMP_USE_LONG_LONG_MULTIPLY
CFLAGS= -O -DHPUX10 -D_POSIX_C_SOURCE=199506L -Ae +Z -DHPUX -Dhppa \
 -D_HPUX_SOURCE +DAportable +DS1.1 -DHPUX11 -DXP_UNIX $(MPICMN)
##CFLAGS= -g -DHPUX10 -D_POSIX_C_SOURCE=199506L -Ae +Z -DHPUX -Dhppa \
# -D_HPUX_SOURCE +DAportable +DS1.1 -DHPUX11 -DXP_UNIX $(MPICMN)
endif

ifeq ($(TARGET),32AIX)
#
CC = xlC_r
MPICMN += -DMP_USE_UINT_DIGIT
MPICMN += -DMP_NO_DIV_WORD
#MPICMN += -DMP_NO_MUL_WORD
MPICMN += -DMP_NO_ADD_WORD
MPICMN += -DMP_NO_SUB_WORD
#MPICMN += -DMP_NO_MP_WORD
#MPICMN += -DMP_USE_LONG_LONG_MULTIPLY
CFLAGS = -O -DAIX -DSYSV -qarch=com -DAIX4_3 -DXP_UNIX -UDEBUG -DNDEBUG  $(MPICMN)
#CFLAGS = -g -DAIX -DSYSV -qarch=com -DAIX4_3 -DXP_UNIX -UDEBUG -DNDEBUG  $(MPICMN)
#CFLAGS += -pg
endif

ifeq ($(TARGET),64AIX)
#
CC = xlC_r
MPICMN += -DMP_USE_UINT_DIGIT
CFLAGS = -O -O2 -DAIX -DSYSV -qarch=com -DAIX_64BIT -DAIX4_3 -DXP_UNIX -UDEBUG -DNDEBUG $(MPICMN)
OBJECT_MODE=64
export OBJECT_MODE
endif

ifeq ($(TARGET),x86LINUX)
#Linux
AS_OBJS = mpi_x86.o
MPICMN += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE -DMP_ASSEMBLY_DIV_2DX1D
MPICMN += -DMP_MONT_USE_MP_MUL -DMP_CHAR_STORE_SLOW -DMP_IS_LITTLE_ENDIAN
CFLAGS= -O2 -fPIC -DLINUX1_2 -Di386 -D_XOPEN_SOURCE -DLINUX2_1 -ansi -Wall \
 -pipe -DLINUX -Dlinux -D_POSIX_SOURCE -D_BSD_SOURCE -DHAVE_STRERROR \
 -DXP_UNIX -UDEBUG -DNDEBUG -D_REENTRANT $(MPICMN)
#CFLAGS= -g -fPIC -DLINUX1_2 -Di386 -D_XOPEN_SOURCE -DLINUX2_1 -ansi -Wall \
 -pipe -DLINUX -Dlinux -D_POSIX_SOURCE -D_BSD_SOURCE -DHAVE_STRERROR \
 -DXP_UNIX -DDEBUG -UNDEBUG -D_REENTRANT $(MPICMN)
#CFLAGS= -g -fPIC -DLINUX1_2 -Di386 -D_XOPEN_SOURCE -DLINUX2_1 -ansi -Wall \
 -pipe -DLINUX -Dlinux -D_POSIX_SOURCE -D_BSD_SOURCE -DHAVE_STRERROR \
 -DXP_UNIX -UDEBUG -DNDEBUG -D_REENTRANT $(MPICMN)
endif

ifeq ($(TARGET),armLINUX)
MPICMN += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE
MPICMN += -DMP_USE_UINT_DIGIT 
AS_OBJS += mpi_arm.o
endif

ifeq ($(TARGET),AMD64SOLARIS)
ASFLAGS += -xarch=generic64
AS_OBJS = mpi_amd64.o mpi_amd64_sun.o
MP_CONFIG = -DMP_ASSEMBLY_MULTIPLY -DMPI_AMD64
MP_CONFIG += -DMP_CHAR_STORE_SLOW -DMP_IS_LITTLE_ENDIAN
CFLAGS = -xarch=generic64 -xO4 -I. -DMP_API_COMPATIBLE -DMP_IOFUNC $(MP_CONFIG)
MPICMN += $(MP_CONFIG)

mpi_amd64_asm.o: mpi_amd64_sun.s
	$(AS) -xarch=generic64 -P -D_ASM mpi_amd64_sun.s
endif

ifeq ($(TARGET),WIN32)
ifeq ($(CPU_ARCH),x86_64)
AS_OBJS = mpi_amd64.obj mpi_amd64_masm.obj mp_comba_amd64_masm.asm
CFLAGS  = -Od -Z7 -MDd -W3 -nologo -DDEBUG -D_DEBUG -UNDEBUG -DDEBUG_$(USER)
CFLAGS += -DWIN32 -DWIN64 -D_WINDOWS -D_AMD_64_ -D_M_AMD64 -DWIN95 -DXP_PC -DNSS_ENABLE_ECC 
CFLAGS += $(MPICMN)

$(AS_OBJS): %.obj : %.asm
	ml64 -Cp -Sn -Zi -coff -nologo -c $<

$(LIBOBJS): %.obj : %.c 
	cl $(CFLAGS) -Fo$@ -c $<
else
AS_OBJS = mpi_x86.obj
MPICMN += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE -DMP_ASSEMBLY_DIV_2DX1D
MPICMN += -DMP_USE_UINT_DIGIT -DMP_NO_MP_WORD -DMP_API_COMPATIBLE 
MPICMN += -DMP_MONT_USE_MP_MUL 
MPICMN += -DMP_CHAR_STORE_SLOW -DMP_IS_LITTLE_ENDIAN
CFLAGS  = -Od -Z7 -MDd -W3 -nologo -DDEBUG -D_DEBUG -UNDEBUG -DDEBUG_$(USER)
CFLAGS += -DWIN32 -D_WINDOWS -D_X86_ -DWIN95 -DXP_PC -DNSS_ENABLE_ECC 
CFLAGS += $(MPICMN)

$(AS_OBJS): %.obj : %.asm
	ml -Cp -Sn -Zi -coff -nologo -c $<

$(LIBOBJS): %.obj : %.c 
	cl $(CFLAGS) -Fo$@ -c $<

endif
endif
