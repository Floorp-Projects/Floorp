#! gmake
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
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Stephen Fung <fungstep@hotmail.com> and
#   Douglas Stebila <douglas@stebila.ca>, Sun Microsystems
#   Laboratories
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

#######################################################################
# (1) Include initial platform-independent assignments (MANDATORY).   #
#######################################################################

include manifest.mn

#######################################################################
# (2) Include "global" configuration information. (OPTIONAL)          #
#######################################################################

include $(CORE_DEPTH)/coreconf/config.mk

#######################################################################
# (3) Include "component" configuration information. (OPTIONAL)       #
#######################################################################



#######################################################################
# (4) Include "local" platform-dependent assignments (OPTIONAL).      #
#######################################################################

-include config.mk

ifdef USE_64
	DEFINES += -DNSS_USE_64
endif

ifdef USE_HYBRID
	DEFINES += -DNSS_USE_HYBRID
endif

# des.c wants _X86_ defined for intel CPUs.  
# coreconf does this for windows, but not for Linux, FreeBSD, etc.
ifeq ($(CPU_ARCH),x86)
ifneq (,$(filter-out WIN%,$(OS_TARGET)))
	OS_REL_CFLAGS += -D_X86_
endif
endif

ifeq ($(OS_TARGET),OSF1)
    DEFINES += -DMP_ASSEMBLY_MULTIPLY -DMP_NO_MP_WORD
    MPI_SRCS += mpvalpha.c
endif

ifeq (,$(filter-out WINNT WIN95,$(OS_TARGET)))  #omits WIN16 and WINCE
ifdef NS_USE_GCC
# Ideally, we want to use assembler
#     ASFILES  = mpi_x86.s
#     DEFINES += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE \
#                -DMP_ASSEMBLY_DIV_2DX1D
# but we haven't figured out how to make it work, so we are not
# using assembler right now.
    ASFILES  =
    DEFINES += -DMP_NO_MP_WORD -DMP_USE_UINT_DIGIT
else
    ASFILES  = mpi_x86.asm
    DEFINES += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE -DMP_ASSEMBLY_DIV_2DX1D \
    -DMP_USE_UINT_DIGIT -DMP_NO_MP_WORD
endif
ifdef BUILD_OPT
ifndef NS_USE_GCC
    OPTIMIZER += -Ox  # maximum optimization for freebl
endif
endif
endif

ifeq ($(OS_TARGET),WINCE)
    DEFINES += -DMP_ARGCHK=0	# no assert in WinCE
    DEFINES += -DSHA_NO_LONG_LONG # avoid 64-bit arithmetic in SHA512
endif

ifdef XP_OS2_VACPP
    ASFILES  = mpi_x86.asm
    DEFINES += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE -DMP_ASSEMBLY_DIV_2DX1D -DMP_USE_UINT_DIGIT -DMP_NO_MP_WORD
endif

ifeq ($(OS_TARGET),IRIX)
ifeq ($(USE_N32),1)
    ASFILES  = mpi_mips.s
ifeq ($(NS_USE_GCC),1)
    ASFLAGS = -Wp,-P -Wp,-traditional -O -mips3
else
    ASFLAGS = -O -OPT:Olimit=4000 -dollar -fullwarn -xansi -n32 -mips3 
endif
    DEFINES += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE
    DEFINES += -DMP_USE_UINT_DIGIT
else
endif
endif

ifeq ($(OS_TARGET),Linux)
ifeq ($(CPU_ARCH),x86)
    ASFILES  = mpi_x86.s
    DEFINES += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE -DMP_ASSEMBLY_DIV_2DX1D
ifdef NSS_ENABLE_ECC
    #enable floating point ECC code	
    DEFINES  += -DECL_USE_FP
    ECL_SRCS += ecp_fp160.c ecp_fp192.c ecp_fp224.c ecp_fp.c
    ECL_HDRS += ecp_fp.h
endif
endif
endif

ifeq ($(OS_TARGET),AIX)
DEFINES += -DMP_USE_UINT_DIGIT
ifndef USE_64
DEFINES += -DMP_NO_DIV_WORD -DMP_NO_ADD_WORD -DMP_NO_SUB_WORD
endif
endif

ifeq ($(OS_TARGET), HP-UX)
ifneq ($(OS_TEST), ia64)
MKSHLIB += +k +vshlibunsats -u FREEBL_GetVector +e FREEBL_GetVector
ifndef FREEBL_EXTENDED_BUILD
ifdef USE_PURE_32
# build for DA1.1 (HP PA 1.1) pure 32 bit model
    DEFINES  += -DMP_USE_UINT_DIGIT -DMP_NO_MP_WORD
    DEFINES += -DSHA_NO_LONG_LONG # avoid 64-bit arithmetic in SHA512
else
ifdef USE_64
# this builds for DA2.0W (HP PA 2.0 Wide), the LP64 ABI, using 32-bit digits 
    MPI_SRCS += mpi_hp.c 
    ASFILES  += hpma512.s hppa20.s 
    DEFINES  += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE
else
# this builds for DA2.0 (HP PA 2.0 Narrow) hybrid model 
# (the 32-bit ABI with 64-bit registers) using 32-bit digits
    MPI_SRCS += mpi_hp.c 
    ASFILES  += hpma512.s hppa20.s 
    DEFINES  += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE
# This is done in coreconf by defining USE_LONG_LONGS
#   OS_CFLAGS += -Aa +e +DA2.0 +DS2.0
endif
endif
endif
endif
endif

# Note: -xarch=v8 or v9 is now done in coreconf
ifeq ($(OS_TARGET),SunOS)

# The -R '$ORIGIN' linker option instructs this library to search for its
# dependencies in the same directory where it resides.
MKSHLIB += -R '$$ORIGIN'

ifeq ($(CPU_ARCH),sparc)
ifndef NS_USE_GCC
ifdef USE_HYBRID
    OS_CFLAGS += -xchip=ultra2
endif
endif
ifeq (5.5.1,$(firstword $(sort 5.5.1 $(OS_RELEASE))))
    SYSV_SPARC = 1
endif
ifeq ($(SYSV_SPARC),1)
SOLARIS_AS = /usr/ccs/bin/as
ifdef NS_USE_GCC
ifdef GCC_USE_GNU_LD
MKSHLIB += -Wl,-Bsymbolic,-z,defs,-z,now,-z,text,--version-script,mapfile.Solaris
else
MKSHLIB += -Wl,-B,symbolic,-z,defs,-z,now,-z,text,-M,mapfile.Solaris
endif
else
MKSHLIB += -B symbolic -z defs -z now -z text -M mapfile.Solaris
endif
ifdef USE_PURE_32
# this builds for Sparc v8 pure 32-bit architecture
    DEFINES += -DMP_USE_LONG_LONG_MULTIPLY -DMP_USE_UINT_DIGIT -DMP_NO_MP_WORD
    DEFINES += -DSHA_NO_LONG_LONG # avoid 64-bit arithmetic in SHA512
else
ifdef USE_64
# this builds for Sparc v9a pure 64-bit architecture
    MPI_SRCS += mpi_sparc.c
    ASFILES   = mpv_sparcv9.s montmulfv9.s
    DEFINES  += -DMP_ASSEMBLY_MULTIPLY -DMP_USING_MONT_MULF
    DEFINES  += -DMP_USE_UINT_DIGIT 
#   MPI_SRCS += mpv_sparc.c
# removed -xdepend from the following line
    SOLARIS_FLAGS = -fast -xO5 -xrestrict=%all -xchip=ultra -xarch=v9a -KPIC -mt
    SOLARIS_AS_FLAGS = -xarch=v9a -K PIC
else
# this builds for Sparc v8+a hybrid architecture, 64-bit registers, 32-bit ABI
    MPI_SRCS += mpi_sparc.c
    ASFILES  = mpv_sparcv8.s montmulfv8.s
    DEFINES  += -DMP_NO_MP_WORD -DMP_ASSEMBLY_MULTIPLY -DMP_USING_MONT_MULF
    DEFINES  += -DMP_USE_UINT_DIGIT 
    SOLARIS_AS_FLAGS = -xarch=v8plusa -K PIC
#   ASM_SUFFIX = .S
endif
endif
ifdef NSS_ENABLE_ECC
DEFINES  += -DECL_USE_FP
ECL_SRCS += ecp_fp160.c ecp_fp192.c ecp_fp224.c ecp_fp.c
ECL_HDRS += ecp_fp.h
endif
endif
else
# Solaris x86
    DEFINES += -D_X86_
    DEFINES += -DMP_USE_UINT_DIGIT
    DEFINES += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE -DMP_ASSEMBLY_DIV_2DX1D
    ASFILES  = mpi_i86pc.s
ifdef NS_USE_GCC
    LD = gcc
    AS = gcc
    ASFLAGS =
endif

endif
endif

$(OBJDIR)/sysrand$(OBJ_SUFFIX): sysrand.c unix_rand.c win_rand.c mac_rand.c os2_rand.c

#######################################################################
# (5) Execute "global" rules. (OPTIONAL)                              #
#######################################################################

include $(CORE_DEPTH)/coreconf/rules.mk

#######################################################################
# (6) Execute "component" rules. (OPTIONAL)                           #
#######################################################################



#######################################################################
# (7) Execute "local" rules. (OPTIONAL).                              #
#######################################################################

export:: private_export

rijndael_tables:
	$(CC) -o $(OBJDIR)/make_rijndael_tab rijndael_tables.c \
	         $(DEFINES) $(INCLUDES) $(OBJDIR)/libfreebl.a
	$(OBJDIR)/make_rijndael_tab

ifdef USE_PURE_32 
vpath %.h $(FREEBL_PARENT)/ecl $(FREEBL_PARENT)/mpi $(FREEBL_PARENT)
vpath %.c $(FREEBL_PARENT)/ecl $(FREEBL_PARENT)/mpi $(FREEBL_PARENT)
vpath %.S $(FREEBL_PARENT)/ecl $(FREEBL_PARENT)/mpi $(FREEBL_PARENT)
vpath %.s $(FREEBL_PARENT)/ecl $(FREEBL_PARENT)/mpi $(FREEBL_PARENT)
vpath %.asm $(FREEBL_PARENT)/ecl $(FREEBL_PARENT)/mpi $(FREEBL_PARENT)
INCLUDES += -I$(FREEBL_PARENT) -I$(FREEBL_PARENT)/mpi -I$(FREEBL_PARENT)/ecl
else
vpath %.h mpi ecl
vpath %.c mpi ecl
vpath %.S mpi ecl
vpath %.s mpi ecl
vpath %.asm mpi ecl
INCLUDES += -Impi -Iecl
endif


DEFINES += -DMP_API_COMPATIBLE

MPI_USERS = dh.c pqg.c dsa.c rsa.c ec.c

MPI_OBJS = $(addprefix $(OBJDIR)/$(PROG_PREFIX), $(MPI_SRCS:.c=$(OBJ_SUFFIX)))
MPI_OBJS += $(addprefix $(OBJDIR)/$(PROG_PREFIX), $(MPI_USERS:.c=$(OBJ_SUFFIX)))

ECL_USERS = ec.c

ECL_OBJS = $(addprefix $(OBJDIR)/$(PROG_PREFIX), $(ECL_SRCS:.c=$(OBJ_SUFFIX)) $(ECL_ASM_SRCS:$(ASM_SUFFIX)=$(OBJ_SUFFIX)))
ECL_OBJS += $(addprefix $(OBJDIR)/$(PROG_PREFIX), $(ECL_USERS:.c=$(OBJ_SUFFIX)))

$(MPI_OBJS): $(MPI_HDRS)

$(ECL_OBJS): $(ECL_HDRS)

$(OBJDIR)/$(PROG_PREFIX)mpprime$(OBJ_SUFFIX): primes.c

$(OBJDIR)/ldvector$(OBJ_SUFFIX) $(OBJDIR)/loader$(OBJ_SUFFIX) : loader.h

ifeq ($(SYSV_SPARC),1)

$(OBJDIR)/mpv_sparcv8.o $(OBJDIR)/montmulfv8.o : $(OBJDIR)/%.o : %.s 
	@$(MAKE_OBJDIR)
	$(SOLARIS_AS) -o $@ $(SOLARIS_AS_FLAGS) $<

$(OBJDIR)/mpv_sparcv9.o $(OBJDIR)/montmulfv9.o : $(OBJDIR)/%.o : %.s
	@$(MAKE_OBJDIR)
	$(SOLARIS_AS) -o $@ $(SOLARIS_AS_FLAGS) $<

$(OBJDIR)/mpmontg.o: mpmontg.c montmulf.h

endif

ifdef FREEBL_EXTENDED_BUILD

PURE32DIR = $(OBJDIR)/$(OS_TARGET)pure32
ALL_TRASH += $(PURE32DIR)

FILES2LN = \
 $(wildcard *.tab) \
 $(wildcard mapfile.*) \
 Makefile manifest.mn config.mk 

LINKEDFILES = $(addprefix $(PURE32DIR)/, $(FILES2LN))

CDDIR := $(shell pwd)

$(PURE32DIR):
	-mkdir $(PURE32DIR)
	-ln -s $(CDDIR)/mpi $(PURE32DIR)

$(LINKEDFILES) : $(PURE32DIR)/% : % 
	ln -s $(CDDIR)/$* $(PURE32DIR)

libs:: 
	$(MAKE) FREEBL_RECURSIVE_BUILD=1 USE_HYBRID=1 libs

libs:: $(PURE32DIR) $(LINKEDFILES)
	cd $(PURE32DIR) && $(MAKE) FREEBL_RECURSIVE_BUILD=1 USE_PURE_32=1 FREEBL_PARENT=$(CDDIR) CORE_DEPTH=$(CDDIR)/$(CORE_DEPTH) libs

release_md::
	$(MAKE) FREEBL_RECURSIVE_BUILD=1 USE_HYBRID=1 $@
	cd $(PURE32DIR) && $(MAKE) FREEBL_RECURSIVE_BUILD=1 USE_PURE_32=1 FREEBL_PARENT=$(CDDIR) CORE_DEPTH=$(CDDIR)/$(CORE_DEPTH) $@

endif

# Bugzilla Bug 209827: disable optimization to work around what appears
# to be a VACPP optimizer bug.
ifdef XP_OS2_VACPP
$(OBJDIR)/alg2268.obj: alg2268.c
	@$(MAKE_OBJDIR)
	$(CC) -Fo$@ -c $(filter-out /O+, $(CFLAGS)) $(call abspath,$<)
endif
