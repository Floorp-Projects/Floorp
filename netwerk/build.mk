# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Mozilla build system.
#
# The Initial Developer of the Original Code is
# Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# NOTE: We need NSPR and xpcom, but their build.mk may or may not be
# included yet. Include it if needed.

ifndef tier_nspr_dirs
include $(topsrcdir)/config/nspr/build.mk
endif

ifndef tier_xpcom_dirs
include $(topsrcdir)/xpcom/build.mk
endif

TIERS += zlib \
	necko \
	$(NULL)


ifndef MOZ_NATIVE_ZLIB
tier_zlib_dirs	+= modules/zlib
endif

#
# tier "necko" - the networking library and its dependencies
#

# the offline cache uses mozStorage
ifdef MOZ_STORAGE
tier_necko_dirs += storage/public
endif

# these are only in the necko tier because libpref needs it

ifndef WINCE
ifneq (,$(MOZ_XPINSTALL))
tier_necko_dirs += modules/libreg
endif
endif

tier_necko_dirs += \
		modules/libpref \
		intl \
		netwerk \
		$(NULL)

ifdef MOZ_AUTH_EXTENSION
tier_necko_dirs += extensions/auth
endif

