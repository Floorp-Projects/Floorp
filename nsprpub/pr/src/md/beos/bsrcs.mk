# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# this file lists the source files to be compiled (used in Makefile) and
# then enumerated as object files (in objs.mk) for inclusion in the NSPR
# shared library

MDCSRCS =             \
	beos.c        \
	beos_errors.c \
	bfile.c       \
	bmisc.c       \
	bnet.c        \
	bproc.c       \
	brng.c        \
	bseg.c        \
	btime.c       \
	bmmap.c       \
	$(NULL)
