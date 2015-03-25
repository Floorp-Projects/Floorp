# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file is shared between compare-locales and locale-inspector
# test_util is in compare-locales only, for the sake of easy
# development.


def parseLocales(content):
    return sorted(l.split()[0] for l in content.splitlines() if l)
