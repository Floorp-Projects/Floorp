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
# Config stuff for AIX.
#
include $(CORE_DEPTH)/coreconf/UNIX.mk

#
# There are two implementation strategies available on AIX:
# pthreads, and pthreads-user.  The default is pthreads.
# In both strategies, we need to use pthread_user.c, instead of
# aix.c.  The fact that aix.c is never used is somewhat strange.
# 
# So we need to do the following:
# - Default (PTHREADS_USER not defined in the environment or on
#   the command line):
#   Set PTHREADS_USER=1, USE_PTHREADS=1
# - PTHREADS_USER=1 set in the environment or on the command line:
#   Do nothing.
#
ifeq ($(PTHREADS_USER),1)
	USE_PTHREADS =            # just to be safe
	IMPL_STRATEGY = _PTH_USER
else
	USE_PTHREADS = 1
	PTHREADS_USER = 1
endif

DEFAULT_COMPILER = xlC_r

CC		= xlC_r
CCC		= xlC_r

CPU_ARCH	= rs6000

RANLIB		= ranlib

OS_CFLAGS	= -DAIX -DSYSV
ifeq ($(CC),xlC_r)
	OS_CFLAGS += -qarch=com
endif

AIX_WRAP	= $(DIST)/lib/aixwrap.o
AIX_TMP		= $(OBJDIR)/_aix_tmp.o
OS_LIBS		+= -lsvld
