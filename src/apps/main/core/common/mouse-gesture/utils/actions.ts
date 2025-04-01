// eslint-disable no-unsafe-optional-chaining
// deno-lint-ignore-file no-window
import { type GestureActionRegistration } from "./gestures.ts";

export const actions: GestureActionRegistration[] = [
  {
    name: "goBack",
    fn: () =>
      (document?.getElementById("back-button") as XULElement).doCommand(),
  },
  {
    name: "goForward",
    fn: () =>
      (document?.getElementById("forward-button") as XULElement).doCommand(),
  },
  {
    name: "reload",
    fn: () =>
      (document?.getElementById("reload-button") as XULElement).doCommand(),
  },
  {
    name: "closeTab",
    fn: () => window.gBrowser.removeTab(window.gBrowser.selectedTab),
  },
  {
    name: "newTab",
    fn: () =>
      (document?.getElementById("tabs-newtab-button") as XULElement)
        .doCommand(),
  },
  {
    name: "duplicateTab",
    fn: () => window.gBrowser.duplicateTab(window.gBrowser.selectedTab),
  },
  {
    name: "reloadAllTabs",
    fn: () => window.gBrowser.reloadAllTabs(),
  },
  {
    name: "reopenClosedTab",
    fn: () => window.undoCloseTab(),
  },
  {
    name: "openNewWindow",
    fn: () => window.OpenBrowserWindow(),
  },
  {
    name: "openNewPrivateWindow",
    fn: () => window.OpenBrowserWindow({ private: true }),
  },
  {
    name: "closeWindow",
    fn: () => window.BrowserTryToCloseWindow(),
  },
  {
    name: "restoreLastWindow",
    fn: () => window.undoCloseWindow(),
  },
  {
    name: "showNextTab",
    fn: () => window.gBrowser.tabContainer.advanceSelectedTab(1, true),
  },
  {
    name: "showPreviousTab",
    fn: () => window.gBrowser.tabContainer.advanceSelectedTab(-1, true),
  },
  {
    name: "showAllTabsPanel",
    fn: () => window.gTabsPanel.showAllTabsPanel(),
  },
  {
    name: "forceReload",
    fn: () => window.BrowserReloadSkipCache(),
  },
  {
    name: "zoomIn",
    fn: () => window.FullZoom.enlarge(),
  },
  {
    name: "zoomOut",
    fn: () => window.FullZoom.reduce(),
  },
  {
    name: "resetZoom",
    fn: () => window.FullZoom.reset(),
  },
  {
    name: "bookmarkThisPage",
    fn: () =>
      window.BrowserPageActions.doCommandForAction(
        window.PageActions.actionForID("bookmark"),
        null,
        window,
      ),
  },
  {
    name: "openHomePage",
    fn: () => window.BrowserHome(),
  },
  {
    name: "openAddonsManager",
    fn: () => window.BrowserOpenAddonsMgr(),
  },
  {
    name: "restoreLastTab",
    fn: () => window.undoCloseTab(),
  },
  {
    name: "sendWithMail",
    fn: () =>
      window.MailIntegration.sendLinkForBrowser(
        window.gBrowser.selectedBrowser,
      ),
  },
  {
    name: "savePage",
    fn: () => window.saveBrowser(window.gBrowser.selectedBrowser),
  },
  {
    name: "printPage",
    fn: () =>
      window.PrintUtils.startPrintWindow(
        window.gBrowser.selectedBrowser.browsingContext,
      ),
  },
  {
    name: "muteCurrentTab",
    fn: () =>
      window.gBrowser.toggleMuteAudioOnMultiSelectedTabs(
        window.gBrowser.selectedTab,
      ),
  },
  {
    name: "showSourceOfPage",
    fn: () => window.BrowserViewSource(window.gBrowser.selectedBrowser),
  },
  {
    name: "showPageInfo",
    fn: () => window.BrowserPageInfo(),
  },
  {
    name: "EnableRestMode",
    fn: () => window.gFloorpCommands.enableRestMode(),
  },
  {
    name: "hideInterface",
    fn: () => window.gFloorpDesign.hideUserInterface(),
  },
  {
    name: "toggleNavigationPanel",
    fn: () => window.gFloorpDesign.toggleNavigationPanel(),
  },
  {
    name: "stop",
    fn: () => window.BrowserStop(),
  },
  {
    name: "searchInThisPage",
    fn: () => window.gLazyFindCommand("onFindCommand"),
  },
  {
    name: "showNextSearchResult",
    fn: () => window.gLazyFindCommand("onFindAgainCommand", false),
  },
  {
    name: "showPreviousSearchResult",
    fn: () => window.gLazyFindCommand("onFindAgainCommand", true),
  },
  {
    name: "searchTheWeb",
    fn: () => window.BrowserSearch.webSearch(),
  },
  {
    name: "openMigrationWizard",
    fn: () =>
      window.MigrationUtils.showMigrationWizard(window, {
        entrypoint: window.MigrationUtils.MIGRATION_ENTRYPOINTS.FILE_MENU,
      }),
  },
  {
    name: "quitFromApplication",
    fn: () => window.Services.startup.quit(window.Ci.nsIAppStartup.eForceQuit),
  },
  {
    name: "enterIntoCustomizeMode",
    fn: () => window.gCustomizeMode.enter(),
  },
  {
    name: "enterIntoOfflineMode",
    fn: () => window.BrowserOffline.toggleOfflineStatus(),
  },
  {
    name: "openScreenCapture",
    fn: () => window.ScreenshotsUtils.notify(window, "shortcut"),
  },
  {
    name: "showPIP",
    fn: () =>
      window.gFloorpCSKActionFunctions.PictureInPicture.togglePictureInPicture(
        null,
      ),
  },
  {
    name: "openBookmarkAddTool",
    fn: () =>
      window.PlacesUIUtils.showBookmarkPagesDialog(
        window.PlacesCommandHook.uniqueCurrentPages,
      ),
  },
  {
    name: "openBookmarksManager",
    fn: () => window.SidebarController.toggle("viewBookmarksSidebar"),
  },
  {
    name: "toggleBookmarkToolbar",
    fn: () => window.BookmarkingUI.toggleBookmarksToolbar("bookmark-tools"),
  },
  {
    name: "openGeneralPreferences",
    fn: () => window.openPreferences(),
  },
  {
    name: "openPrivacyPreferences",
    fn: () => window.openPreferences("panePrivacy"),
  },
  {
    name: "openWorkspacesPreferences",
    fn: () => window.openPreferences("paneWorkspaces"),
  },
  {
    name: "openContainersPreferences",
    fn: () => window.openPreferences("paneContainers"),
  },
  {
    name: "openSearchPreferences",
    fn: () => window.openPreferences("paneSearch"),
  },
  {
    name: "openSyncPreferences",
    fn: () => window.openPreferences("paneSync"),
  },
  {
    name: "openTaskManager",
    fn: () => window.switchToTabHavingURI("about:processes", true),
  },
  {
    name: "forgetHistory",
    fn: () => window.Sanitizer.showUI(window),
  },
  {
    name: "quickForgetHistory",
    fn: () => window.PlacesUtils.history.clear(true),
  },
  {
    name: "clearRecentHistory",
    fn: () => window.BrowserTryToCloseWindow(),
  },
  {
    name: "restoreLastSession",
    fn: () => window.SessionStore.restoreLastSession(),
  },
  {
    name: "searchHistory",
    fn: () => window.PlacesCommandHook.searchHistory(),
  },
  {
    name: "manageHistory",
    fn: () => window.PlacesCommandHook.showPlacesOrganizer("History"),
  },
  {
    name: "openDownloads",
    fn: () => window.DownloadsPanel.showDownloadsHistory(),
  },
  {
    name: "showBookmarkSidebar",
    fn: () => window.SidebarController.show("viewBookmarksSidebar"),
  },
  {
    name: "showHistorySidebar",
    fn: () => window.SidebarController.show("viewHistorySidebar"),
  },
  {
    name: "showSyncedTabsSidebar",
    fn: () => window.SidebarController.show("viewTabsSidebar"),
  },
  {
    name: "reverseSidebarPosition",
    fn: () => window.SidebarController.reversePosition(),
  },
  {
    name: "hideSidebar",
    fn: () => window.SidebarController.hide(),
  },
  {
    name: "toggleSidebar",
    fn: () => window.SidebarController.toggle(),
  },
  {
    name: "scrollUp",
    fn: () => window.goDoCommand("cmd_scrollPageUp"),
  },
  {
    name: "scrollDown",
    fn: () => window.goDoCommand("cmd_scrollPageDown"),
  },
  {
    name: "scrollRight",
    fn: () => window.goDoCommand("cmd_scrollRight"),
  },
  {
    name: "scrollLeft",
    fn: () => window.goDoCommand("cmd_scrollLeft"),
  },
  {
    name: "scrollToTop",
    fn: () => window.goDoCommand("cmd_scrollTop"),
  },
  {
    name: "scrollToBottom",
    fn: () => window.goDoCommand("cmd_scrollBottom"),
  },
];
