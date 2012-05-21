# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_makefiles "
mobile/locales/Makefile
mobile/xul/Makefile
mobile/xul/app/Makefile
mobile/xul/app/profile/extensions/Makefile
$MOZ_BRANDING_DIRECTORY/Makefile
$MOZ_BRANDING_DIRECTORY/content/Makefile
$MOZ_BRANDING_DIRECTORY/locales/Makefile
mobile/xul/chrome/Makefile
mobile/xul/components/Makefile
mobile/xul/modules/Makefile
mobile/xul/installer/Makefile
mobile/xul/locales/Makefile
mobile/xul/themes/core/Makefile
"

if [ ! "$LIBXUL_SDK" ]; then
  add_makefiles "
    mobile/xul/components/build/Makefile
  "
fi

if [ "$ENABLE_TESTS" ]; then
  add_makefiles "
    mobile/xul/chrome/tests/Makefile
  "
fi
