# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import json

engines = []

locale = sys.argv[2]

with open(sys.argv[1]) as f:
  searchinfo = json.load(f)

if locale in searchinfo["locales"]:
  for region in searchinfo["locales"][locale]:
    engines = list(set(engines)|set(searchinfo["locales"][locale][region]["visibleDefaultEngines"]))
else:
  engines = searchinfo["default"]["visibleDefaultEngines"]

print '\n'.join(engines)
