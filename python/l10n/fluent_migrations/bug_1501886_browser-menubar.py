
# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import MESSAGE_REFERENCE, TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate import COPY, CONCAT, REPLACE


def migrate(ctx):
    """Bug 1501886 - Migrate browser main menubar to Fluent, part {index}"""

    ctx.add_transforms(
        'browser/browser/menubar.ftl',
        'browser/browser/menubar.ftl',
        transforms_from(
"""
menu-file =
    .label = { COPY("browser/chrome/browser/browser.dtd", "fileMenu.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "fileMenu.accesskey") }
menu-file-new-tab =
    .label = { COPY("browser/chrome/browser/browser.dtd", "tabCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "tabCmd.accesskey") }
menu-file-new-container-tab =
    .label = { COPY("browser/chrome/browser/browser.dtd", "newUserContext.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "newUserContext.accesskey") }
menu-file-new-window =
    .label = { COPY("browser/chrome/browser/browser.dtd", "newNavigatorCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "newNavigatorCmd.accesskey") }
menu-file-new-private-window =
    .label = { COPY("browser/chrome/browser/browser.dtd", "newPrivateWindow.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "newPrivateWindow.accesskey") }
menu-file-open-location =
    .label = { COPY("browser/chrome/browser/browser.dtd", "openLocationCmd.label") }
menu-file-open-file =
    .label = { COPY("browser/chrome/browser/browser.dtd", "openFileCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "openFileCmd.accesskey") }
menu-file-close =
    .label = { COPY("browser/chrome/browser/browser.dtd", "closeCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "closeCmd.accesskey") }
menu-file-close-window =
    .label = { COPY("browser/chrome/browser/browser.dtd", "closeWindow.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "closeWindow.accesskey") }
menu-file-save-page =
    .label = { COPY("browser/chrome/browser/browser.dtd", "savePageCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "savePageCmd.accesskey") }
menu-file-email-link =
    .label = { COPY("browser/chrome/browser/browser.dtd", "emailPageCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "emailPageCmd.accesskey") }
menu-file-print-setup =
    .label = { COPY("browser/chrome/browser/browser.dtd", "printSetupCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "printSetupCmd.accesskey") }
menu-file-print-preview =
    .label = { COPY("browser/chrome/browser/browser.dtd", "printPreviewCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "printPreviewCmd.accesskey") }
menu-file-print =
    .label = { COPY("browser/chrome/browser/browser.dtd", "printCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "printCmd.accesskey") }
menu-file-import-from-another-browser =
    .label = { COPY("browser/chrome/browser/browser.dtd", "importFromAnotherBrowserCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "importFromAnotherBrowserCmd.accesskey") }
menu-file-go-offline =
    .label = { COPY("browser/chrome/browser/browser.dtd", "goOfflineCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "goOfflineCmd.accesskey") }
menu-edit =
    .label = { COPY("browser/chrome/browser/browser.dtd", "editMenu.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "editMenu.accesskey") }
menu-edit-undo =
    .label = { COPY("browser/chrome/browser/browser.dtd", "undoCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "undoCmd.accesskey") }
menu-edit-redo =
    .label = { COPY("browser/chrome/browser/browser.dtd", "redoCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "redoCmd.accesskey") }
menu-edit-cut =
    .label = { COPY("browser/chrome/browser/browser.dtd", "cutCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "cutCmd.accesskey") }
menu-edit-copy =
    .label = { COPY("browser/chrome/browser/browser.dtd", "copyCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "copyCmd.accesskey") }
menu-edit-paste =
    .label = { COPY("browser/chrome/browser/browser.dtd", "pasteCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "pasteCmd.accesskey") }
menu-edit-delete =
    .label = { COPY("browser/chrome/browser/browser.dtd", "deleteCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "deleteCmd.accesskey") }
menu-edit-select-all =
    .label = { COPY("browser/chrome/browser/browser.dtd", "selectAllCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "selectAllCmd.accesskey") }
menu-edit-find-on =
    .label = { COPY("browser/chrome/browser/browser.dtd", "findOnCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "findOnCmd.accesskey") }
menu-edit-find-again =
    .label = { COPY("browser/chrome/browser/browser.dtd", "findAgainCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "findAgainCmd.accesskey") }
menu-edit-bidi-switch-text-direction =
    .label = { COPY("browser/chrome/browser/browser.dtd", "bidiSwitchTextDirectionItem.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "bidiSwitchTextDirectionItem.accesskey") }
menu-view =
    .label = { COPY("browser/chrome/browser/browser.dtd", "viewMenu.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "viewMenu.accesskey") }
menu-view-toolbars-menu =
    .label = { COPY("browser/chrome/browser/browser.dtd", "viewToolbarsMenu.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "viewToolbarsMenu.accesskey") }
menu-view-customize-toolbar =
    .label = { COPY("browser/chrome/browser/browser.dtd", "viewCustomizeToolbar.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "viewCustomizeToolbar.accesskey") }
menu-view-sidebar =
    .label = { COPY("browser/chrome/browser/browser.dtd", "viewSidebarMenu.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "viewSidebarMenu.accesskey") }
menu-view-bookmarks =
    .label = { COPY("browser/chrome/browser/browser.dtd", "bookmarksButton.label") }
menu-view-history-button =
    .label = { COPY("browser/chrome/browser/browser.dtd", "historyButton.label") }
menu-view-synced-tabs-sidebar =
    .label = { COPY("browser/chrome/browser/browser.dtd", "syncedTabs.sidebar.label") }
menu-view-full-zoom =
    .label = { COPY("browser/chrome/browser/browser.dtd", "fullZoom.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "fullZoom.accesskey") }
menu-view-full-zoom-enlarge =
    .label = { COPY("browser/chrome/browser/browser.dtd", "fullZoomEnlargeCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "fullZoomEnlargeCmd.accesskey") }
menu-view-full-zoom-reduce =
    .label = { COPY("browser/chrome/browser/browser.dtd", "fullZoomReduceCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "fullZoomReduceCmd.accesskey") }
menu-view-full-zoom-reset =
    .label = { COPY("browser/chrome/browser/browser.dtd", "fullZoomResetCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "fullZoomResetCmd.accesskey") }
menu-view-full-zoom-toggle =
    .label = { COPY("browser/chrome/browser/browser.dtd", "fullZoomToggleCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "fullZoomToggleCmd.accesskey") }
menu-view-page-style-menu =
    .label = { COPY("browser/chrome/browser/browser.dtd", "pageStyleMenu.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "pageStyleMenu.accesskey") }
menu-view-page-style-no-style =
    .label = { COPY("browser/chrome/browser/browser.dtd", "pageStyleNoStyle.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "pageStyleNoStyle.accesskey") }
menu-view-page-basic-style =
    .label = { COPY("browser/chrome/browser/browser.dtd", "pageStylePersistentOnly.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "pageStylePersistentOnly.accesskey") }
menu-view-charset =
    .label = { COPY("toolkit/chrome/global/charsetMenu.dtd", "charsetMenu2.label") }
    .accesskey = { COPY("toolkit/chrome/global/charsetMenu.dtd", "charsetMenu2.accesskey") }
menu-view-enter-full-screen =
    .label = { COPY("browser/chrome/browser/browser.dtd", "enterFullScreenCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "enterFullScreenCmd.accesskey") }
menu-view-exit-full-screen =
    .label = { COPY("browser/chrome/browser/browser.dtd", "exitFullScreenCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "exitFullScreenCmd.accesskey") }
menu-view-full-screen =
    .label = { COPY("browser/chrome/browser/browser.dtd", "fullScreenCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "fullScreenCmd.accesskey") }
menu-view-show-all-tabs =
    .label = { COPY("browser/chrome/browser/browser.dtd", "showAllTabsCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "showAllTabsCmd.accesskey") }
menu-view-bidi-switch-page-direction =
    .label = { COPY("browser/chrome/browser/browser.dtd", "bidiSwitchPageDirectionItem.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "bidiSwitchPageDirectionItem.accesskey") }
menu-history =
    .label = { COPY("browser/chrome/browser/browser.dtd", "historyMenu.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "historyMenu.accesskey") }
menu-history-show-all-history =
    .label = { COPY("browser/chrome/browser/browser.dtd", "showAllHistoryCmd2.label") }
menu-history-clear-recent-history =
    .label = { COPY("browser/chrome/browser/browser.dtd", "clearRecentHistory.label") }
menu-history-synced-tabs =
    .label = { COPY("browser/chrome/browser/browser.dtd", "syncTabsMenu3.label") }
menu-history-restore-last-session =
    .label = { COPY("browser/chrome/browser/browser.dtd", "historyRestoreLastSession.label") }
menu-history-hidden-tabs =
    .label = { COPY("browser/chrome/browser/browser.dtd", "hiddenTabs.label") }
menu-history-undo-menu =
    .label = { COPY("browser/chrome/browser/browser.dtd", "historyUndoMenu.label") }
menu-history-undo-window-menu =
    .label = { COPY("browser/chrome/browser/browser.dtd", "historyUndoWindowMenu.label") }
menu-bookmarks-menu =
    .label = { COPY("browser/chrome/browser/browser.dtd", "bookmarksMenu.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "bookmarksMenu.accesskey") }
menu-bookmarks-show-all =
    .label = { COPY("browser/chrome/browser/browser.dtd", "showAllBookmarks2.label") }
menu-bookmarks-all-tabs =
    .label = { COPY("browser/chrome/browser/browser.dtd", "addCurPagesCmd.label") }
menu-bookmarks-toolbar =
    .label = { COPY("browser/chrome/browser/browser.dtd", "personalbarCmd.label") }
menu-bookmarks-other =
    .label = { COPY("browser/chrome/browser/browser.dtd", "otherBookmarksCmd.label") }
menu-bookmarks-mobile =
    .label = { COPY("browser/chrome/browser/browser.dtd", "mobileBookmarksCmd.label") }
menu-tools =
    .label = { COPY("browser/chrome/browser/browser.dtd", "toolsMenu.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "toolsMenu.accesskey") }
menu-tools-downloads =
    .label = { COPY("browser/chrome/browser/browser.dtd", "downloads.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "downloads.accesskey") }
menu-tools-addons =
    .label = { COPY("browser/chrome/browser/browser.dtd", "addons.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "addons.accesskey") }
menu-tools-sync-now =
    .label = { COPY("browser/chrome/browser/browser.dtd", "syncSyncNowItem.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "syncSyncNowItem.accesskey") }
menu-tools-web-developer =
    .label = { COPY("browser/chrome/browser/browser.dtd", "webDeveloperMenu.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "webDeveloperMenu.accesskey") }
menu-tools-page-source =
    .label = { COPY("browser/chrome/browser/browser.dtd", "pageSourceCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "pageSourceCmd.accesskey") }
menu-tools-page-info =
    .label = { COPY("browser/chrome/browser/browser.dtd", "pageInfoCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "pageInfoCmd.accesskey") }
menu-preferences =
    .label =
        { PLATFORM() ->
            [windows] { COPY("browser/chrome/browser/browser.dtd", "preferencesCmd2.label") }
           *[other] { COPY("browser/chrome/browser/browser.dtd", "preferencesCmdUnix.label") }
        }
    .accesskey =
        { PLATFORM() ->
            [windows] { COPY("browser/chrome/browser/browser.dtd", "preferencesCmd2.accesskey") }
           *[other] { COPY("browser/chrome/browser/browser.dtd", "preferencesCmdUnix.accesskey") }
        }
menu-tools-layout-debugger =
    .label = { COPY("browser/chrome/browser/browser.dtd", "ldbCmd.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "ldbCmd.accesskey") }
menu-window-menu =
    .label = { COPY("browser/chrome/browser/baseMenuOverlay.dtd", "windowMenu.label") }
menu-window-bring-all-to-front =
    .label = { COPY("browser/chrome/browser/baseMenuOverlay.dtd", "bringAllToFront.label") }
menu-help =
    .label = { COPY("browser/chrome/browser/baseMenuOverlay.dtd", "helpMenu.label") }
    .accesskey = { COPY("browser/chrome/browser/baseMenuOverlay.dtd", "helpMenu.accesskey") }
menu-help-keyboard-shortcuts =
    .label = { COPY("browser/chrome/browser/baseMenuOverlay.dtd", "helpKeyboardShortcuts.label") }
    .accesskey = { COPY("browser/chrome/browser/baseMenuOverlay.dtd", "helpKeyboardShortcuts.accesskey") }
menu-help-troubleshooting-info =
    .label = { COPY("browser/chrome/browser/baseMenuOverlay.dtd", "helpTroubleshootingInfo.label") }
    .accesskey = { COPY("browser/chrome/browser/baseMenuOverlay.dtd", "helpTroubleshootingInfo.accesskey") }
menu-help-feedback-page =
    .label = { COPY("browser/chrome/browser/baseMenuOverlay.dtd", "helpFeedbackPage.label") }
    .accesskey = { COPY("browser/chrome/browser/baseMenuOverlay.dtd", "helpFeedbackPage.accesskey") }
menu-help-safe-mode-without-addons =
    .label = { COPY("browser/chrome/browser/baseMenuOverlay.dtd", "helpSafeMode.label") }
    .accesskey = { COPY("browser/chrome/browser/baseMenuOverlay.dtd", "helpSafeMode.accesskey") }
menu-help-safe-mode-with-addons =
    .label = { COPY("browser/chrome/browser/baseMenuOverlay.dtd", "helpSafeMode.stop.label") }
    .accesskey = { COPY("browser/chrome/browser/baseMenuOverlay.dtd", "helpSafeMode.stop.accesskey") }
menu-help-report-deceptive-site =
    .label = { COPY("browser/chrome/browser/safebrowsing/report-phishing.dtd", "reportDeceptiveSiteMenu.title") }
    .accesskey = { COPY("browser/chrome/browser/safebrowsing/report-phishing.dtd", "reportDeceptiveSiteMenu.accesskey") }
"""
        )
    )

    ctx.add_transforms(
        'browser/browser/menubar.ftl',
        'browser/browser/menubar.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('menu-tools-sync-sign-in'),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.dtd",
                            "syncSignIn.label",
                            {
                                "&syncBrand.shortName.label;": TERM_REFERENCE("sync-brand-short-name"),
                            }
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            "browser/chrome/browser/browser.dtd",
                            "syncSignIn.accesskey",
                        ),
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('menu-tools-sync-re-auth'),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.dtd",
                            "syncReAuthItem.label",
                            {
                                "&syncBrand.shortName.label;": TERM_REFERENCE("sync-brand-short-name"),
                            }
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            "browser/chrome/browser/browser.dtd",
                            "syncReAuthItem.accesskey",
                        ),
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('menu-help-product'),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            "browser/chrome/browser/baseMenuOverlay.dtd",
                            "productHelp2.label",
                            {
                                "&brandShorterName;": TERM_REFERENCE("brand-shorter-name"),
                            }
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            "browser/chrome/browser/baseMenuOverlay.dtd",
                            "productHelp2.accesskey",
                        ),
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('menu-help-show-tour'),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            "browser/chrome/browser/baseMenuOverlay.dtd",
                            "helpShowTour2.label",
                            {
                                "&brandShorterName;": TERM_REFERENCE("brand-shorter-name"),
                            }
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            "browser/chrome/browser/baseMenuOverlay.dtd",
                            "helpShowTour2.accesskey",
                        ),
                    ),
                ]
            ),
        ]
    )