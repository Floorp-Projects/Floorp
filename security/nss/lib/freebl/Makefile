#! gmake
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#

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

CPORLN = ln -s

ifdef USE_64
	DEFINES += -DNSS_USE_64
endif

ifdef USE_HYBRID
	DEFINES += -DNSS_USE_HYBRID
endif

# des.c wants _X86_ defined for intel CPUs.  
# coreconf does this for windows, but not for Linux, FreeBSD, etc.
ifeq ($(CPU_ARCH),x86)
ifneq ($(OS_ARCH),WINNT)
	OS_REL_CFLAGS += -D_X86_
endif
endif

ifeq ($(OS_ARCH),WINNT)
CPORLN = cp
ifneq ($(OS_TARGET),WIN16)
    ASFILES  = mpi_x86.asm
    DEFINES += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE -DMP_ASSEMBLY_DIV_2DX1D
endif
endif


ifeq ($(OS_ARCH),IRIX)
ifeq ($(USE_N32),1)
    ASFILES  = mpi_mips.s
    ASFLAGS = -O -OPT:Olimit=4000 -dollar -fullwarn -xansi -n32 -mips3 
    DEFINES += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE
    DEFINES += -DMP_USE_UINT_DIGIT
else
endif
endif

ifeq ($(OS_TARGET),Linux)
ifeq ($(CPU_ARCH),x86)
    ASFILES  = mpi_x86.s
    DEFINES += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE -DMP_ASSEMBLY_DIV_2DX1D
endif
endif

ifeq ($(OS_ARCH),AIX)
DEFINES += -DMP_USE_UINT_DIGIT
ifndef USE_64
DEFINES += -DMP_NO_DIV_WORD -DMP_NO_ADD_WORD -DMP_NO_SUB_WORD
endif
endif

ifeq ($(OS_ARCH), HP-UX)
MKSHLIB += +k +vshlibunsats -u FREEBL_GetVector +e FREEBL_GetVector
ifndef FREEBL_EXTENDED_BUILD
ifdef USE_PURE_32
# build for DA1.1 (HP PA 1.1) pure 32 bit model
    DEFINES  += -DMP_USE_UINT_DIGIT -DMP_NO_MP_WORD
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

# Note: -xarch=v8 or v9 is now done in coreconf
ifeq ($(OS_TARGET),SunOS)
ifeq ($(CPU_ARCH),sparc)
ifndef NS_USE_GCC
ifndef USE_PURE_32
    OS_CFLAGS += -xchip=ultra2
endif
endif
ifeq ($(OS_RELEASE),5.5.1)
    SYSV_SPARC = 1
endif
ifeq ($(OS_RELEASE),5.6)
    SYSV_SPARC = 1
endif
ifeq ($(OS_RELEASE),5.7)
    SYSV_SPARC = 1
endif
ifeq ($(OS_RELEASE),5.8)
    SYSV_SPARC = 1
endif
ifeq ($(SYSV_SPARC),1)
SOLARIS_AS = /usr/ccs/bin/as
MKSHLIB += -B symbolic -z defs -z now -z text -M mapfile.Solaris
ifdef USE_PURE_32
# this builds for Sparc v8 pure 32-bit architecture
    DEFINES += -DMP_USE_LONG_LONG_MULTIPLY -DMP_USE_UINT_DIGIT -DMP_NO_MP_WORD
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
endif
endif
endif


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

ifdef MOZILLA_BSAFE_BUILD

private_export::
ifeq ($(OS_ARCH), WINNT)
	rm -f $(DIST)/lib/bsafe$(BSAFEVER).lib
endif
	$(NSINSTALL) -R $(BSAFEPATH) $(DIST)/lib
endif

mp%.h : mpi/mp%.h
	-$(CPORLN) $< .

mp%.c : mpi/mp%.c
	-$(CPORLN) $< .

mp%.S : mpi/mp%.S
	-$(CPORLN) $< .

mp%.s : mpi/mp%.s
	-$(CPORLN) $< .

mp%.asm : mpi/mp%.asm
	-$(CPORLN) $< .

logtab.h : mpi/logtab.h
	-$(CPORLN) $< .

primes.c : mpi/primes.c
	-$(CPORLN) $< .

vis%.il : mpi/vis%.il
	-$(CPORLN) $< .

vis%.h : mpi/vis%.h
	-$(CPORLN) $< .

mont% : mpi/mont%
	-$(CPORLN) $< .

hp%.s : mpi/hp%.s
	-$(CPORLN) $< .

.PRECIOUS : $(MPI_SRCS) $(MPI_HDRS) $(ASFILES)

ALL_TRASH += $(MPI_SRCS) $(MPI_HDRS) $(ASFILES) primes.c mont*

DEFINES += -DMP_API_COMPATIBLE

MPI_USERS = dh.c pqg.c dsa.c rsa.c 

MPI_OBJS = $(addprefix $(OBJDIR)/$(PROG_PREFIX), $(MPI_SRCS:.c=$(OBJ_SUFFIX)))
MPI_OBJS += $(addprefix $(OBJDIR)/$(PROG_PREFIX), $(MPI_USERS:.c=$(OBJ_SUFFIX)))

$(MPI_OBJS): $(MPI_HDRS) primes.c

ifeq ($(SYSV_SPARC),1)
SPARCFIX = $(OBJDIR)/sparcfix

$(SPARCFIX): sparcfix.c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ -O sparcfix.c -lelf

$(OBJDIR)/mpv_sparcv8.o $(OBJDIR)/montmulfv8.o : $(OBJDIR)/%.o : %.s $(SPARCFIX)
	@$(MAKE_OBJDIR)
	$(SOLARIS_AS) -o $@ $(SOLARIS_AS_FLAGS) $*.s
	$(SPARCFIX) $@

$(OBJDIR)/mpv_sparcv9.o $(OBJDIR)/montmulfv9.o : $(OBJDIR)/%.o : %.s
	@$(MAKE_OBJDIR)
	$(SOLARIS_AS) -o $@ $(SOLARIS_AS_FLAGS) $*.s
#	$(SPARCFIX) $@

$(OBJDIR)/mpv_sparc.o: vis_64.il vis_proto.h mpv_sparc.c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ $(SOLARIS_FLAGS) -c vis_64.il mpv_sparc.c
#       $(SPARCFIX) $@

$(OBJDIR)/mpmontg.o: mpmontg.c montmulf.h
endif

ifeq ($(OS_ARCH), HP-UX)
$(OBJDIR)/mpmontg.o: mpmontg.c montmulf.h
endif

$(OBJDIR)/ldvector.o $(OBJDIR)/loader.o : loader.h

ifdef FREEBL_EXTENDED_BUILD

PURE32DIR = $(OS_ARCH)pure32
ALL_TRASH += $(PURE32DIR)

FILES2LN = \
 $(ALL_HDRS) \
 $(ALL_CSRCS) \
 $(wildcard *.tab) \
 $(wildcard mapfile.*) \
 Makefile manifest.mn config.mk 

LINKEDFILES = $(addprefix $(PURE32DIR)/, $(FILES2LN))

$(PURE32DIR):
	-mkdir $(PURE32DIR)
	-ln -s ../mpi $(PURE32DIR)

$(LINKEDFILES) : $(PURE32DIR)/% : % 
	ln -s ../$* $(PURE32DIR)

libs:: 
	$(MAKE) FREEBL_RECURSIVE_BUILD=1 USE_HYBRID=1 libs

libs:: $(PURE32DIR) $(LINKEDFILES)
	cd $(PURE32DIR) && $(MAKE) FREEBL_RECURSIVE_BUILD=1 USE_PURE_32=1 libs

endif
