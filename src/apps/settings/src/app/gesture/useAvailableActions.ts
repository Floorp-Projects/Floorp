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
  "gecko-back",
  "gecko-forward",
  "gecko-reload",
  "gecko-close-tab",
  "gecko-open-new-tab",
  "gecko-duplicate-tab",
  "gecko-reload-all-tabs",
  "gecko-restore-last-tab",
  "gecko-open-new-window",
  "gecko-open-new-private-window",
  "gecko-close-window",
  "gecko-restore-last-window",
  "gecko-show-next-tab",
  "gecko-show-previous-tab",
  "gecko-show-all-tabs-panel",
  "gecko-force-reload",
  "gecko-zoom-in",
  "gecko-zoom-out",
  "gecko-reset-zoom",
  "gecko-bookmark-this-page",
  "gecko-open-home-page",
  "gecko-open-addons-manager",
  "gecko-restore-last-tab",
  "gecko-send-with-mail",
  "gecko-save-page",
  "gecko-print-page",
  "gecko-mute-current-tab",
  "gecko-show-source-of-page",
  "gecko-show-page-info",
  "floorp-rest-mode",
  "floorp-hide-user-interface",
  "floorp-toggle-navigation-panel",
  "gecko-stop",
  "gecko-search-in-this-page",
  "gecko-show-next-search-result",
  "gecko-show-previous-search-result",
  "gecko-search-the-web",
  "gecko-open-migration-wizard",
  "gecko-quit-from-application",
  "gecko-enter-into-customize-mode",
  "gecko-enter-into-offline-mode",
  "gecko-open-screen-capture",
  "floorp-show-pip",
  "gecko-open-bookmark-add-tool",
  "gecko-open-bookmarks-manager",
  "gecko-toggle-bookmark-toolbar",
  "gecko-open-general-preferences",
  "gecko-open-privacy-preferences",
  "gecko-open-workspaces-preferences",
  "gecko-open-containers-preferences",
  "gecko-open-search-preferences",
  "gecko-open-sync-preferences",
  "gecko-open-task-manager",
  "gecko-forget-history",
  "gecko-quick-forget-history",
  "gecko-clear-recent-history",
  "gecko-restore-last-session",
  "gecko-search-history",
  "gecko-manage-history",
  "gecko-open-downloads",
  "gecko-show-bookmark-sidebar",
  "gecko-show-history-sidebar",
  "gecko-show-synced-tabs-sidebar",
  "gecko-reverse-sidebar",
  "gecko-hide-sidebar",
  "gecko-toggle-sidebar",
  "gecko-scroll-up",
  "gecko-scroll-down",
  "gecko-scroll-right",
  "gecko-scroll-left",
  "gecko-scroll-to-top",
  "gecko-scroll-to-bottom",
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
