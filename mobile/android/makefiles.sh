# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_makefiles "
mobile/locales/Makefile
mobile/android/Makefile
mobile/android/app/Makefile
mobile/android/app/profile/extensions/Makefile
mobile/android/base/Makefile
mobile/android/base/locales/Makefile
$MOZ_BRANDING_DIRECTORY/Makefile
$MOZ_BRANDING_DIRECTORY/content/Makefile
$MOZ_BRANDING_DIRECTORY/locales/Makefile
mobile/android/chrome/Makefile
mobile/android/components/Makefile
mobile/android/modules/Makefile
mobile/android/installer/Makefile
mobile/android/locales/Makefile
mobile/android/themes/core/Makefile
"

if [ ! "$LIBXUL_SDK" ]; then
  add_makefiles "
    mobile/android/components/build/Makefile
  "
fi
