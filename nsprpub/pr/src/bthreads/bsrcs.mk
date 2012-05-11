# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# this file lists the source files to be compiled (used in Makefile) and
# then enumerated as object files (in objs.mk) for inclusion in the NSPR
# shared library

BTCSRCS = \
	btthread.c \
	btlocks.c \
	btcvar.c \
	btmon.c \
	btsem.c \
	btmisc.c \
	$(NULL)
