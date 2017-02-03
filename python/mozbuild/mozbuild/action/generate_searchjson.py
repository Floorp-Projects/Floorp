# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import json
import copy

engines = []

locale = sys.argv[2]
output_file = sys.argv[3]

output = open(output_file, 'w')

with open(sys.argv[1]) as f:
  searchinfo = json.load(f)

if locale in searchinfo["locales"]:
  localeSearchInfo = searchinfo["locales"][locale]
else:
  localeSearchInfo = {}
  localeSearchInfo["default"] = searchinfo["default"]

# If we have region overrides, enumerate through them
# and add the additional regions to the locale information.
if "regionOverrides" in searchinfo:
  regionOverrides = searchinfo["regionOverrides"]

  for region in regionOverrides:
    if not region in localeSearchInfo:
      # Only add the region if it has engines that need to be overridden
      if set(localeSearchInfo["default"]["visibleDefaultEngines"]) & set(regionOverrides[region].keys()):
        localeSearchInfo[region] = copy.deepcopy(localeSearchInfo["default"])
      else:
        continue
    for i, engine in enumerate(localeSearchInfo[region]["visibleDefaultEngines"]):
      if engine in regionOverrides[region]:
        localeSearchInfo[region]["visibleDefaultEngines"][i] = regionOverrides[region][engine]

output.write(json.dumps(localeSearchInfo))

output.close();
