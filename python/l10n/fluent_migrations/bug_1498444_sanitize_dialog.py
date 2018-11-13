# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY
from fluent.migrate import REPLACE
from fluent.migrate import CONCAT
from fluent.migrate.helpers import TERM_REFERENCE

def migrate(ctx):
    """Bug 1498444 - Migrate Sanitize Dialogs to use Fluent for localization, part {index}."""

    ctx.add_transforms(
        "browser/browser/sanitize.ftl",
        "browser/browser/sanitize.ftl",
        transforms_from(
"""
clear-time-duration-prefix =
    .value = { COPY("browser/chrome/browser/sanitize.dtd", "clearTimeDuration.label") }
    .accesskey = { COPY("browser/chrome/browser/sanitize.dtd", "clearTimeDuration.accesskey") }

clear-time-duration-value-last-hour =
    .label = { COPY("browser/chrome/browser/sanitize.dtd", "clearTimeDuration.lastHour") }

clear-time-duration-value-last-2-hours =
    .label = { COPY("browser/chrome/browser/sanitize.dtd", "clearTimeDuration.last2Hours") }

clear-time-duration-value-last-4-hours =
    .label = { COPY("browser/chrome/browser/sanitize.dtd", "clearTimeDuration.last4Hours") }

clear-time-duration-value-today =
    .label = { COPY("browser/chrome/browser/sanitize.dtd", "clearTimeDuration.today") }

clear-time-duration-value-everything =
    .label = { COPY("browser/chrome/browser/sanitize.dtd", "clearTimeDuration.everything") }

clear-time-duration-suffix =
    .value = { COPY("browser/chrome/browser/sanitize.dtd", "clearTimeDuration.suffix") }

history-section-label = { COPY("browser/chrome/browser/sanitize.dtd", "historySection.label") }

item-history-and-downloads =
    .label = { COPY("browser/chrome/browser/sanitize.dtd", "itemHistoryAndDownloads.label") }
    .accesskey = { COPY("browser/chrome/browser/sanitize.dtd", "itemHistoryAndDownloads.accesskey") }

item-cookies =
    .label = { COPY("browser/chrome/browser/sanitize.dtd", "itemCookies.label") }
    .accesskey = { COPY("browser/chrome/browser/sanitize.dtd", "itemCookies.accesskey") }

item-active-logins =
    .label = { COPY("browser/chrome/browser/sanitize.dtd", "itemActiveLogins.label") }
    .accesskey = { COPY("browser/chrome/browser/sanitize.dtd", "itemActiveLogins.accesskey") }

item-cache =
    .label = { COPY("browser/chrome/browser/sanitize.dtd", "itemCache.label") }
    .accesskey = { COPY("browser/chrome/browser/sanitize.dtd", "itemCache.accesskey") }

item-form-search-history =
    .label = { COPY("browser/chrome/browser/sanitize.dtd", "itemFormSearchHistory.label") }
    .accesskey = { COPY("browser/chrome/browser/sanitize.dtd", "itemFormSearchHistory.accesskey") }

data-section-label = { COPY("browser/chrome/browser/sanitize.dtd", "dataSection.label") }

item-site-preferences =
    .label = { COPY("browser/chrome/browser/sanitize.dtd", "itemSitePreferences.label") }
    .accesskey = { COPY("browser/chrome/browser/sanitize.dtd", "itemSitePreferences.accesskey") }

item-offline-apps =
    .label = { COPY("browser/chrome/browser/sanitize.dtd", "itemOfflineApps.label") }
    .accesskey = { COPY("browser/chrome/browser/sanitize.dtd", "itemOfflineApps.accesskey") }

sanitize-everything-undo-warning = { COPY("browser/chrome/browser/sanitize.dtd", "sanitizeEverythingUndoWarning") }

window-close =
    .key = { COPY("toolkit/chrome/global/preferences.dtd", "windowClose.key") }

sanitize-button-ok =
    .label = { COPY("browser/chrome/browser/browser.properties", "sanitizeButtonOK") }

sanitize-button-clearing =
    .label = { COPY("browser/chrome/browser/browser.properties", "sanitizeButtonClearing") }

sanitize-everything-warning = { COPY("browser/chrome/browser/browser.properties", "sanitizeEverythingWarning2") }
sanitize-selected-warning = { COPY("browser/chrome/browser/browser.properties", "sanitizeSelectedWarning") }
""")
)

    ctx.add_transforms(
    "browser/browser/sanitize.ftl",
    "browser/browser/sanitize.ftl",
    [
        FTL.Message(
            id=FTL.Identifier("clear-data-settings-label"),
            value=REPLACE(
                "browser/chrome/browser/sanitize.dtd",
                "clearDataSettings4.label",
                {
                    "&brandShortName;": TERM_REFERENCE("-brand-short-name")
                },
            )
        ),

        FTL.Message(
            id=FTL.Identifier("sanitize-prefs"),
            attributes=[
                FTL.Attribute(
                    id=FTL.Identifier("title"),
                    value=COPY(
                            "browser/chrome/browser/sanitize.dtd",
                            "sanitizePrefs2.title",
                        )
                    ),
                FTL.Attribute(
                    id=FTL.Identifier("style"),
                    value=CONCAT(
                        FTL.TextElement("width: "),
                        COPY(
                            "browser/chrome/browser/sanitize.dtd",
                            "sanitizePrefs2.modal.width",
                        )
                    )
                )
            ]
        ),

        FTL.Message(
            id=FTL.Identifier("sanitize-prefs-style"),
            attributes=[
                FTL.Attribute(
                    id=FTL.Identifier("style"),
                    value=CONCAT(
                        FTL.TextElement("width: "),
                        COPY(
                            "browser/chrome/browser/sanitize.dtd",
                            "sanitizePrefs2.column.width",
                        )
                    )
                )
            ]
        ),

        FTL.Message(
            id=FTL.Identifier("dialog-title"),
            attributes=[
                FTL.Attribute(
                    id=FTL.Identifier("title"),
                    value=COPY(
                            "browser/chrome/browser/sanitize.dtd",
                            "sanitizeDialog2.title",
                        )
                    ),
                FTL.Attribute(
                    id=FTL.Identifier("style"),
                    value=CONCAT(
                        FTL.TextElement("width: "),
                        COPY(
                            "browser/chrome/browser/sanitize.dtd",
                            "sanitizeDialog2.width",
                            )
                        )
                    )
                ]
            ),

            FTL.Message(
                id=FTL.Identifier("dialog-title-everything"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(
                                "browser/chrome/browser/browser.properties",
                                "sanitizeDialog2.everything.title",
                            )
                        ),
                    FTL.Attribute(
                        id=FTL.Identifier("style"),
                        value=CONCAT(
                            FTL.TextElement("width: "),
                            COPY(
                                "browser/chrome/browser/sanitize.dtd",
                                "sanitizeDialog2.width",
                                )
                            )
                        )
                    ]
                ),
            ]
        )

