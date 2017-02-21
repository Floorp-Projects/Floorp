# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json


def save_profile(profile, filename):
    with open(filename, "w") as f:
        json.dump(profile, f)
