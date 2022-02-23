# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""This routine controls which localizable files and entries are
reported and l10n-merged.
This needs to stay in sync with the copy in mobile/locales.
"""

from __future__ import absolute_import


def test(mod, path, entity=None):
    import re

    # ignore anything but mobile, which is our local repo checkout name
    if mod not in ("dom", "toolkit", "mobile", "mobile/android"):
        return "ignore"

    if mod == "toolkit":
        # keep this file list in sync with jar.mn
        if path in (
            "chrome/global/aboutReader.properties",
            "chrome/global/commonDialogs.properties",
            "chrome/global/intl.properties",
            "chrome/global/intl.css",
        ):
            return "error"
        if re.match(r"crashreporter/[^/]*.ftl", path):
            # error on crashreporter/*.ftl
            return "error"
        if re.match(r"toolkit/about/[^/]*About.ftl", path):
            # error on toolkit/about/*About.ftl
            return "error"
        if re.match(r"toolkit/about/[^/]*Mozilla.ftl", path):
            # error on toolkit/about/*Mozilla.ftl
            return "error"
        if re.match(r"toolkit/about/[^/]*Plugins.ftl", path):
            # error on toolkit/about/*Plugins.ftl
            return "error"
        if re.match(r"toolkit/about/[^/]*Rights.ftl", path):
            # error on toolkit/about/*Rights.ftl
            return "error"
        if re.match(r"toolkit/about/[^/]*Compat.ftl", path):
            # error on toolkit/about/*Compat.ftl
            return "error"
        if re.match(r"toolkit/about/[^/]*Support.ftl", path):
            # error on toolkit/about/*Support.ftl
            return "error"
        if re.match(r"toolkit/about/[^/]*Webrtc.ftl", path):
            # error on toolkit/about/*Webrtc.ftl
            return "error"
        return "ignore"

    if mod == "dom":
        # keep this file list in sync with jar.mn
        if path in (
            "chrome/global.dtd",
            "chrome/accessibility/AccessFu.properties",
            "chrome/dom/dom.properties",
        ):
            return "error"
        return "ignore"

    if mod not in ("mobile", "mobile/android"):
        # we only have exceptions for mobile*
        return "error"
    if mod == "mobile/android":
        if entity is None:
            if re.match(r"mobile-l10n.js", path) or re.match(r"defines.inc", path):
                return "ignore"
        if path == "defines.inc":
            if entity == "MOZ_LANGPACK_CONTRIBUTORS":
                return "ignore"
        return "error"

    # we're in mod == "mobile"
    if path == "chrome/region.properties":
        # only region.properties exceptions remain
        if (
            re.match(r"browser\.contentHandlers\.types\.[0-5]", entity)
            or re.match(r"gecko\.handlerService\.schemes\.", entity)
            or re.match(r"gecko\.handlerService\.defaultHandlersVersion", entity)
            or re.match(r"browser\.suggestedsites\.", entity)
        ):
            return "ignore"

    return "error"
