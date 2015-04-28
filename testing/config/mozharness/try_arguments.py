# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Because this list exposes new surface to our interface to try, and could
# easily produce unexpected results if misused, this should only include
# arguments likely to work with multiple harnesses, and will have unintended
# effects if conflicts with TryParser are introduced.
config = {
    '--tag': {
        'action': 'append',
        'dest': 'tags',
        'default': None,
    },
}
