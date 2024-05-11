export const keyboardShortcutActions = {
  /*
   * these are the actions that can be triggered by keyboard shortcuts.
   * 1st element of each array is the code that will be executed.
   * 2nd element is use for Fluent localization.
   * 3rd is the type of action. You can use it to filter actions or get all actions of a type.
   * If you want to add a new action, you need to add it here.
   */

  // manage actions
  openNewTab: ["BrowserOpenTab()", "open-new-tab", "tab-action"],
  closeTab: ["BrowserCloseTabOrWindow()", "close-tab", "tab-action"],
  openNewWindow: ["OpenBrowserWindow()", "open-new-window", "tab-action"],
  openNewPrivateWindow: [
    "OpenBrowserWindow({private: true})",
    "open-new-private-window",
    "tab-action",
  ],
  closeWindow: ["BrowserTryToCloseWindow()", "close-window", "tab-action"],
  restoreLastTab: ["undoCloseTab()", "restore-last-session", "tab-action"],
  restoreLastWindow: ["undoCloseWindow()", "restore-last-window", "tab-action"],
  showNextTab: [
    "gBrowser.tabContainer.advanceSelectedTab(1, true)",
    "show-next-tab",
    "tab-action",
  ],
  showPreviousTab: [
    "gBrowser.tabContainer.advanceSelectedTab(-1, true)",
    "show-previous-tab",
    "tab-action",
  ],
  showAllTabsPanel: [
    "gTabsPanel.showAllTabsPanel()",
    "show-all-tabs-panel",
    "tab-action",
  ],

  // Page actions
  sendWithMail: [
    "MailIntegration.sendLinkForBrowser(gBrowser.selectedBrowser)",
    "send-with-mail",
    "page-action",
  ],
  savePage: [
    "saveBrowser(gBrowser.selectedBrowser)",
    "save-page",
    "page-action",
  ],
  printPage: [
    "PrintUtils.startPrintWindow(gBrowser.selectedBrowser.browsingContext)",
    "print-page",
    "page-action",
  ],
  muteCurrentTab: [
    "gBrowser.toggleMuteAudioOnMultiSelectedTabs(gBrowser.selectedTab)",
    "mute-current-tab",
    "page-action",
  ],
  showSourceOfPage: [
    "BrowserViewSource(window.gBrowser.selectedBrowser)",
    "show-source-of-page",
    "page-action",
  ],
  showPageInfo: ["BrowserPageInfo()", "show-page-info", "page-action"],
  EnableRestMode: [
    "gFloorpCommands.enableRestMode();",
    "rest-mode",
    "page-action",
  ],

  // Visible actions
  zoomIn: ["FullZoom.enlarge()", "zoom-in", "visible-action"],
  zoomOut: ["FullZoom.reduce()", "zoom-out", "visible-action"],
  resetZoom: ["FullZoom.reset()", "reset-zoom", "visible-action"],
  hideInterface: [
    "gFloorpDesign.hideUserInterface()",
    "hide-user-interface",
    "visible-action",
  ],

  // History actions
  back: ["BrowserBack()", "back", "history-action"],
  forward: ["BrowserForward()", "forward", "history-action"],
  stop: ["BrowserStop()", "stop", "history-action"],
  reload: ["BrowserReload()", "reload", "history-action"],
  forceReload: ["BrowserReloadSkipCache()", "force-reload", "history-action"],

  // search actions
  searchInThisPage: [
    "gLazyFindCommand('onFindCommand')",
    "search-in-this-page",
    "search-action",
  ],
  showNextSearchResult: [
    "gLazyFindCommand('onFindAgainCommand', false)",
    "show-next-search-result",
    "search-action",
  ],
  showPreviousSearchResult: [
    "gLazyFindCommand('onFindAgainCommand', true)",
    "show-previous-search-result",
    "search-action",
  ],
  searchTheWeb: [
    "BrowserSearch.webSearch()",
    "search-the-web",
    "search-action",
  ],

  // Tools actions
  openMigrationWizard: [
    "MigrationUtils.showMigrationWizard(window, { entrypoint: MigrationUtils.MIGRATION_ENTRYPOINTS.FILE_MENU })",
    "open-migration-wizard",
    "tools-action",
  ],
  quitFromApplication: [
    "Services.startup.quit(Ci.nsIAppStartup.eForceQuit);",
    "quit-from-application",
    "tools-action",
  ],
  enterIntoCustomizeMode: [
    "gCustomizeMode.enter()",
    "enter-into-customize-mode",
    "tools-action",
  ],
  enterIntoOfflineMode: [
    "BrowserOffline.toggleOfflineStatus()",
    "enter-into-offline-mode",
    "tools-action",
  ],
  openScreenCapture: [
    "ScreenshotsUtils.notify(window, 'shortcut')",
    "open-screen-capture",
    "tools-action",
  ],

  // PIP actions
  showPIP: [
    "gFloorpCSKActionFunctions.PictureInPicture.togglePictureInPicture(event)",
    "show-pip",
    "pip-action",
  ],

  // Bookmark actions
  bookmarkThisPage: [
    "BrowserPageActions.doCommandForAction(PageActions.actionForID('bookmark'), event, this);",
    "bookmark-this-page",
    "bookmark-action",
  ],
  openBookmarkAddTool: [
    "PlacesUIUtils.showBookmarkPagesDialog(PlacesCommandHook.uniqueCurrentPages)",
    "open-bookmark-add-tool",
    "bookmark-action",
  ],
  openBookmarksManager: [
    "SidebarUI.toggle('viewBookmarksSidebar');",
    "open-bookmarks-manager",
    "bookmark-action",
  ],
  toggleBookmarkToolbar: [
    "BookmarkingUI.toggleBookmarksToolbar('bookmark-tools')",
    "toggle-bookmark-toolbar",
    "bookmark-action",
  ],

  // Open Page actions
  openGeneralPreferences: [
    "openPreferences()",
    "open-general-preferences",
    "open-page-action",
  ],
  openPrivacyPreferences: [
    "openPreferences('panePrivacy')",
    "open-privacy-preferences",
    "open-page-action",
  ],
  openWorkspacesPreferences: [
    "openPreferences('paneWorkspaces')",
    "open-workspaces-preferences",
    "open-page-action",
  ],
  openContainersPreferences: [
    "openPreferences('paneContainers')",
    "open-containers-preferences",
    "open-page-action",
  ],
  openSearchPreferences: [
    "openPreferences('paneSearch')",
    "open-search-preferences",
    "open-page-action",
  ],
  openSyncPreferences: [
    "openPreferences('paneSync')",
    "open-sync-preferences",
    "open-page-action",
  ],
  openTaskManager: [
    "switchToTabHavingURI('about:processes', true)",
    "open-task-manager",
    "open-page-action",
  ],
  openAddonsManager: [
    "BrowserOpenAddonsMgr()",
    "open-addons-manager",
    "open-page-action",
  ],
  openHomePage: ["BrowserHome()", "open-home-page", "open-page-action"],

  // History actions
  forgetHistory: [
    "Sanitizer.showUI(window)",
    "forget-history",
    "history-action",
  ],
  quickForgetHistory: [
    "PlacesUtils.history.clear(true)",
    "quick-forget-history",
    "history-action",
  ],
  clearRecentHistory: [
    "BrowserTryToCloseWindow()",
    "clear-recent-history",
    "history-action",
  ],
  restoreLastSession: [
    "SessionStore.restoreLastSession()",
    "restore-last-session",
    "history-action",
  ],
  searchHistory: [
    "PlacesCommandHook.searchHistory()",
    "search-history",
    "history-action",
  ],
  manageHistory: [
    "PlacesCommandHook.showPlacesOrganizer('History')",
    "manage-history",
    "history-action",
  ],

  // Downloads actions
  openDownloads: [
    "DownloadsPanel.showDownloadsHistory()",
    "open-downloads",
    "downloads-action",
  ],

  // Sidebar actions
  showBookmarkSidebar: [
    "SidebarUI.show('viewBookmarksSidebar')",
    "show-bookmark-sidebar",
    "sidebar-action",
  ],
  showHistorySidebar: [
    "SidebarUI.show('viewHistorySidebar')",
    "show-history-sidebar",
    "sidebar-action",
  ],
  showSyncedTabsSidebar: [
    "SidebarUI.show('viewTabsSidebar')",
    "show-synced-tabs-sidebar",
    "sidebar-action",
  ],
  reverseSidebarPosition: [
    "SidebarUI.reversePosition()",
    "reverse-sidebar",
    "sidebar-action",
  ],
  hideSidebar: ["SidebarUI.hide()", "hide-sidebar", "sidebar-action"],
  toggleSidebar: ["SidebarUI.toggle()", "toggle-sidebar", "sidebar-action"],

  // Workspaces actions
  changeWorkspaceToPrevious: [
    "gWorkspaces.changeWorkspaceToNextOrBeforeWorkspace()",
    "open-previous-workspace",
    "workspaces-action",
  ],
  changeWorkspaceToNext: [
    "gWorkspaces.changeWorkspaceToNextOrBeforeWorkspace(true)",
    "open-next-workspace",
    "workspaces-action",
  ],

  // BMS actions
  toggleBMS: [
    "gBrowserManagerSidebar.controllFunctions.toggleBMSShortcut()",
    "show-bsm",
    "bms-action",
  ],
  showPanel1: [
    "gBrowserManagerSidebar.contextMenu.showWithNumber(0)",
    "show-panel-1",
    "bms-action",
  ],
  showPanel2: [
    "gBrowserManagerSidebar.contextMenu.showWithNumber(1)",
    "show-panel-2",
    "bms-action",
  ],
  showPanel3: [
    "gBrowserManagerSidebar.contextMenu.showWithNumber(2)",
    "show-panel-3",
    "bms-action",
  ],
  showPanel4: [
    "gBrowserManagerSidebar.contextMenu.showWithNumber(3)",
    "show-panel-4",
    "bms-action",
  ],
  showPanel5: [
    "gBrowserManagerSidebar.contextMenu.showWithNumber(4)",
    "show-panel-5",
    "bms-action",
  ],
  showPanel6: [
    "gBrowserManagerSidebar.contextMenu.showWithNumber(5)",
    "show-panel-6",
    "bms-action",
  ],
  showPanel7: [
    "gBrowserManagerSidebar.contextMenu.showWithNumber(6)",
    "show-panel-7",
    "bms-action",
  ],
  showPanel8: [
    "gBrowserManagerSidebar.contextMenu.showWithNumber(7)",
    "show-panel-8",
    "bms-action",
  ],
  showPanel9: [
    "gBrowserManagerSidebar.contextMenu.showWithNumber(8)",
    "show-panel-9",
    "bms-action",
  ],
  showPanel10: [
    "gBrowserManagerSidebar.contextMenu.showWithNumber(9)",
    "show-panel-10",
    "bms-action",
  ],

  // Split View actions
  openSplitViewOnLeft: [
    "gSplitView.Functions.setSplitView(gBrowser.selectedTab, 'left')",
    "open-split-view-on-left",
    "split-view-action",
  ],
  openSplitViewOnRight: [
    "gSplitView.Functions.setSplitView(gBrowser.selectedTab, 'right')",
    "open-split-view-on-right",
    "split-view-action",
  ],
  closeSplitView: [
    "gSplitView.Functions.removeSplitView()",
    "close-split-view",
    "split-view-action",
  ],

  // Custom actions
  customAction1: [
    "gFloorpCustomActionFunctions.evalCustomeActionWithNum(1)",
    "custom-action-1",
    "custom-action",
  ],
  customAction2: [
    "gFloorpCustomActionFunctions.evalCustomeActionWithNum(2)",
    "custom-action-2",
    "custom-action",
  ],
  customAction3: [
    "gFloorpCustomActionFunctions.evalCustomeActionWithNum(3)",
    "custom-action-3",
    "custom-action",
  ],
  customAction4: [
    "gFloorpCustomActionFunctions.evalCustomeActionWithNum(4)",
    "custom-action-4",
    "custom-action",
  ],
  customAction5: [
    "gFloorpCustomActionFunctions.evalCustomeActionWithNum(5)",
    "custom-action-5",
    "custom-action",
  ],
};
