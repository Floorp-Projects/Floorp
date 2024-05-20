import { z } from "zod";

export const csk_category = [
  "tab-action",
  "history-action",
  "page-action",
  "visible-action",
  "search-action",
  "tools-action",
  "pip-action",
  "bookmark-action",
  "open-page-action",
  "sidebar-action",
  "workspaces-action",
  "bms-action",
  "custom-action",
  "downloads-action",
  "split-view-action",
] as const;

const zCommands = z.record(
  z.union([z.string().startsWith("gecko-"), z.string().startsWith("floorp-")]),
  z.object({ command: z.function(), type: z.enum(csk_category) }),
);

type Commands = z.infer<typeof zCommands>;

export const commands: Commands = {
  "gecko-open-new-tab": {
    command: () => window.BrowserOpenTab(),
    type: "tab-action",
  },
  "gecko-close-tab": {
    command: () => window.BrowserCloseTabOrWindow(),
    type: "tab-action",
  },
  "gecko-open-new-window": {
    command: () => window.OpenBrowserWindow(),
    type: "tab-action",
  },
  "gecko-open-new-private-window": {
    command: () => window.OpenBrowserWindow({ private: true }),
    type: "tab-action",
  },
  "gecko-close-window": {
    command: () => window.BrowserTryToCloseWindow(),
    type: "tab-action",
  },
  "gecko-restore-last-session": {
    command: () => window.SessionStore.restoreLastSession(),
    type: "history-action",
  },
  "gecko-restore-last-window": {
    command: () => window.undoCloseWindow(),
    type: "tab-action",
  },
  "gecko-show-next-tab": {
    command: () => window.gBrowser.tabContainer.advanceSelectedTab(1, true),
    type: "tab-action",
  },
  "gecko-show-previous-tab": {
    command: () => window.gBrowser.tabContainer.advanceSelectedTab(-1, true),
    type: "tab-action",
  },
  "gecko-show-all-tabs-panel": {
    command: () => window.gTabsPanel.showAllTabsPanel(),
    type: "tab-action",
  },
  "gecko-send-with-mail": {
    command: () =>
      window.MailIntegration.sendLinkForBrowser(gBrowser.selectedBrowser),
    type: "page-action",
  },
  "gecko-save-page": {
    command: () => window.saveBrowser(gBrowser.selectedBrowser),
    type: "page-action",
  },
  "gecko-print-page": {
    command: () =>
      window.PrintUtils.startPrintWindow(
        gBrowser.selectedBrowser.browsingContext,
      ),
    type: "page-action",
  },
  "gecko-mute-current-tab": {
    command: () =>
      window.gBrowser.toggleMuteAudioOnMultiSelectedTabs(gBrowser.selectedTab),
    type: "page-action",
  },
  "gecko-show-source-of-page": {
    command: () => window.BrowserViewSource(window.gBrowser.selectedBrowser),
    type: "page-action",
  },
  "gecko-show-page-info": {
    command: () => window.BrowserPageInfo(),
    type: "page-action",
  },
  "floorp-rest-mode": {
    command: () => window.gFloorpCommands.enableRestMode(),
    type: "page-action",
  },
  "gecko-zoom-in": {
    command: () => window.FullZoom.enlarge(),
    type: "visible-action",
  },
  "gecko-zoom-out": {
    command: () => window.FullZoom.reduce(),
    type: "visible-action",
  },
  "gecko-reset-zoom": {
    command: () => window.FullZoom.reset(),
    type: "visible-action",
  },
  "floorp-hide-user-interface": {
    command: () => window.gFloorpDesign.hideUserInterface(),
    type: "visible-action",
  },
  "gecko-back": { command: () => window.BrowserBack(), type: "history-action" },
  "gecko-forward": {
    command: () => window.BrowserForward(),
    type: "history-action",
  },
  "gecko-stop": { command: () => window.BrowserStop(), type: "history-action" },
  "gecko-reload": {
    command: () => window.BrowserReload(),
    type: "history-action",
  },
  "gecko-force-reload": {
    command: () => window.BrowserReloadSkipCache(),
    type: "history-action",
  },
  "gecko-search-in-this-page": {
    command: () => window.gLazyFindCommand("onFindCommand"),
    type: "search-action",
  },
  "gecko-show-next-search-result": {
    command: () => window.gLazyFindCommand("onFindAgainCommand", false),
    type: "search-action",
  },
  "gecko-show-previous-search-result": {
    command: () => window.gLazyFindCommand("onFindAgainCommand", true),
    type: "search-action",
  },
  "gecko-search-the-web": {
    command: () => window.BrowserSearch.webSearch(),
    type: "search-action",
  },
  "gecko-open-migration-wizard": {
    command: () =>
      window.MigrationUtils.showMigrationWizard(window, {
        entrypoint: window.MigrationUtils.MIGRATION_ENTRYPOINTS.FILE_MENU,
      }),
    type: "tools-action",
  },
  "gecko-quit-from-application": {
    command: () => window.Services.startup.quit(Ci.nsIAppStartup.eForceQuit),
    type: "tools-action",
  },
  "gecko-enter-into-customize-mode": {
    command: () => window.gCustomizeMode.enter(),
    type: "tools-action",
  },
  "gecko-enter-into-offline-mode": {
    command: () => window.BrowserOffline.toggleOfflineStatus(),
    type: "tools-action",
  },
  "gecko-gecko-open-screen-capture": {
    command: () => window.ScreenshotsUtils.notify(window, "shortcut"),
    type: "tools-action",
  },
  "floorp-show-pip": {
    command: (event: Event) =>
      window.gFloorpCSKActionFunctions.PictureInPicture.togglePictureInPicture(
        event,
      ),
    type: "pip-action",
  },
  "gecko-bookmark-this-page": {
    command: (event: Event) =>
      window.BrowserPageActions.doCommandForAction(
        window.PageActions.actionForID("bookmark"),
        event,
        this,
      ),
    type: "bookmark-action",
  },
  "gecko-open-bookmark-add-tool": {
    command: () =>
      window.PlacesUIUtils.showBookmarkPagesDialog(
        window.PlacesCommandHook.uniqueCurrentPages,
      ),
    type: "bookmark-action",
  },
  "gecko-open-bookmarks-manager": {
    command: () => window.SidebarUI.toggle("viewBookmarksSidebar"),
    type: "bookmark-action",
  },
  "gecko-toggle-bookmark-toolbar": {
    command: () =>
      window.BookmarkingUI.toggleBookmarksToolbar("bookmark-tools"),
    type: "bookmark-action",
  },
  "gecko-open-general-preferences": {
    command: () => window.openPreferences(),
    type: "open-page-action",
  },
  "gecko-open-privacy-preferences": {
    command: () => window.openPreferences("panePrivacy"),
    type: "open-page-action",
  },
  "gecko-open-workspaces-preferences": {
    command: () => window.openPreferences("paneWorkspaces"),
    type: "open-page-action",
  },
  "gecko-open-containers-preferences": {
    command: () => window.openPreferences("paneContainers"),
    type: "open-page-action",
  },
  "gecko-open-search-preferences": {
    command: () => window.openPreferences("paneSearch"),
    type: "open-page-action",
  },
  "gecko-open-sync-preferences": {
    command: () => window.openPreferences("paneSync"),
    type: "open-page-action",
  },
  "gecko-open-task-manager": {
    command: () => window.switchToTabHavingURI("about:processes", true),
    type: "open-page-action",
  },
  "gecko-open-addons-manager": {
    command: () => window.BrowserOpenAddonsMgr(),
    type: "open-page-action",
  },
  "gecko-open-home-page": {
    command: () => window.BrowserHome(),
    type: "open-page-action",
  },
  "gecko-forget-history": {
    command: () => window.Sanitizer.showUI(window),
    type: "history-action",
  },
  "gecko-quick-forget-history": {
    command: () => window.PlacesUtils.history.clear(true),
    type: "history-action",
  },
  "gecko-clear-recent-history": {
    command: () => window.BrowserTryToCloseWindow(),
    type: "history-action",
  },
  "gecko-search-history": {
    command: () => window.PlacesCommandHook.searchHistory(),
    type: "history-action",
  },
  "gecko-manage-history": {
    command: () => window.PlacesCommandHook.showPlacesOrganizer("History"),
    type: "history-action",
  },
  "gecko-open-downloads": {
    command: () => window.DownloadsPanel.showDownloadsHistory(),
    type: "downloads-action",
  },
  "gecko-show-bookmark-sidebar": {
    command: () => window.SidebarUI.show("viewBookmarksSidebar"),
    type: "sidebar-action",
  },
  "gecko-show-history-sidebar": {
    command: () => window.SidebarUI.show("viewHistorySidebar"),
    type: "sidebar-action",
  },
  "gecko-show-synced-tabs-sidebar": {
    command: () => window.SidebarUI.show("viewTabsSidebar"),
    type: "sidebar-action",
  },
  "gecko-reverse-sidebar": {
    command: () => window.SidebarUI.reversePosition(),
    type: "sidebar-action",
  },
  "gecko-hide-sidebar": {
    command: () => window.SidebarUI.hide(),
    type: "sidebar-action",
  },
  "gecko-toggle-sidebar": {
    command: () => window.SidebarUI.toggle(),
    type: "sidebar-action",
  },
  "floorp-open-previous-workspace": {
    command: () => window.gWorkspaces.changeWorkspaceToNextOrBeforeWorkspace(),
    type: "workspaces-action",
  },
  "floorp-open-next-workspace": {
    command: () =>
      window.gWorkspaces.changeWorkspaceToNextOrBeforeWorkspace(true),
    type: "workspaces-action",
  },
  "floorp-show-bsm": {
    command: () =>
      window.gBrowserManagerSidebar.controllFunctions.toggleBMSShortcut(),
    type: "bms-action",
  },
  "floorp-show-panel-1": {
    command: () => window.gBrowserManagerSidebar.contextMenu.showWithNumber(0),
    type: "bms-action",
  },
  "floorp-show-panel-2": {
    command: () => window.gBrowserManagerSidebar.contextMenu.showWithNumber(1),
    type: "bms-action",
  },
  "floorp-show-panel-3": {
    command: () => window.gBrowserManagerSidebar.contextMenu.showWithNumber(2),
    type: "bms-action",
  },
  "floorp-show-panel-4": {
    command: () => window.gBrowserManagerSidebar.contextMenu.showWithNumber(3),
    type: "bms-action",
  },
  "floorp-show-panel-5": {
    command: () => window.gBrowserManagerSidebar.contextMenu.showWithNumber(4),
    type: "bms-action",
  },
  "floorp-show-panel-6": {
    command: () => window.gBrowserManagerSidebar.contextMenu.showWithNumber(5),
    type: "bms-action",
  },
  "floorp-show-panel-7": {
    command: () => window.gBrowserManagerSidebar.contextMenu.showWithNumber(6),
    type: "bms-action",
  },
  "floorp-show-panel-8": {
    command: () => window.gBrowserManagerSidebar.contextMenu.showWithNumber(7),
    type: "bms-action",
  },
  "floorp-show-panel-9": {
    command: () => window.gBrowserManagerSidebar.contextMenu.showWithNumber(8),
    type: "bms-action",
  },
  "floorp-show-panel-10": {
    command: () => window.gBrowserManagerSidebar.contextMenu.showWithNumber(9),
    type: "bms-action",
  },
  "floorp-open-split-view-on-left": {
    command: () =>
      window.gSplitView.Functions.setSplitView(gBrowser.selectedTab, "left"),
    type: "split-view-action",
  },
  "floorp-open-split-view-on-right": {
    command: () =>
      window.gSplitView.Functions.setSplitView(gBrowser.selectedTab, "right"),
    type: "split-view-action",
  },
  "floorp-close-split-view": {
    command: () => window.gSplitView.Functions.removeSplitView(),
    type: "split-view-action",
  },
  "floorp-custom-action-1": {
    command: () =>
      window.gFloorpCustomActionFunctions.evalCustomeActionWithNum(1),
    type: "custom-action",
  },
  "floorp-custom-action-2": {
    command: () =>
      window.gFloorpCustomActionFunctions.evalCustomeActionWithNum(2),
    type: "custom-action",
  },
  "floorp-custom-action-3": {
    command: () =>
      window.gFloorpCustomActionFunctions.evalCustomeActionWithNum(3),
    type: "custom-action",
  },
  "floorp-custom-action-4": {
    command: () =>
      window.gFloorpCustomActionFunctions.evalCustomeActionWithNum(4),
    type: "custom-action",
  },
  "floorp-custom-action-5": {
    command: () =>
      window.gFloorpCustomActionFunctions.evalCustomeActionWithNum(5),
    type: "custom-action",
  },
};
