#
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "MPL"); you may not use this file except in
# compliance with the MPL.  You may obtain a copy of the MPL at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the MPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
# for the specific language governing rights and limitations under the
# MPL.
#

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
	bseg.c        \
	btime.c       \
	bmmap.c       \
	$(NULL)
