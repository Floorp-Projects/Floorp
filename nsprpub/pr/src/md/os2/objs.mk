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

# This makefile appends to the variable OBJS the platform-dependent
# object modules that will be part of the nspr20 library.

CSRCS = \
	os2io.c      \
	os2sock.c    \
	os2thred.c   \
	os2cv.c      \
	os2gc.c      \
	os2misc.c    \
	os2inrval.c  \
	os2sem.c     \
	os2_errors.c \
	os2poll.c    \
	os2rng.c     \
	$(NULL)

ifeq ($(MOZ_OS2_TOOLS),VACPP)
ASFILES = os2vacpp.asm
endif

ifeq ($(MOZ_OS2_TOOLS),EMX)
ASFILES = os2emx.s os2vaclegacy.s
endif

OBJS += $(addprefix md/os2/$(OBJDIR)/,$(CSRCS:.c=.$(OBJ_SUFFIX)))  \
	$(addprefix md/os2/$(OBJDIR)/,$(ASFILES:.$(ASM_SUFFIX)=.$(OBJ_SUFFIX)))

