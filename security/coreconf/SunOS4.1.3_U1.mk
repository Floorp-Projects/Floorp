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
# Config stuff for SunOS4.1
#

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER = cc

INCLUDES += -I/usr/dt/include -I/usr/openwin/include -I/home/motif/usr/include

# SunOS 4 _requires_ that shared libs have a version number.
# XXX FIXME: Version number should use NSPR_VERSION_NUMBER?
DLL_SUFFIX = so.1.0
CC         = gcc
RANLIB     = ranlib
CPU_ARCH   = sparc

# Purify doesn't like -MDupdate
NOMD_OS_CFLAGS += -Wall -Wno-format -DSUNOS4
OS_CFLAGS      += $(DSO_CFLAGS) $(NOMD_OS_CFLAGS) -MDupdate $(DEPENDENCIES)
MKSHLIB         = $(LD)
MKSHLIB        += $(DSO_LDOPTS)
NOSUCHFILE      = /solaris-rm-f-sucks
DSO_LDOPTS      =

# -fPIC generates position-independent code for use in a shared library.
DSO_CFLAGS += -fPIC
