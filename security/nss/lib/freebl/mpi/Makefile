##
## Makefile for MPI library
##
## The contents of this file are subject to the Mozilla Public
## License Version 1.1 (the "License"); you may not use this file
## except in compliance with the License. You may obtain a copy of
## the License at http://www.mozilla.org/MPL/
##
## Software distributed under the License is distributed on an "AS
## IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
## implied. See the License for the specific language governing
## rights and limitations under the License.
##
## The Original Code is the MPI Arbitrary Precision Integer Arithmetic
## library.
##
## The Initial Developer of the Original Code is 
## Michael J. Fromberger <sting@linguist.dartmouth.edu>
##
## Portions created by Michael J. Fromberger are 
## Copyright (C) 1998, 2000 Michael J. Fromberger. All Rights Reserved.
##
## Contributor(s):
##	Netscape Communications Corporation
## 	Richard C. Swift	(swift@netscape.com)
##
## Alternatively, the contents of this file may be used under the
## terms of the GNU General Public License Version 2 or later (the
## "GPL"), in which case the provisions of the GPL are applicable
## instead of those above.  If you wish to allow use of your
## version of this file only under the terms of the GPL and not to
## allow others to use your version of this file under the MPL,
## indicate your decision by deleting the provisions above and
## replace them with the notice and other provisions required by
## the GPL.  If you do not delete the provisions above, a recipient
## may use your version of this file under either the MPL or the
## GPL.
## 
##
## $Id: Makefile,v 1.19 2003/03/29 00:18:22 nelsonb%netscape.com Exp $
##

## Define CC to be the C compiler you wish to use.  The GNU cc
## compiler (gcc) should work, at the very least
#CC=cc
#CC=gcc

## 
## Define PERL to point to your local Perl interpreter.  It
## should be Perl 5.x, although it's conceivable that Perl 4
## might work ... I haven't tested it.
##
#PERL=/usr/bin/perl
PERL=perl

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
 -DSOLARIS2_8 -D_SVID_GETTOD -xarch=v9 -DXP_UNIX $(MPICMN)
#CFLAGS= -g -KPIC -DSVR4 -DSYSV -D__svr4 -D__svr4__ -DSOLARIS -D_REENTRANT \
 -DSOLARIS2_8 -D_SVID_GETTOD -xarch=v9 -DXP_UNIX $(MPICMN)
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
 -DSOLARIS2_6 -D_SVID_GETTOD -xarch=v8plus -DXP_UNIX $(MPICMN)
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
 -DSOLARIS2_6 -D_SVID_GETTOD -xarch=v8 -DXP_UNIX $(MPICMN)
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
MPICMN += -DMP_MONT_USE_MP_MUL
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

##
## Define LIBS to include any libraries you need to link against.
## If NO_TABLE is define, LIBS should include '-lm' or whatever is
## necessary to bring in the math library.  Otherwise, it can be
## left alone, unless your system has other peculiar requirements.
##
LIBS=#-lmalloc#-lefence#-lm

## 
## Define RANLIB to be the library header randomizer; you might not
## need this on some systems (just set it to 'echo' on these systems,
## such as IRIX)
##
RANLIB=echo

##
## This is the version string used for the documentation and 
## building the distribution tarball.  Don't mess with it unless
## you are releasing a new version
VERS=1.7p6

## ----------------------------------------------------------------------
## You probably don't need to change anything below this line...
##

##
## This is the list of source files that need to be packed into
## the distribution file
SRCS=   mpi.c mpprime.c mplogic.c mp_gf2m.c mpmontg.c mpi-test.c primes.c tests/ \
	utils/gcd.c utils/invmod.c utils/lap.c \
	utils/ptab.pl utils/sieve.c utils/isprime.c\
	utils/dec2hex.c utils/hex2dec.c utils/bbs_rand.c \
	utils/bbsrand.c utils/prng.c utils/primegen.c \
	utils/basecvt.c utils/makeprime.c\
	utils/fact.c utils/exptmod.c utils/pi.c utils/metime.c \
	utils/mpi.h utils/mpprime.h mulsqr.c \
	make-test-arrays test-arrays.txt all-tests make-logtab \
	types.pl stats timetest multest

## These are the header files that go into the distribution file
HDRS=mpi.h mpi-config.h utils/mpi.h utils/mpi-config.h mpprime.h mplogic.h mp_gf2m.h \
     utils/bbs_rand.h tests/mpi.h tests/mpprime.h

## These are the documentation files that go into the distribution file
DOCS=README doc utils/README utils/PRIMES 

## This is the list of tools built by 'make tools'
TOOLS=gcd invmod isprime lap dec2hex hex2dec primegen prng \
	basecvt fact exptmod pi makeprime identest

LIBOBJS = mpprime.o mpmontg.o mplogic.o mp_gf2m.o mpi.o $(AS_OBJS)
LIBHDRS = mpi-config.h mpi-priv.h mpi.h
APPHDRS = mpi-config.h mpi.h mplogic.h mp_gf2m.h mpprime.h

help:
	@ echo ""
	@ echo "The following targets can be built with this Makefile:"
	@ echo ""
	@ echo "libmpi.a     - arithmetic and prime testing library"
	@ echo "mpi-test     - test driver (requires MP_IOFUNC)"
	@ echo "tools        - command line tools"
	@ echo "doc          - manual pages for tools"
	@ echo "clean        - clean up objects and such"
	@ echo "distclean    - get ready for distribution"
	@ echo "dist         - distribution tarball"
	@ echo ""

.SUFFIXES: .c .o .i

.c.i:
	$(CC) $(CFLAGS) -E $< > $@

#.c.o: $*.h $*.c
#	$(CC) $(CFLAGS) -c $<

#---------------------------------------

$(LIBOBJS): $(LIBHDRS)

logtab.h: make-logtab
	$(PERL) make-logtab > logtab.h

mpi.o: mpi.c logtab.h $(LIBHDRS)

mplogic.o: mplogic.c mpi-priv.h mplogic.h $(LIBHDRS)

mp_gf2m.o: mp_gf2m.c mpi-priv.h mp_gf2m.h $(LIBHDRS)

mpmontg.o: mpmontg.c mpi-priv.h mplogic.h mpprime.h $(LIBHDRS)

mpprime.o: mpprime.c mpi-priv.h mpprime.h mplogic.h primes.c $(LIBHDRS)

mpi_mips.o: mpi_mips.s
	$(CC) -o $@ $(ASFLAGS) -c mpi_mips.s

mpi_sparc.o : montmulf.h

mpv_sparcv9.s: vis_64.il mpv_sparc.c
	$(CC) -o $@ $(SOLARIS_FPU_FLAGS) -S vis_64.il mpv_sparc.c

mpv_sparcv8.s: vis_64.il mpv_sparc.c
	$(CC) -o $@ $(SOLARIS_FPU_FLAGS) -S vis_32.il mpv_sparc.c

montmulfv8.o montmulfv9.o mpv_sparcv8.o mpv_sparcv9.o : %.o : %.s 
	$(CC) -o $@ $(SOLARIS_ASM_FLAGS) -c $<

# This rule is used to build the .s sources, which are then hand optimized.
#montmulfv8.s montmulfv9.s : montmulf%.s : montmulf%.il montmulf.c montmulf.h 
#	$(CC) -o $@ $(SOLARIS_ASM_FLAGS) -S montmulf$*.il montmulf.c


libmpi.a: $(LIBOBJS)
	ar -cvr libmpi.a $(LIBOBJS)
	$(RANLIB) libmpi.a

lib libs: libmpi.a

mpi.i: mpi.h

#---------------------------------------

MPTESTOBJS = mptest1.o mptest2.o mptest3.o mptest3a.o mptest4.o mptest4a.o \
	mptest4b.o mptest6.o mptest7.o mptest8.o mptest9.o mptestb.o
MPTESTS = $(MPTESTOBJS:.o=)

$(MPTESTOBJS): mptest%.o: tests/mptest-%.c $(LIBHDRS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(MPTESTS): mptest%: mptest%.o libmpi.a
	$(CC) $(CFLAGS) -o $@ $^  $(LIBS)

tests: mptest1 mptest2 mptest3 mptest3a mptest4 mptest4a mptest4b mptest6 \
	mptestb bbsrand

utests: mptest7 mptest8 mptest9

#---------------------------------------

EXTRAOBJS = bbsrand.o bbs_rand.o prng.o
UTILOBJS = primegen.o metime.o identest.o basecvt.o fact.o exptmod.o pi.o \
	makeprime.o gcd.o invmod.o lap.o isprime.o \
	dec2hex.o hex2dec.o
UTILS = $(UTILOBJS:.o=) 

$(UTILS): % : %.o libmpi.a
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(UTILOBJS) $(EXTRAOBJS): %.o : utils/%.c $(LIBHDRS)
	$(CC) $(CFLAGS) -o $@ -c $<

prng: prng.o bbs_rand.o libmpi.a
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bbsrand: bbsrand.o bbs_rand.o libmpi.a
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

utils: $(UTILS) prng bbsrand

#---------------------------------------

test-info.c: test-arrays.txt
	$(PERL) make-test-arrays test-arrays.txt > test-info.c

mpi-test.o: mpi-test.c test-info.c $(LIBHDRS)
	$(CC) $(CFLAGS) -o $@ -c $<

mpi-test: mpi-test.o libmpi.a
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

mdxptest.o: mdxptest.c $(LIBHDRS) mpi-priv.h

mdxptest: mdxptest.o libmpi.a
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

mulsqr.o: mulsqr.c logtab.h mpi.h mpi-config.h mpprime.h 
	$(CC) $(CFLAGS) -DMP_SQUARE=1 -o $@ -c mulsqr.c 

mulsqr: mulsqr.o libmpi.a
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

#---------------------------------------

alltests: tests utests mpi-test

tools: $(TOOLS)

doc:
	(cd doc; ./build)

clean:
	rm -f *.o *.a *.i
	rm -f core
	rm -f *~ .*~
	rm -f utils/*.o
	rm -f utils/core
	rm -f utils/*~ utils/.*~

clobber: clean
	rm -f $(TOOLS) $(UTILS)

distclean: clean
	rm -f mptest? mpi-test metime mulsqr karatsuba
	rm -f mptest?a mptest?b
	rm -f utils/mptest?
	rm -f test-info.c logtab.h
	rm -f libmpi.a
	rm -f $(TOOLS)

dist: Makefile $(HDRS) $(SRCS) $(DOCS)
	tar -cvf mpi-$(VERS).tar Makefile $(HDRS) $(SRCS) $(DOCS)
	pgps -ab mpi-$(VERS).tar
	chmod +r mpi-$(VERS).tar.asc
	gzip -9 mpi-$(VERS).tar

# END
