/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";

interface ActionOption {
  id: string;
  name: string;
}

export const GESTURE_ACTIONS = [
  "goBack",
  "goForward",
  "reload",
  "closeTab",
  "newTab",
  "duplicateTab",
  "reloadAllTabs",
  "reopenClosedTab",
  "openNewWindow",
  "openNewPrivateWindow",
  "closeWindow",
  "restoreLastWindow",
  "showNextTab",
  "showPreviousTab",
  "showAllTabsPanel",
  "forceReload",
  "zoomIn",
  "zoomOut",
  "resetZoom",
  "bookmarkThisPage",
  "openHomePage",
  "openAddonsManager",
  "restoreLastTab",
  "sendWithMail",
  "savePage",
  "printPage",
  "muteCurrentTab",
  "showSourceOfPage",
  "showPageInfo",
  "EnableRestMode",
  "hideInterface",
  "toggleNavigationPanel",
  "stop",
  "searchInThisPage",
  "showNextSearchResult",
  "showPreviousSearchResult",
  "searchTheWeb",
  "openMigrationWizard",
  "quitFromApplication",
  "enterIntoCustomizeMode",
  "enterIntoOfflineMode",
  "openScreenCapture",
  "showPIP",
  "openBookmarkAddTool",
  "openBookmarksManager",
  "toggleBookmarkToolbar",
  "openGeneralPreferences",
  "openPrivacyPreferences",
  "openWorkspacesPreferences",
  "openContainersPreferences",
  "openSearchPreferences",
  "openSyncPreferences",
  "openTaskManager",
  "forgetHistory",
  "quickForgetHistory",
  "clearRecentHistory",
  "restoreLastSession",
  "searchHistory",
  "manageHistory",
  "openDownloads",
  "showBookmarkSidebar",
  "showHistorySidebar",
  "showSyncedTabsSidebar",
  "reverseSidebarPosition",
  "hideSidebar",
  "toggleSidebar",
  "scrollUp",
  "scrollDown",
  "scrollRight",
  "scrollLeft",
  "scrollToTop",
  "scrollToBottom",
] as const;

export const useAvailableActions = () => {
  const { t } = useTranslation();
  const [actions, setActions] = useState<ActionOption[]>([]);

  useEffect(() => {
    const translatedActions = GESTURE_ACTIONS.map((id) => ({
      id,
      name: t(`mouseGesture.actions.${id}`, id),
    }));

    setActions(translatedActions);
  }, [t]);

  return actions;
};
