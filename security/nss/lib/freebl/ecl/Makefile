#
# Makefile for elliptic curve library

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

include ../mpi/target.mk

##
## Define platform-dependent variables for use of floating-point code.
##
ifeq ($(TARGET),v9SOLARIS)
ECL_USE_FP=1
else
ifeq ($(TARGET),v8plusSOLARIS)
ECL_USE_FP=1
else
ifeq ($(TARGET),v8SOLARIS)
ECL_USE_FP=1
else
ifeq ($(TARGET),x86LINUX)
ECL_USE_FP=1
endif
endif
endif
endif

##
## Add to definition of CFLAGS depending on use of floating-point code.
##
ifeq ($(ECL_USE_FP),1)
CFLAGS+= -DECL_USE_FP
endif

##
## Define LIBS to include any libraries you need to link against.
## If NO_TABLE is define, LIBS should include '-lm' or whatever is
## necessary to bring in the math library.  Otherwise, it can be
## left alone, unless your system has other peculiar requirements.
##
LIBS=-L../mpi -lmpi -lm#-lmalloc#-lefence

##
## Define INCLUDES to include any include directories you need to 
## compile with.  
##
INCLUDES=-I../mpi
CFLAGS+= $(INCLUDES) $(XCFLAGS)

## 
## Define RANLIB to be the library header randomizer; you might not
## need this on some systems (just set it to 'echo' on these systems,
## such as IRIX)
##
RANLIB=echo

## 
## Define LIBOBJS to be the object files that will be created during
## the build process.
##
LIBOBJS = ecl.o ecl_curve.o ecl_mult.o ecl_gf.o \
	ec2_aff.o ec2_mont.o ec2_proj.o \
	ec2_163.o ec2_193.o ec2_233.o \
	ecp_aff.o ecp_jac.o ecp_mont.o \
	ec_naf.o ecp_jm.o \
	ecp_192.o ecp_224.o ecp_256.o ecp_384.o ecp_521.o
ifeq ($(ECL_USE_FP),1)
LIBOBJS+= ecp_fp160.o ecp_fp192.o ecp_fp224.o ecp_fp.o
endif

## The headers contained in this library.
LIBHDRS = ecl-exp.h ecl.h ec2.h ecp.h ecl-priv.h ecl-curve.h
APPHDRS = ecl-exp.h ecl.h ec2.h ecp.h ecl-priv.h ecl-curve.h
ifeq ($(ECL_GFP_ASSEMBLY_FP),1)
LIBHDRS += ecp_fp.h
APPHDRS += ecp_fp.h
endif


help:
	@ echo ""
	@ echo "The following targets can be built with this Makefile:"
	@ echo ""
	@ echo "libecl.a     - elliptic curve library"
	@ echo "tests        - build command line tests"
	@ echo "test         - run command line tests"
	@ echo "clean        - clean up objects and such"
	@ echo ""

.SUFFIXES: .c .o .i

.c.i:
	$(CC) $(CFLAGS) -E $< > $@

#---------------------------------------

$(LIBOBJS): $(LIBHDRS)

ecl.o: ecl.c $(LIBHDRS)
ecl_curve.o: ecl_curve.c $(LIBHDRS)
ecl_mult.o: ecl_mult.c $(LIBHDRS)
ecl_gf.o: ecl_gf.c $(LIBHDRS)
ec2_aff.o: ec2_aff.c $(LIBHDRS)
ec2_mont.o: ec2_mont.c $(LIBHDRS)
ec2_proj.o: ec2_proj.c $(LIBHDRS)
ec2_163.o: ec2_163.c $(LIBHDRS)
ec2_193.o: ec2_193.c $(LIBHDRS)
ec2_233.o: ec2_233.c $(LIBHDRS)
ecp_aff.o: ecp_aff.c $(LIBHDRS)
ecp_jac.o: ecp_jac.c $(LIBHDRS)
ecp_jm.o: ecp_jm.c $(LIBHDRS)
ecp_mont.o: ecp_mont.c $(LIBHDRS)
ecp_192.o: ecp_192.c $(LIBHDRS)
ecp_224.o: ecp_224.c $(LIBHDRS)
ecp_256.o: ecp_256.c $(LIBHDRS)
ecp_384.o: ecp_384.c $(LIBHDRS)
ecp_521.o: ecp_521.c $(LIBHDRS)
ecp_fp.o: ecp_fp.c $(LIBHDRS)
ifeq ($(ECL_USE_FP),1)
ecp_fp160.o: ecp_fp160.c ecp_fpinc.c $(LIBHDRS)
ecp_fp192.o: ecp_fp192.c ecp_fpinc.c $(LIBHDRS)
ecp_fp224.o: ecp_fp224.c ecp_fpinc.c $(LIBHDRS)
endif

libecl.a: $(LIBOBJS)
	ar -cvr libecl.a $(LIBOBJS)
	$(RANLIB) libecl.a

lib libs: libecl.a

ecl.i: ecl.h

#---------------------------------------

ECLTESTOBJS = ec2_test.o ecp_test.o ec_naft.o
ifeq ($(ECL_USE_FP),1)
ECLTESTOBJS+= ecp_fpt.o
endif
ECLTESTS = $(ECLTESTOBJS:.o=)

$(ECLTESTOBJS): %.o: tests/%.c $(LIBHDRS)
	$(CC) $(CFLAGS) -o $@ -c $<  $(INCLUDES)

$(ECLTESTS): %: %.o libecl.a
	$(CC) $(CFLAGS) -o $@ $^  $(LIBS)

ifeq ($(ECL_USE_FP),1)
tests: ec2_test ecp_test ec_naft ecp_fpt 
else
tests: ec2_test ecp_test ec_naft
endif

#---------------------------------------

ifeq ($(ECL_USE_FP),1)
test: tests
	./ecp_test
	./ec2_test
	./ec_naft
	./ecp_fpt
else
test: tests
	./ecp_test
	./ec_naft
	./ec2_test
endif

#---------------------------------------

alltests: tests

clean:
	rm -f *.o *.a *.i
	rm -f core
	rm -f *~ .*~
	rm -f $(ECLTESTS)

clobber: clean

# END
