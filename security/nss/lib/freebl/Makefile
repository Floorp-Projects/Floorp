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

ifdef USE_64
	DEFINES += -DNSS_USE_64
endif

# des.c wants _X86_ defined for intel CPUs.  
# coreconf does this for windows, but not for Linux, FreeBSD, etc.
ifeq ($(CPU_ARCH),x86)
ifneq ($(OS_ARCH),WINNT)
	OS_REL_CFLAGS += -D_X86_
endif
endif

ifeq ($(OS_ARCH),WINNT)
ifneq ($(OS_TARGET),WIN16)
    ASFILES  = mpi_x86.asm
    DEFINES += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE -DMP_ASSEMBLY_DIV_2DX1D
endif
endif


ifeq ($(OS_ARCH),IRIX)
ifeq ($(USE_N32),1)
    ASFILES  = mpi_mips.s
    ASFLAGS = -O -OPT:Olimit=4000 -dollar -fullwarn -xansi -n32 -mips3 -exceptions 
    DEFINES += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE
else
endif
endif

ifeq ($(OS_TARGET),Linux)
ifeq ($(CPU_ARCH),x86)
    ASFILES  = mpi_x86.s
    DEFINES += -DMP_ASSEMBLY_MULTIPLY -DMP_ASSEMBLY_SQUARE -DMP_ASSEMBLY_DIV_2DX1D
endif
endif

ifeq ($(OS_ARCH), HP-UX)
ifdef USE_64
else
    DEFINES += -DMP_NO_MP_WORD
endif
endif

# Note: -xarch=v8 or v9 is now done in coreconf
ifeq ($(OS_TARGET),SunOS)
ifeq ($(CPU_ARCH),sparc)
    OS_CFLAGS += -xchip=ultra2
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
ifdef USE_64
else
    DEFINES += -DMP_NO_MP_WORD
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

ifdef MOZILLA_BSAFE_BUILD
ifeq ($(OS_ARCH),WINNT)
libbsafe=bsafe41.lib
else
libbsafe=libbsafe.a
endif

private_export::
	$(NSINSTALL) -m 777 $(libbsafe) $(DIST)/lib; \

endif

mp%.h : mpi/mp%.h
	cd mpi; cp $@ ..

mp%.c : mpi/mp%.c
	cd mpi; cp $@ ..

mp%.s : mpi/mp%.s
	cd mpi; cp $@ ..

mp%.asm : mpi/mp%.asm
	cd mpi; cp $@ ..

logtab.h : mpi/logtab.h
	cd mpi; cp $@ ..

primes.c : mpi/primes.c
	cd mpi; cp $@ ..

.PRECIOUS : $(MPI_SRCS) $(MPI_HDRS) $(ASFILES)

ALL_TRASH += $(MPI_SRCS) $(MPI_HDRS) $(ASFILES) primes.c

DEFINES += -DMP_API_COMPATIBLE

MPI_OBJS = $(addprefix $(OBJDIR)/$(PROG_PREFIX), $(MPI_SRCS:.c=$(OBJ_SUFFIX)))
#MPI_OBJS += $(addprefix $(OBJDIR)/$(PROG_PREFIX), $(DH_SRCS:.c=$(OBJ_SUFFIX)))

$(MPI_OBJS): $(MPI_HDRS) primes.c


