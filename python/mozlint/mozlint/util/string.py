# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


def pluralize(s, num):
    if num != 1:
        s += "s"
    return str(num) + " " + s
