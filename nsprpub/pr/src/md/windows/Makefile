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
# The Original Code is the Netscape Portable Runtime (NSPR).
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
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

#! gmake

MOD_DEPTH = ../../../..

include $(MOD_DEPTH)/config/config.mk

ifeq ($(OS_TARGET), WIN16)
CSRCS = \
	w16null.c \
	w16thred.c \
	w16proc.c \
    w16fmem.c \
    w16sock.c \
    w16mem.c \
    w16io.c  \
    w16gc.c  \
    w16error.c  \
    w16stdio.c  \
    w16callb.c \
    ntinrval.c \
    $(NULL)
else
ifeq ($(OS_TARGET), WIN95)
CSRCS =          \
    ntmisc.c \
    ntsec.c   \
    ntsem.c   \
    ntinrval.c \
    ntgc.c \
	w95thred.c \
	w95io.c \
	w95cv.c \
	w32rng.c \
	w95sock.c \
	win32_errors.c \
    w32ipcsem.c \
    w32poll.c \
    w32shm.c \
    w95dllmain.c \
    $(NULL)
else
CSRCS =          \
    ntdllmn.c \
    ntmisc.c \
    ntsec.c   \
    ntsem.c   \
    ntinrval.c \
    ntgc.c \
    ntthread.c \
    ntio.c    \
	win32_errors.c \
    w32ipcsem.c \
    w32poll.c \
    w32rng.c \
    w32shm.c \
    $(NULL)
endif
endif

TARGETS	= $(OBJS)

INCLUDES = -I$(DIST)/include -I$(MOD_DEPTH)/pr/include -I$(MOD_DEPTH)/pr/include/private

DEFINES     += -D_NSPR_BUILD_

include $(MOD_DEPTH)/config/rules.mk

export:: $(TARGETS)

install:: export
