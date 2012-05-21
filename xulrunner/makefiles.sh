#! /bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_makefiles "
xulrunner/Makefile
xulrunner/app/Makefile
xulrunner/app/profile/Makefile
xulrunner/app/profile/chrome/Makefile
xulrunner/app/profile/extensions/Makefile
xulrunner/examples/Makefile
xulrunner/examples/simple/Makefile
xulrunner/examples/simple/components/Makefile
xulrunner/examples/simple/components/public/Makefile
xulrunner/examples/simple/components/src/Makefile
xulrunner/setup/Makefile
xulrunner/stub/Makefile
xulrunner/installer/Makefile
"

if [ "$MAKENSISU" ]; then
  add_makefiles "
    xulrunner/installer/windows/Makefile
  "
fi

if [ "$OS_ARCH" = "Darwin" ]; then
  add_makefiles "
    xulrunner/installer/mac/Makefile
  "
fi

if [ "$OS_ARCH" = "WINNT" ]; then
  add_makefiles "
    xulrunner/tools/redit/Makefile
  "
  if [ "$MOZILLA_OFFICIAL" -o "$ENABLE_TESTS" ]; then
    add_makefiles "
      embedding/tests/winEmbed/Makefile
    "
  fi
fi
