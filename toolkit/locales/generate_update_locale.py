# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Generate update.locale, which simply contains the name of the current locale.

from __future__ import unicode_literals, print_function


def main(output, locale=None):
    assert locale is not None
    # update.locale is a trivial file but let's be unicode aware anyway.
    print(locale, file=output)
