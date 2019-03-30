# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import COPY
from fluent.migrate import REPLACE
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import TERM_REFERENCE

def migrate(ctx):
    """Bug 1523761 - Move the syncedTabs strings from browser.dtd to fluent, part {index}."""

    ctx.add_transforms(
        "browser/browser/syncedTabs.ftl",
        "browser/browser/syncedTabs.ftl",
        transforms_from(
"""
synced-tabs-sidebar-title = { COPY(from_path, "syncedTabs.sidebar.label") }
synced-tabs-sidebar-noclients-subtitle = { COPY(from_path, "syncedTabs.sidebar.noclients.subtitle") }
synced-tabs-sidebar-notsignedin = { COPY(from_path, "syncedTabs.sidebar.notsignedin.label") }
synced-tabs-sidebar-unverified = { COPY(from_path, "syncedTabs.sidebar.unverified.label") }
synced-tabs-sidebar-notabs = { COPY(from_path, "syncedTabs.sidebar.notabs.label") }
synced-tabs-sidebar-tabsnotsyncing = { COPY(from_path, "syncedTabs.sidebar.tabsnotsyncing.label") }
synced-tabs-sidebar-connect-another-device = { COPY(from_path, "syncedTabs.sidebar.connectAnotherDevice") }
synced-tabs-sidebar-search =
    .placeholder = { COPY(from_path, "syncedTabs.sidebar.searchPlaceholder") }
synced-tabs-context-open =
    .label = { COPY(from_path, "syncedTabs.context.open.label") }
    .accesskey = { COPY(from_path, "syncedTabs.context.open.accesskey") }
synced-tabs-context-open-in-new-tab =
    .label = { COPY(from_path, "syncedTabs.context.openInNewTab.label") }
    .accesskey = { COPY(from_path, "syncedTabs.context.openInNewTab.accesskey") }
synced-tabs-context-open-in-new-window =
    .label = { COPY(from_path, "syncedTabs.context.openInNewWindow.label") }
    .accesskey = { COPY(from_path, "syncedTabs.context.openInNewWindow.accesskey") }
synced-tabs-context-open-in-new-private-window =
    .label = { COPY(from_path, "syncedTabs.context.openInNewPrivateWindow.label") }
    .accesskey = { COPY(from_path, "syncedTabs.context.openInNewPrivateWindow.accesskey") }
synced-tabs-context-bookmark-single-tab =
    .label = { COPY(from_path, "syncedTabs.context.bookmarkSingleTab.label") }
    .accesskey = { COPY(from_path, "syncedTabs.context.bookmarkSingleTab.accesskey") }
synced-tabs-context-copy =
    .label = { COPY(from_path, "syncedTabs.context.copy.label") }
    .accesskey = { COPY(from_path, "syncedTabs.context.copy.accesskey") }
synced-tabs-context-open-all-in-tabs =
    .label = { COPY(from_path, "syncedTabs.context.openAllInTabs.label") }
    .accesskey = { COPY(from_path, "syncedTabs.context.openAllInTabs.accesskey") }
synced-tabs-context-manage-devices =
    .label = { COPY(from_path, "syncedTabs.context.managedevices.label") }
    .accesskey = { COPY(from_path, "syncedTabs.context.managedevices.accesskey") }
""", from_path="browser/chrome/browser/browser.dtd")
    )

    ctx.add_transforms(
        "browser/browser/syncedTabs.ftl",
        "browser/browser/syncedTabs.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("fxa-sign-in"),
                value=REPLACE(
                    "browser/chrome/browser/browser.dtd",
                    "fxaSignIn.label",
                    {
                        "&syncBrand.shortName.label;": TERM_REFERENCE("sync-brand-short-name"),
                    },
                )
            ),
            FTL.Message(
                id=FTL.Identifier("synced-tabs-sidebar-openprefs"),
                value=REPLACE(
                    "browser/chrome/browser/browser.dtd",
                    "syncedTabs.sidebar.openprefs.label",
                    {
                        "&syncBrand.shortName.label;": TERM_REFERENCE("sync-brand-short-name"),
                    },
                )
            ),
        ]
    )
