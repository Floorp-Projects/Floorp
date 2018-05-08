# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import json

engines = []

locale = sys.argv[2]

with open(sys.argv[1]) as f:
  searchinfo = json.load(f)

# Get a list of the engines from the locale or the default
engines = set()
if locale in searchinfo["locales"]:
  for region, table in searchinfo["locales"][locale].iteritems():
    if "visibleDefaultEngines" in table:
      engines.update(table["visibleDefaultEngines"])

if not engines:
  engines.update(searchinfo["default"]["visibleDefaultEngines"])

# Get additional engines from regionOverrides
for region, overrides in searchinfo["regionOverrides"].iteritems():
  for originalengine, replacement in overrides.iteritems():
    if originalengine in engines:
      # We add the engine because we still need the original
      engines.add(replacement)

# join() will take an iterable, not just a list.
print('\n'.join(engines))
