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
# Copyright (C) 2001 Netscape Communications Corporation.  All
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
# Config stuff for QNX
#

include $(CORE_DEPTH)/coreconf/UNIX.mk

USE_PTHREADS = 1

ifeq ($(USE_PTHREADS),1)
	IMPL_STRATEGY = _PTH
endif

CC			= qcc
CCC			= qcc
RANLIB			= ranlib

DEFAULT_COMPILER = qcc
ifeq ($(OS_TEST),mips)
	CPU_ARCH	= mips
else
	CPU_ARCH	= x86
endif

MKSHLIB		= $(CC) -shared -Wl,-soname -Wl,$(@:$(OBJDIR)/%.so=%.so)
ifdef BUILD_OPT
	OPTIMIZER	= -O2
endif

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -Vgcc_ntox86 -Wall -pipe -DNTO -DHAVE_STRERROR -D_QNX_SOURCE -D_POSIX_C_SOURCE=199506 -D_XOPEN_SOURCE=500

ifdef USE_PTHREADS
	DEFINES		+= -D_REENTRANT
endif

ARCH			= QNX

DSO_CFLAGS		= -Wc,-fPIC
DSO_LDOPTS		= -shared
DSO_LDFLAGS		=
