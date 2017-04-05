# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""This routine controls which localizable files and entries are
reported and l10n-merged.
This needs to stay in sync with the copy in mobile/locales.
"""

def test(mod, path, entity = None):
  import re
  # ignore anything but mobile, which is our local repo checkout name
  if mod not in ("dom", "toolkit", "mobile",
                 "mobile/android/base",  "mobile/android"):
    return "ignore"

  if mod == "toolkit":
    # keep this file list in sync with jar.mn
    if path in (
        "chrome/global/about.dtd",
        "chrome/global/aboutAbout.dtd",
        "chrome/global/aboutReader.properties",
        "chrome/global/aboutRights.dtd",
        "chrome/global/charsetMenu.properties",
        "chrome/global/commonDialogs.properties",
        "chrome/global/intl.properties",
        "chrome/global/intl.css",
        "chrome/passwordmgr/passwordmgr.properties",
        "chrome/search/search.properties",
        "chrome/pluginproblem/pluginproblem.dtd",
        "chrome/global/aboutSupport.dtd",
        "chrome/global/aboutSupport.properties",
        "crashreporter/crashes.dtd",
        "crashreporter/crashes.properties",
        "chrome/global/mozilla.dtd",
        "chrome/global/aboutTelemetry.dtd",
        "chrome/global/aboutTelemetry.properties",
        "chrome/global/aboutWebrtc.properties"):
      return "error"
    return "ignore"

  if mod == "dom":
    # keep this file list in sync with jar.mn
    if path in (
        "chrome/global.dtd",
        "chrome/accessibility/AccessFu.properties",
        "chrome/dom/dom.properties",
        "chrome/plugins.properties"):
      return "error"
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
