#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import json
import sys

l10n_changesets_json_path = sys.argv[1]
with open(l10n_changesets_json_path) as f:
    locales = json.load(f).keys()
linux_locales = [l for l in locales if l != "ja-JP-mac"]

print("\n".join(sorted(linux_locales)))
