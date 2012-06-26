#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_makefiles "
  services/Makefile
  services/aitc/Makefile
  services/common/Makefile
  services/crypto/Makefile
  services/crypto/component/Makefile
  services/notifications/Makefile
  services/sync/Makefile
  services/sync/locales/Makefile
"

if [ "$ENABLE_TESTS" ]; then
  add_makefiles "
    services/aitc/tests/Makefile
    services/common/tests/Makefile
    services/crypto/tests/Makefile
    services/notifications/tests/Makefile
    services/sync/tests/Makefile
  "
fi
