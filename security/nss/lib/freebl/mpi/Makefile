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
## $Id: Makefile,v 1.6 2000/08/11 05:01:59 nelsonb%netscape.com Exp $
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
MPICMN = -I. -DMP_API_COMPATIBLE
CFLAGS= -O $(MPICMN)
#CFLAGS=-ansi -fullwarn -woff 1521 -O3 $(MPICMN)
#CFLAGS=-ansi -fullwarn -woff 1521 -L/usr/linguist/lib -g -DMP_DEBUG=1 $(MPICMN)
#CFLAGS=-ansi -pedantic -Wall -O3 $(MPICMN)
#CFLAGS=-ansi -pedantic -Wall -g -O2 -DMP_DEBUG=1 $(MPICMN)

#IRIX
#CFLAGS=-ansi -n32 -O3 -fullwarn -woff 1429 -D_SGI_SOURCE -DMP_IOFUNC $(MPICMN)
#CFLAGS=-ansi -n32 -O2 -fullwarn -woff 1429 -D_SGI_SOURCE -DMP_IOFUNC $(MPICMN)
#CFLAGS=-ansi -n32 -g -fullwarn -woff 1429 -D_SGI_SOURCE -DMP_IOFUNC $(MPICMN)
#CFLAGS=-ansi -n32 -g -fullwarn -woff 1429 -D_SGI_SOURCE -DMP_IOFUNC \
 -DMP_USE_32 $(MPICMN)

#Alpha/OSF1
#CFLAGS= -O -Olimit 4000 -ieee_with_inexact -std1 -DOSF1 -D_REENTRANT $(MPICMN)

#Solaris
#CFLAGS= -O -KPIC -DSVR4 -DSYSV -D__svr4 -D__svr4__ -DSOLARIS -D_REENTRANT \
 -DSOLARIS2_8 -D_SVID_GETTOD -xarch=v9 -DXP_UNIX -D_PR_GLOBAL_THREADS_ONLY \
 -DNSS_USE_64 $(MPICMN)
#CFLAGS=-O -KPIC -DSVR4 -DSYSV -D__svr4 -D__svr4__ -DSOLARIS -D_REENTRANT \
 -DSOLARIS2_6 -D_SVID_GETTOD -xarch=v8 -DXP_UNIX -D_PR_GLOBAL_THREADS_ONLY \
 $(MPICMN)

#HPUX
#CFLAGS= -O -DHPUX10 -D_POSIX_C_SOURCE=199506L -Ae +Z -DHPUX -Dhppa \
 -D_HPUX_SOURCE -Aa +e +DA2.0W +DS2.0 +DChpux -DHPUX11  -DXP_UNIX \
 -DNSS_USE_64 $(MPICMN)
#CFLAGS= -O -DHPUX10 -D_POSIX_C_SOURCE=199506L -Ae +Z -DHPUX -Dhppa \
 -D_HPUX_SOURCE +DAportable +DS1.1 -DHPUX11 -DXP_UNIX $(MPICMN)


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
SRCS=   mpi.c mpprime.c mplogic.c mpmontg.c mpi-test.c primes.c tests/ \
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
HDRS=mpi.h mpi-config.h utils/mpi.h utils/mpi-config.h mpprime.h mplogic.h \
     utils/bbs_rand.h tests/mpi.h tests/mpprime.h

## These are the documentation files that go into the distribution file
DOCS=README doc \
	utils/README utils/PRIMES 

## This is the list of tools built by 'make tools'
TOOLS=gcd invmod isprime lap dec2hex hex2dec primegen prng \
	basecvt fact exptmod pi makeprime

LIBOBJS = mpprime.o mpmontg.o mplogic.o mpi.o
LIBHDRS = mpi-config.h mpi-priv.h mpi.h

help:
	@ echo ""
	@ echo "The following targets can be built with this Makefile:"
	@ echo ""
	@ echo "libmpi       - arithmetic and prime testing library"
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

$(LIBOBJS): $(LIBHDRS)

mpi.o: mpi-config.h mpi-priv.h logtab.h mpi.h mpi.c
	$(CC) $(CFLAGS) -c mpi.c

libmpi.a: $(LIBOBJS)
	ar -cvr libmpi.a mpprime.o mpmontg.o mplogic.o mpi.o
	$(RANLIB) libmpi.a

logtab.h: make-logtab
	$(PERL) make-logtab > logtab.h

mdxptest: mdxptest.c libmpi.a
	$(CC) $(CFLAGS) -o mdxptest mdxptest.c libmpi.a $(LIBS)

mptest1: mpi.o tests/mptest-1.c
	$(CC) $(CFLAGS) -o mptest1 tests/mptest-1.c mpi.o $(LIBS)

mptest2: mpi.o tests/mptest-2.c
	$(CC) $(CFLAGS) -o mptest2 tests/mptest-2.c mpi.o $(LIBS)

mptest3: mpi.o tests/mptest-3.c
	$(CC) $(CFLAGS) -o mptest3 tests/mptest-3.c mpi.o $(LIBS)

mptest3a: libmpi.a tests/mptest-3a.c
	$(CC) $(CFLAGS) -o mptest3a tests/mptest-3a.c libmpi.a $(LIBS)

mptest4: mpi.o tests/mptest-4.c
	$(CC) $(CFLAGS) -o mptest4 tests/mptest-4.c mpi.o $(LIBS)

mptest4a: libmpi.a tests/mptest-4a.c
	$(CC) $(CFLAGS) -o mptest4a tests/mptest-4a.c libmpi.a $(LIBS)

mptest4b: libmpi.a tests/mptest-4b.c
	$(CC) $(CFLAGS) -o mptest4b tests/mptest-4b.c libmpi.a $(LIBS)

mptest5: mpi.o tests/mptest-5.c
	$(CC) $(CFLAGS) -o mptest5 tests/mptest-5.c mpi.o $(LIBS)

mptest5a: libmpi.a tests/mptest-5a.c
	$(CC) $(CFLAGS) -o mptest5a tests/mptest-5a.c libmpi.a $(LIBS)

mptest6: mpi.o tests/mptest-6.c
	$(CC) $(CFLAGS) -o mptest6 tests/mptest-6.c mpi.o $(LIBS)

mptest7: libmpi.a tests/mptest-7.c
	$(CC) $(CFLAGS) -o mptest7 tests/mptest-7.c libmpi.a $(LIBS)

mptest8: libmpi.a tests/mptest-8.c
	$(CC) $(CFLAGS) -o mptest8 tests/mptest-8.c libmpi.a $(LIBS)

mptest9: libmpi.a tests/mptest-9.c
	$(CC) $(CFLAGS) -o mptest9 tests/mptest-9.c libmpi.a $(LIBS)

test-info.c: test-arrays.txt
	$(PERL) make-test-arrays test-arrays.txt > test-info.c

mpi-test: libmpi.a test-info.c mpi-test.c
	$(CC) $(CFLAGS) -o mpi-test mpi-test.c libmpi.a $(LIBS)

bbsrand: utils/bbs_rand.o mpi.o utils/bbsrand.c
	$(CC) $(CFLAGS) -o bbsrand mpi.o bbs_rand.o utils/bbsrand.c $(LIBS)

primegen: libmpi.a utils/primegen.c primes.c
	$(CC) $(CFLAGS) -o primegen utils/primegen.c libmpi.a $(LIBS)

metime: libmpi.a utils/metime.c
	$(CC) $(CFLAGS) -o metime utils/metime.c libmpi.a $(LIBS)

mulsqr: logtab.h mpi.h mpi-config.h mpi.c mpprime.h mpprime.c mulsqr.c
	$(CC) $(CFLAGS) -DMP_SQUARE=1 -o mulsqr mpi.c mpprime.c mulsqr.c $(LIBS)

basecvt: mpi.o utils/basecvt.c
	$(CC) $(CFLAGS) -o basecvt utils/basecvt.c mpi.o $(LIBS)

fact: mpi.o utils/fact.c
	$(CC) $(CFLAGS) -o fact utils/fact.c mpi.o $(LIBS)

exptmod: mpi.o utils/exptmod.c
	$(CC) $(CFLAGS) -o exptmod utils/exptmod.c mpi.o $(LIBS)

pi: mpi.o utils/pi.c
	$(CC) $(CFLAGS) -o pi utils/pi.c mpi.o $(LIBS)

makeprime: libmpi.a utils/makeprime.c
	$(CC) $(CFLAGS) -o makeprime utils/makeprime.c libmpi.a $(LIBS)

prng: utils/bbs_rand.o mpi.o utils/prng.c
	$(CC) $(CFLAGS) -o prng utils/prng.c mpi.o bbs_rand.o $(LIBS)

karatsuba: libmpi.a utils/karatsuba.c
	$(CC) $(CFLAGS) -o karatsuba utils/karatsuba.c libmpi.a $(LIBS)

tests: mptest1 mptest2 mptest3 mptest3a mptest4 mptest4a mptest4b \
	mptest6 bbsrand

utests: mptest7 mptest8 mptest9

alltests: tests utests mpi-test

tools: $(TOOLS)

doc:
	(cd doc; ./build)

gcd: mpi.o utils/gcd.c
	$(CC) $(CFLAGS) -o gcd mpi.o utils/gcd.c $(LIBS)

invmod: mpi.o utils/invmod.c
	$(CC) $(CFLAGS) -o invmod mpi.o utils/invmod.c $(LIBS)

lap: mpi.o utils/lap.c
	$(CC) $(CFLAGS) -o lap mpi.o utils/lap.c $(LIBS)

isprime: mpprime.o mpi.o utils/isprime.c
	$(CC) $(CFLAGS) -o isprime mpprime.o mpi.o utils/isprime.c $(LIBS)

dec2hex: mpi.o utils/dec2hex.c
	$(CC) $(CFLAGS) -o dec2hex mpi.o utils/dec2hex.c $(LIBS)

hex2dec: mpi.o utils/hex2dec.c
	$(CC) $(CFLAGS) -o hex2dec mpi.o utils/hex2dec.c $(LIBS)

clean:
	rm -f *.o *.a 
	rm -f core
	rm -f *~ .*~
	rm -f utils/*.o
	rm -f utils/core
	rm -f utils/*~ utils/.*~

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
