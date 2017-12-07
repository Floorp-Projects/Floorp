#!/usr/bin/python

#  This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""This file contains various locale lists consumed by other tools"""

# Sorted list of locales that ship in release builds of Focus/Klar.
#
# Other builds might include more locales.
#
# Note that there are differences in the locale codes used by Pontoon
# and by Android (e.g. Hebrew: he (Pontoon) vs. iw (Android)).
# This list uses the Android notation. A valid entry can be a
# language code (de) or a language plus country (de-DE). Do not use
# the name of the Androidresource folder (de-rDE) here.
#
# Releases should contain all locales that are at 100% or have been
# shipping in previous releases. Ping :delphine in case you want
# to add or remove locales from releases.
RELEASE_LOCALES = [
	"ar",
	"ast",
	"az",
	"bg",
	"bn-BD",
	"bn-IN",
        "bs",
	"ca",
    "cak",
	"cs",
	"cy",
	"da",
	"de",
	"dsb",
	"el",
	"eo",
	"es-AR",
	"es-CL",
	"es-ES",
	"es-MX",
	"eu",
	"fa",
    "fi",
	"fr",
	"fy-NL",
	"ga-IE",
	"hi-IN",
	"hsb",
	"hu",
	"hy-AM",
	"ia",
	"in",
	"it",
	"iw",
	"ja",
	"ka",
	"kab",
	"kk",
	"ko",
	"lo",
        "meh",
    "mix",
	"ms",
	"my",
	"nb-NO",
    "ne-NP",
	"nl",
	"nn-NO",
	"pl",
	"pt-BR",
	"ro",
	"ru",
	"sk",
	"sl",
	"sq",
	"sr",
	"sv-SE",
	"ta",
	"te",
	"th",
	"tr",
    "trs",
	"uk",
	"ur",
	"vi",
	"zh-CN",
	"zh-TW",
	"zam"
]

# This is the list of locales that we want to take automated screenshots of
# in addition to the list of release locales. We want to take screenshots
# of other locales so that translators of not yet completed locales can
# verify their work in progress.
ADDITIONAL_SCREENSHOT_LOCALES = [
	"lt"
]

# Those are locales that we take automated screenshots of.
SCREENSHOT_LOCALES = sorted(RELEASE_LOCALES + ADDITIONAL_SCREENSHOT_LOCALES)
