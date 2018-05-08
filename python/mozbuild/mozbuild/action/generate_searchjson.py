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

# If we have a locale, use it, otherwise use the default
if locale in searchinfo["locales"]:
  localeSearchInfo = searchinfo["locales"][locale]
else:
  localeSearchInfo = {}
  localeSearchInfo["default"] = searchinfo["default"]

def validateDefault(key):
  if (not key in searchinfo["default"]):
    print >>sys.stderr, "Error: Missing default %s in list.json" % (key)
    sys.exit(1)

validateDefault("searchDefault");
validateDefault("visibleDefaultEngines");

# If the selected locale doesn't have a searchDefault,
# use the global one.
if not "searchDefault" in localeSearchInfo["default"]:
  localeSearchInfo["default"]["searchDefault"] = searchinfo["default"]["searchDefault"]

# If we have region overrides, enumerate through them
# and add the additional regions to the locale information.
if "regionOverrides" in searchinfo:
  regionOverrides = searchinfo["regionOverrides"]

  for region in regionOverrides:
    # Only add a new engine list if there is an engine that is overridden
    enginesToOverride = set(regionOverrides[region].keys())
    if region in localeSearchInfo and "visibleDefaultEngines" in localeSearchInfo[region]:
       visibleDefaultEngines = localeSearchInfo[region]["visibleDefaultEngines"]
    else:
       visibleDefaultEngines = localeSearchInfo["default"]["visibleDefaultEngines"]
    if set(visibleDefaultEngines) & enginesToOverride:
      if region not in localeSearchInfo:
        localeSearchInfo[region] = {}
      localeSearchInfo[region]["visibleDefaultEngines"] = copy.deepcopy(visibleDefaultEngines)
      for i, engine in enumerate(localeSearchInfo[region]["visibleDefaultEngines"]):
        if engine in regionOverrides[region]:
          localeSearchInfo[region]["visibleDefaultEngines"][i] = regionOverrides[region][engine]

output.write(json.dumps(localeSearchInfo, ensure_ascii=False).encode('utf8'))

output.close();
