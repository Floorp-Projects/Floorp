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


ifeq (WINNT,$(MOZ_TARGET))
CSRCS = ntmisc.c \
	ntsem.c \
	ntinrval.c \
	ntgc.c \
	ntio.c \
	ntthread.c \
	ntdllmn.c \
	win32_errors.c \
	w32poll.c
else
ifeq (WIN95,$(MOZ_TARGET))
CSRCS =	ntmisc.c \
	ntsem.c \
	ntinrval.c \
	ntgc.c \
	w95thred.c \
	w95io.c \
	w95cv.c \
	w95sock.c \
	win32_errors.c \
	w32poll.c \
	w95dllmain.c
else
ifeq (WIN16,$(MOZ_TARGET))
CSRCS =	w16null.c \
	w16thred.c \
	w16proc.c \
	w16fmem.c \
	w16sock.c \
	w16mem.c \
	w16io.c \
	w16gc.c \
	w16error.c \
	w16stdio.c \
	w16callb.c \
	ntinrval.c
endif # win16
endif # win95
endif # winnt

CSRCS	+= $(PR_MD_CSRCS)
ASFILES += $(PR_MD_ASFILES)

OBJS += $(addprefix md/windows/$(OBJDIR)/,$(CSRCS:.c=.$(OBJ_SUFFIX)))  \
	$(addprefix md/windows/$(OBJDIR)/,$(ASFILES:.s=.$(OBJ_SUFFIX)))


