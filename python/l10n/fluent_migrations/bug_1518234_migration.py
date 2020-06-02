# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY, REPLACE
from fluent.migrate.helpers import VARIABLE_REFERENCE, TERM_REFERENCE

def migrate(ctx):
    """Bug 1518234 - Migrate the browser migration wizard, part {index}."""

    ctx.add_transforms(
            "browser/browser/migration.ftl",
            "browser/browser/migration.ftl",
    transforms_from(
"""
migration-wizard =
    .title = { COPY(from_path, "migrationWizard.title") }

import-from =
    { PLATFORM() ->
        [windows] { COPY(from_path, "importFrom.label") }
       *[other] { COPY(from_path, "importFromUnix.label") }
    }

import-from-bookmarks = { COPY(from_path, "importFromBookmarks.label") }
import-from-ie =
    .label = { COPY(from_path, "importFromIE.label") }
    .accesskey = { COPY(from_path, "importFromIE.accesskey") }
import-from-edge =
    .label = { COPY(from_path, "importFromEdge.label") }
    .accesskey = { COPY(from_path, "importFromEdge.accesskey") }
import-from-edge-legacy =
    .label = { COPY(from_path, "importFromEdgeLegacy.label") }
    .accesskey = { COPY(from_path, "importFromEdgeLegacy.accesskey") }
import-from-edge-beta =
    .label = { COPY(from_path, "importFromEdgeBeta.label") }
    .accesskey = { COPY(from_path, "importFromEdgeBeta.accesskey") }
import-from-nothing =
    .label = { COPY(from_path, "importFromNothing.label") }
    .accesskey = { COPY(from_path, "importFromNothing.accesskey") }
import-from-safari =
    .label = { COPY(from_path, "importFromSafari.label") }
    .accesskey = { COPY(from_path, "importFromSafari.accesskey") }
import-from-canary =
    .label = { COPY(from_path, "importFromCanary.label") }
    .accesskey = { COPY(from_path, "importFromCanary.accesskey") }
import-from-chrome =
    .label = { COPY(from_path, "importFromChrome.label") }
    .accesskey = { COPY(from_path, "importFromChrome.accesskey") }
import-from-chrome-beta =
    .label = { COPY(from_path, "importFromChromeBeta.label") }
    .accesskey = { COPY(from_path, "importFromChromeBeta.accesskey") }
import-from-chrome-dev =
    .label = { COPY(from_path, "importFromChromeDev.label") }
    .accesskey = { COPY(from_path, "importFromChromeDev.accesskey") }
import-from-chromium =
    .label = { COPY(from_path, "importFromChromium.label") }
    .accesskey = { COPY(from_path, "importFromChromium.accesskey") }
import-from-firefox =
    .label = { COPY(from_path, "importFromFirefox.label") }
    .accesskey = { COPY(from_path, "importFromFirefox.accesskey") }
import-from-360se =
    .label = { COPY(from_path, "importFrom360se.label") }
    .accesskey = { COPY(from_path, "importFrom360se.accesskey") }

no-migration-sources = { COPY(from_path, "noMigrationSources.label") }

import-source =
    .label = { COPY(from_path, "importSource.title") }
import-items-title =
    .label = { COPY(from_path, "importItems.title") }

import-items-description = { COPY(from_path, "importItems.label") }

import-migrating-title =
    .label = { COPY(from_path, "migrating.title") }

import-migrating-description = { COPY(from_path, "migrating.label") }

import-select-profile-title =
    .label = { COPY(from_path, "selectProfile.title") }

import-select-profile-description = { COPY(from_path, "selectProfile.label") }

import-done-title =
    .label = { COPY(from_path, "done.title") }

import-done-description = { COPY(from_path, "done.label") }

import-close-source-browser = { COPY(from_path, "closeSourceBrowser.label") }
""", from_path="browser/chrome/browser/migration/migration.dtd"))

    ctx.add_transforms(
            "browser/browser/migration.ftl",
            "browser/browser/migration.ftl",
            [
            FTL.Message(
                id=FTL.Identifier("imported-bookmarks-source"),
                value=REPLACE(
                    "browser/chrome/browser/migration/migration.properties",
                    "importedBookmarksFolder",
                    {
                        "%1$S": VARIABLE_REFERENCE("source")
                    },
                    normalize_printf=True
                )
            )
            ]
    )

    ctx.add_transforms(
            "browser/browser/migration.ftl",
            "browser/browser/migration.ftl",
    transforms_from(
"""
source-name-ie = { COPY(from_path, "sourceNameIE") }
source-name-edge = { COPY(from_path, "sourceNameEdge") }
source-name-edge-beta = { COPY(from_path, "sourceNameEdgeBeta") }
source-name-safari = { COPY(from_path, "sourceNameSafari") }
source-name-canary = { COPY(from_path, "sourceNameCanary") }
source-name-chrome = { COPY(from_path, "sourceNameChrome") }
source-name-chrome-beta = { COPY(from_path, "sourceNameChromeBeta") }
source-name-chrome-dev = { COPY(from_path, "sourceNameChromeDev") }
source-name-chromium = { COPY(from_path, "sourceNameChromium") }
source-name-firefox = { COPY(from_path, "sourceNameFirefox") }
source-name-360se = { COPY(from_path, "sourceName360se") }

imported-safari-reading-list = { COPY(from_path, "importedSafariReadingList") }
imported-edge-reading-list = { COPY(from_path, "importedEdgeReadingList") }

browser-data-ie-1 =
    .label = { COPY(from_path, "1_ie") }
    .value = { COPY(from_path, "1_ie") }
browser-data-edge-1 =
    .label = { COPY(from_path, "1_edge") }
    .value = { COPY(from_path, "1_edge") }
browser-data-safari-1 =
    .label = { COPY(from_path, "1_safari") }
    .value = { COPY(from_path, "1_safari") }
browser-data-chrome-1 =
    .label = { COPY(from_path, "1_chrome") }
    .value = { COPY(from_path, "1_chrome") }
browser-data-canary-1 =
    .label = { COPY(from_path, "1_chrome") }
    .value = { COPY(from_path, "1_chrome") }
browser-data-360se-1 =
    .label = { COPY(from_path, "1_360se") }
    .value = { COPY(from_path, "1_360se") }

browser-data-ie-2 =
    .label = { COPY(from_path, "2_ie") }
    .value = { COPY(from_path, "2_ie") }
browser-data-edge-2 =
    .label = { COPY(from_path, "2_edge") }
    .value = { COPY(from_path, "2_edge") }
browser-data-safari-2 =
    .label = { COPY(from_path, "2_safari") }
    .value = { COPY(from_path, "2_safari") }
browser-data-chrome-2 =
    .label = { COPY(from_path, "2_chrome") }
    .value = { COPY(from_path, "2_chrome") }
browser-data-canary-2 =
    .label = { COPY(from_path, "2_chrome") }
    .value = { COPY(from_path, "2_chrome") }
browser-data-firefox-2 =
    .label = { COPY(from_path, "2_firefox") }
    .value = { COPY(from_path, "2_firefox") }
browser-data-360se-2 =
    .label = { COPY(from_path, "2_360se") }
    .value = { COPY(from_path, "2_360se") }

browser-data-ie-4 =
    .label = { COPY(from_path, "4_ie") }
    .value = { COPY(from_path, "4_ie") }
browser-data-edge-4 =
    .label = { COPY(from_path, "4_edge") }
    .value = { COPY(from_path, "4_edge") }
browser-data-safari-4 =
    .label = { COPY(from_path, "4_safari") }
    .value = { COPY(from_path, "4_safari") }
browser-data-chrome-4 =
    .label = { COPY(from_path, "4_chrome") }
    .value = { COPY(from_path, "4_chrome") }
browser-data-canary-4 =
    .label = { COPY(from_path, "4_chrome") }
    .value = { COPY(from_path, "4_chrome") }
browser-data-firefox-history-and-bookmarks-4 =
    .label = { COPY(from_path, "4_firefox_history_and_bookmarks") }
    .value = { COPY(from_path, "4_firefox_history_and_bookmarks") }
browser-data-360se-4 =
    .label = { COPY(from_path, "4_360se") }
    .value = { COPY(from_path, "4_360se") }

browser-data-ie-8 =
    .label = { COPY(from_path, "8_ie") }
    .value = { COPY(from_path, "8_ie") }
browser-data-edge-8 =
    .label = { COPY(from_path, "8_edge") }
    .value = { COPY(from_path, "8_edge") }
browser-data-safari-8 =
    .label = { COPY(from_path, "8_safari") }
    .value = { COPY(from_path, "8_safari") }
browser-data-chrome-8 =
    .label = { COPY(from_path, "8_chrome") }
    .value = { COPY(from_path, "8_chrome") }
browser-data-canary-8 =
    .label = { COPY(from_path, "8_chrome") }
    .value = { COPY(from_path, "8_chrome") }
browser-data-firefox-8 =
    .label = { COPY(from_path, "8_firefox") }
    .value = { COPY(from_path, "8_firefox") }
browser-data-360se-8 =
    .label = { COPY(from_path, "8_360se") }
    .value = { COPY(from_path, "8_360se") }

browser-data-ie-16 =
    .label = { COPY(from_path, "16_ie") }
    .value = { COPY(from_path, "16_ie") }
browser-data-edge-16 =
    .label = { COPY(from_path, "16_edge") }
    .value = { COPY(from_path, "16_edge") }
browser-data-safari-16 =
    .label = { COPY(from_path, "16_safari") }
    .value = { COPY(from_path, "16_safari") }
browser-data-chrome-16 =
    .label = { COPY(from_path, "16_chrome") }
    .value = { COPY(from_path, "16_chrome") }
browser-data-canary-16 =
    .label = { COPY(from_path, "16_chrome") }
    .value = { COPY(from_path, "16_chrome") }
browser-data-firefox-16 =
    .label = { COPY(from_path, "16_firefox") }
    .value = { COPY(from_path, "16_firefox") }
browser-data-360se-16 =
    .label = { COPY(from_path, "16_360se") }
    .value = { COPY(from_path, "16_360se") }

browser-data-ie-32 =
    .label = { COPY(from_path, "32_ie") }
    .value = { COPY(from_path, "32_ie") }
browser-data-edge-32 =
    .label = { COPY(from_path, "32_edge") }
    .value = { COPY(from_path, "32_edge") }
browser-data-safari-32 =
    .label = { COPY(from_path, "32_safari") }
    .value = { COPY(from_path, "32_safari") }
browser-data-chrome-32 =
    .label = { COPY(from_path, "32_chrome") }
    .value = { COPY(from_path, "32_chrome") }
browser-data-canary-32 =
    .label = { COPY(from_path, "32_chrome") }
    .value = { COPY(from_path, "32_chrome") }
browser-data-360se-32 =
    .label = { COPY(from_path, "32_360se") }
    .value = { COPY(from_path, "32_360se") }

browser-data-ie-64 =
    .label = { COPY(from_path, "64_ie") }
    .value = { COPY(from_path, "64_ie") }
browser-data-edge-64 =
    .label = { COPY(from_path, "64_edge") }
    .value = { COPY(from_path, "64_edge") }
browser-data-safari-64 =
    .label = { COPY(from_path, "64_safari") }
    .value = { COPY(from_path, "64_safari") }
browser-data-chrome-64 =
    .label = { COPY(from_path, "64_chrome") }
    .value = { COPY(from_path, "64_chrome") }
browser-data-canary-64 =
    .label = { COPY(from_path, "64_chrome") }
    .value = { COPY(from_path, "64_chrome") }
browser-data-firefox-other-64 =
    .label = { COPY(from_path, "64_firefox_other") }
    .value = { COPY(from_path, "64_firefox_other") }
browser-data-360se-64 =
    .label = { COPY(from_path, "64_360se") }
    .value = { COPY(from_path, "64_360se") }

browser-data-firefox-128 =
    .label = { COPY(from_path, "128_firefox") }
    .value = { COPY(from_path, "128_firefox") }

""", from_path="browser/chrome/browser/migration/migration.properties"))
