# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, TERM_REFERENCE, MESSAGE_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate import COPY_PATTERN, REPLACE, COPY


def migrate(ctx):
    """Bug 1608022 - Migrate browser-sets to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/browser.ftl',
        'browser/browser/browser.ftl',
        [
            FTL.Message(
                id=FTL.Identifier("urlbar-star-edit-bookmark"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "starButtonOn.tooltip2",
                            {
                                "%1$S": VARIABLE_REFERENCE("shortcut")
                            },
                            normalize_printf=True
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("urlbar-star-add-bookmark"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "starButtonOff.tooltip2",
                            {
                                "%1$S": VARIABLE_REFERENCE("shortcut")
                            },
                            normalize_printf=True
                        )
                    )
                ]
            ),
        ]
    )

    ctx.add_transforms(
        'browser/browser/browserContext.ftl',
        'browser/browser/browserContext.ftl',
        [
            FTL.Message(
                id=FTL.Identifier("main-context-menu-bookmark-add"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=COPY_PATTERN(
                            "browser/browser/browserContext.ftl",
                            "main-context-menu-bookmark-page.aria-label",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY_PATTERN(
                            "browser/browser/browserContext.ftl",
                            "main-context-menu-bookmark-page.accesskey",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "starButtonOff.tooltip2",
                            {
                                " (%1$S)": FTL.TextElement("")
                            },
                            trim=True,
                            normalize_printf=True
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("main-context-menu-bookmark-add-with-shortcut"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=COPY_PATTERN(
                            "browser/browser/browserContext.ftl",
                            "main-context-menu-bookmark-page.aria-label",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY_PATTERN(
                            "browser/browser/browserContext.ftl",
                            "main-context-menu-bookmark-page.accesskey",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "starButtonOff.tooltip2",
                            {
                                "%1$S": VARIABLE_REFERENCE("shortcut")
                            },
                            normalize_printf=True
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("main-context-menu-bookmark-change"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=COPY(
                            "browser/chrome/browser/browser.dtd",
                            "editThisBookmarkCmd.label",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY_PATTERN(
                            "browser/browser/browserContext.ftl",
                            "main-context-menu-bookmark-page.accesskey",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "starButtonOn.tooltip2",
                            {
                                " (%1$S)": FTL.TextElement("")
                            },
                            trim=True,
                            normalize_printf=True
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("main-context-menu-bookmark-change-with-shortcut"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=COPY(
                            "browser/chrome/browser/browser.dtd",
                            "editThisBookmarkCmd.label",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY_PATTERN(
                            "browser/browser/browserContext.ftl",
                            "main-context-menu-bookmark-page.accesskey",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "starButtonOn.tooltip2",
                            {
                                "%1$S": VARIABLE_REFERENCE("shortcut")
                            },
                            normalize_printf=True
                        )
                    ),
                ]
            ),
        ]
    )

    ctx.add_transforms(
        'browser/browser/menubar.ftl',
        'browser/browser/menubar.ftl',
        transforms_from(
"""
menu-bookmark-this-page =
    .label = { COPY(from_path, "bookmarkThisPageCmd.label") }
menu-bookmark-edit =
    .label = { COPY(from_path, "editThisBookmarkCmd.label") }
""", from_path="browser/chrome/browser/browser.dtd")
    )

    ctx.add_transforms(
        'browser/browser/browserSets.ftl',
        'browser/browser/browserSets.ftl',
        transforms_from(
"""
window-minimize-command =
    .label = { COPY(baseMenuOverlay_path, "minimizeWindow.label") }

window-zoom-command =
    .label = { COPY(baseMenuOverlay_path, "zoomWindow.label") }

window-new-shortcut =
    .key = { COPY(browser_path, "newNavigatorCmd.key") }

window-minimize-shortcut =
    .key = { COPY(baseMenuOverlay_path, "minimizeWindow.key") }

close-shortcut =
    .key = { COPY(browser_path, "closeCmd.key") }

tab-new-shortcut =
    .key = { COPY(browser_path, "tabCmd.commandkey") }

location-open-shortcut =
    .key = { COPY(browser_path, "openCmd.commandkey") }

location-open-shortcut-alt =
    .key = { COPY(browser_path, "urlbar.accesskey") }

search-focus-shortcut =
    .key = { COPY(browser_path, "searchFocus.commandkey") }

find-shortcut =
    .key = { COPY(browser_path, "findOnCmd.commandkey") }

search-find-again-shortcut =
    .key = { COPY(browser_path, "findAgainCmd.commandkey") }

search-find-again-shortcut-alt =
    .keycode = { COPY(browser_path, "findAgainCmd.commandkey2") }

search-find-selection-shortcut =
    .key = { COPY(browser_path, "findSelectionCmd.commandkey") }

search-focus-shortcut-alt =
    .key = { PLATFORM() ->
        [linux] { COPY(browser_path, "searchFocusUnix.commandkey") }
       *[other] { COPY(browser_path, "searchFocus.commandkey2") }
    }

downloads-shortcut =
    .key = { PLATFORM() ->
        [linux] { COPY(browser_path, "downloadsUnix.commandkey") }
       *[other] { COPY(browser_path, "downloads.commandkey") }
    }

addons-shortcut =
    .key = { COPY(browser_path, "addons.commandkey") }

file-open-shortcut =
    .key = { COPY(browser_path, "openFileCmd.commandkey") }

save-page-shortcut =
    .key = { COPY(browser_path, "savePageCmd.commandkey") }

page-source-shortcut =
    .key = { COPY(browser_path, "pageSourceCmd.commandkey") }

page-source-shortcut-safari =
    .key = { COPY(browser_path, "pageSourceCmd.SafariCommandKey") }

page-info-shortcut =
    .key = { COPY(browser_path, "pageInfoCmd.commandkey") }

print-shortcut =
    .key = { COPY(browser_path, "printCmd.commandkey") }

mute-toggle-shortcut =
    .key = { COPY(browser_path, "toggleMuteCmd.key") }

nav-back-shortcut-alt =
    .key = { COPY(browser_path, "goBackCmd.commandKey") }

nav-fwd-shortcut-alt =
    .key = { COPY(browser_path, "goForwardCmd.commandKey") }

nav-reload-shortcut =
    .key = { COPY(browser_path, "reloadCmd.commandkey") }

nav-stop-shortcut =
    .key = { COPY(browser_path, "stopCmd.macCommandKey") }

history-show-all-shortcut =
    .key = { COPY(browser_path, "showAllHistoryCmd.commandkey") }

history-sidebar-shortcut =
    .key = { COPY(browser_path, "historySidebarCmd.commandKey") }

full-screen-shortcut =
    .key = { COPY(browser_path, "fullScreenCmd.macCommandKey") }

reader-mode-toggle-shortcut =
    .key = { PLATFORM() ->
        [windows] { COPY(browser_path, "toggleReaderMode.win.keycode") }
       *[other] { COPY(browser_path, "toggleReaderMode.key") }
    }

picture-in-picture-toggle-shortcut =
    .key = { COPY(browser_path, "togglePictureInPicture.key") }

picture-in-picture-toggle-shortcut-alt =
    .key = { "}" }

bookmark-this-page-shortcut =
    .key = { COPY(browser_path, "bookmarkThisPageCmd.commandkey") }

bookmark-show-all-shortcut =
    .key = { PLATFORM() ->
        [linux] { COPY(browser_path, "bookmarksGtkCmd.commandkey") }
       *[other] { COPY(browser_path, "bookmarksWinCmd.commandkey") }
    }

bookmark-show-sidebar-shortcut =
    .key = { COPY(browser_path, "bookmarksCmd.commandkey") }

full-zoom-reduce-shortcut =
    .key = { COPY(browser_path, "fullZoomReduceCmd.commandkey") }

full-zoom-reduce-shortcut-alt =
    .key = { COPY(browser_path, "fullZoomReduceCmd.commandkey2") }

full-zoom-enlarge-shortcut =
    .key = { COPY(browser_path, "fullZoomEnlargeCmd.commandkey") }

full-zoom-enlarge-shortcut-alt =
    .key = { COPY(browser_path, "fullZoomEnlargeCmd.commandkey2") }

full-zoom-enlarge-shortcut-alt2 =
    .key = { COPY(browser_path, "fullZoomEnlargeCmd.commandkey3") }

full-zoom-reset-shortcut =
    .key = { COPY(browser_path, "fullZoomResetCmd.commandkey") }

full-zoom-reset-shortcut-alt =
    .key = { COPY(browser_path, "fullZoomResetCmd.commandkey2") }

bidi-switch-direction-shortcut =
    .key = { COPY(browser_path, "bidiSwitchTextDirectionItem.commandkey") }

private-browsing-shortcut =
    .key = { COPY(browser_path, "privateBrowsingCmd.commandkey") }

quit-app-shortcut =
    .key = { COPY(browser_path, "quitApplicationCmd.key") }

help-shortcut =
    .key = { COPY(baseMenuOverlay_path, "helpMac.commandkey") }

preferences-shortcut =
    .key = { COPY(baseMenuOverlay_path, "preferencesCmdMac.commandkey") }

hide-app-shortcut =
    .key = { COPY(baseMenuOverlay_path, "hideThisAppCmdMac2.commandkey") }

hide-other-apps-shortcut =
    .key = { COPY(baseMenuOverlay_path, "hideOtherAppsCmdMac.commandkey") }
""", baseMenuOverlay_path="browser/chrome/browser/baseMenuOverlay.dtd",
     browser_path="browser/chrome/browser/browser.dtd")
    )
