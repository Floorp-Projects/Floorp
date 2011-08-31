#
# Makefile for MPI library

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
#   Richard C. Swift        (swift@netscape.com)
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

#
# $Id: Makefile,v 1.27 2011/02/06 08:42:27 nelson%bolyard.com Exp $
#

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
#PERL=perl

include target.mk

CFLAGS+= $(XCFLAGS)

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
SRCS=   mpi.c mpprime.c mplogic.c mp_gf2m.c mpmontg.c mpi-test.c primes.c \
	mpcpucache.c tests/ \
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
     mp_gf2m-priv.h utils/bbs_rand.h tests/mpi.h tests/mpprime.h

## These are the documentation files that go into the distribution file
DOCS=README doc utils/README utils/PRIMES 

## This is the list of tools built by 'make tools'
TOOLS=gcd invmod isprime lap dec2hex hex2dec primegen prng \
	basecvt fact exptmod pi makeprime identest

LIBOBJS = mpprime.o mpmontg.o mplogic.o mp_gf2m.o mpi.o mpcpucache.o $(AS_OBJS)
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

mp_gf2m.o: mp_gf2m.c mpi-priv.h mp_gf2m.h mp_gf2m-priv.h $(LIBHDRS)

mpmontg.o: mpmontg.c mpi-priv.h mplogic.h mpprime.h $(LIBHDRS)

mpprime.o: mpprime.c mpi-priv.h mpprime.h mplogic.h primes.c $(LIBHDRS)

mpcpucache.o: mpcpucache.c $(LIBHDRS)

mpi_mips.o: mpi_mips.s
	$(CC) -o $@ $(ASFLAGS) -c mpi_mips.s

mpi_sparc.o : montmulf.h

mpv_sparcv9.s: vis_64.il mpv_sparc.c
	$(CC) -o $@ $(SOLARIS_FPU_FLAGS) -S vis_64.il mpv_sparc.c

mpv_sparcv8.s: vis_64.il mpv_sparc.c
	$(CC) -o $@ $(SOLARIS_FPU_FLAGS) -S vis_32.il mpv_sparc.c

montmulfv8.o montmulfv9.o mpv_sparcv8.o mpv_sparcv9.o : %.o : %.s 
	$(CC) -o $@ $(SOLARIS_ASM_FLAGS) -c $<

mpi_arm.o: mpi_arm.c $(LIBHDRS)

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
