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
# The Original Code is Mozilla.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation <http://www.mozilla.org/>.
# Portions created by the Initial Developer are Copyright (C) 2007
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Mark Finkle <mfinkle@mozilla.com>
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

add_makefiles "
netwerk/locales/Makefile
dom/locales/Makefile
toolkit/locales/Makefile
security/manager/locales/Makefile
mobile/android/app/Makefile
mobile/android/app/profile/extensions/Makefile
mobile/android/base/Makefile
mobile/android/base/locales/Makefile
mobile/locales/Makefile
$MOZ_BRANDING_DIRECTORY/Makefile
$MOZ_BRANDING_DIRECTORY/locales/Makefile
mobile/android/chrome/Makefile
mobile/android/chrome/tests/Makefile
mobile/android/components/Makefile
mobile/android/components/build/Makefile
mobile/android/modules/Makefile
mobile/android/installer/Makefile
mobile/android/locales/Makefile
mobile/android/Makefile
mobile/android/themes/core/Makefile
"

if test -n "$MOZ_UPDATE_PACKAGING"; then
   add_makefiles "
     tools/update-packaging/Makefile
   "
fi
