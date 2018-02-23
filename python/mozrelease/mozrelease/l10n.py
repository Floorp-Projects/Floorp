from __future__ import absolute_import

from .platforms import shippedLocales2ftp


def getPlatformLocales(shipped_locales, platform):
    platform_locales = []
    for line in shipped_locales.splitlines():
        entry = line.strip().split()
        locale = entry[0]
        if len(entry) > 1:
            for sl_platform in entry[1:]:
                if platform in shippedLocales2ftp(sl_platform):
                    platform_locales.append(locale)
        else:
            platform_locales.append(locale)
    return platform_locales
