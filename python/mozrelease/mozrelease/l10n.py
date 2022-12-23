# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


def getPlatformLocales(shipped_locales, platform):
    platform_locales = []
    for line in shipped_locales.splitlines():
        locale = line.strip().split()[0]
        # ja-JP-mac locale is a MacOS only locale
        if locale == "ja-JP-mac" and not platform.startswith("mac"):
            continue
        # Skip the "ja" locale on MacOS
        if locale == "ja" and platform.startswith("mac"):
            continue
        platform_locales.append(locale)
    return platform_locales
