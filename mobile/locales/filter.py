# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""This routine controls which localizable files and entries are
reported and l10n-merged.
This needs to stay in sync with the copy in mobile/android/locales.
"""

def test(mod, path, entity = None):
  import re
  # ignore anything but mobile, which is our local repo checkout name
  if mod not in ("netwerk", "dom", "toolkit", "security/manager",
                 "services/sync", "mobile",
                 "mobile/android/base",  "mobile/android"):
    return "ignore"

  if mod not in ("mobile", "mobile/android"):
    # we only have exceptions for mobile*
    return "error"
  if mod == "mobile/android":
    if not entity:
      if (re.match(r"mobile-l10n.js", path) or
          re.match(r"defines.inc", path)):
        return "ignore"
    if path == "defines.inc":
      if entity == "MOZ_LANGPACK_CONTRIBUTORS":
        return "ignore"
    return "error"

  # we're in mod == "mobile"
  if re.match(r"searchplugins\/.+\.xml", path):
    return "ignore"
  if path == "chrome/region.properties":
    # only region.properties exceptions remain
    if (re.match(r"browser\.search\.order\.[1-9]", entity) or
        re.match(r"browser\.search\.[a-zA-Z]+\.US", entity) or
        re.match(r"browser\.contentHandlers\.types\.[0-5]", entity) or
        re.match(r"gecko\.handlerService\.schemes\.", entity) or
        re.match(r"gecko\.handlerService\.defaultHandlersVersion", entity) or
        re.match(r"browser\.suggestedsites\.", entity)):
      return "ignore"

  return "error"
