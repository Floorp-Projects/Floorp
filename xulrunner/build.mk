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
# the Mozilla Foundation <http://www.mozilla.org/>.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Benjamin Smedberg <benjamin@smedbergs.us> (Initial Code)
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

include $(topsrcdir)/toolkit/toolkit-tiers.mk

TIERS += app

ifdef MOZ_EXTENSIONS
tier_app_dirs += extensions
endif

# axcontrol
ifndef LIBXUL_SDK
ifeq ($(OS_ARCH),WINNT)
ifndef MOZ_NO_ACTIVEX_SUPPORT
tier_app_dirs += \
		embedding/browser/activex/src/control \
		embedding/browser/activex/src/control_kicker \
		$(NULL)
endif # MOZ_NO_ACTIVEX_SUPPORT
endif # WINNT
endif # LIBXUL_SDK

# winembed, mfcembed
ifeq ($(OS_ARCH),WINNT)
ifneq (,$(ENABLE_TESTS)$(MOZILLA_OFFICIAL))
tier_app_dirs += embedding/tests
endif
endif

# os2embed
ifeq ($(OS_ARCH),OS2)
ifdef ENABLE_TESTS
tier_app_dirs += embedding/tests
endif
endif

ifdef MOZ_JAVAXPCOM
tier_app_dirs += extensions/java
endif

tier_app_dirs += xulrunner

installer:
	@echo "XULRunner doesn't have an installer yet."
	@exit 1

package:
	@$(MAKE) -C xulrunner/installer
