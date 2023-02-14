#!/usr/bin/python

#  This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import re

OPEN_LOCALES = "locales = ["
CLOSE_LOCALES = "]"

# Android uses non-standard locale codes, these are the mappings back and forth
# See Legacy language codes in https://developer.android.com/reference/java/util/Locale.html
ANDROID_LEGACY_MAP = {
    'he': 'iw',
    'id': 'in',
    'yi': 'ji'
}

def trim_to_locale(str):
    match = re.search('\s*"([a-z]+-?[A-Z]*)",\s*', str)
    if not match:
        raise Exception("Failed parsing locale found in l10n.toml: " + str)

    locale = match.group(1)
    return ANDROID_LEGACY_MAP.get(locale, locale)


# This file is a dumb parser that converts values from '/l10n-release.toml' to be easily
# consumed from Python.
#
# 'l10n-release.toml' has a very simple structure, and it is reasonable to believe that this
# (very basic) algorithm will continue to work as it is changed.
#
# Alternatives to custom parsing that were considered:
# - Using standard library module --- none exists to parse TOML
# - Importing a TOML parsing module --- introduces an additional build step, complexity, and
#   security risk
# - Vendoring a TOML module --- large amount of code given the use case. Introduces a security
#   risk
def get_release_locales():
    with open(r"l10n-release.toml") as f:
        file = f.read().splitlines()

    locales_opened = False
    locales_closed = False

    found_locales = []

    for line in file:
        if line == OPEN_LOCALES:
            locales_opened = True
        elif line == CLOSE_LOCALES:
            locales_closed = True
            break
        elif locales_opened:
            found_locales.append(trim_to_locale(line))

    if locales_opened == False:
        raise Exception("Could not find `locales` open in l10n.toml")
    if locales_closed == False:
        raise Exception("Could not find `locales` close in l10n.toml")

    return found_locales


RELEASE_LOCALES = get_release_locales()
