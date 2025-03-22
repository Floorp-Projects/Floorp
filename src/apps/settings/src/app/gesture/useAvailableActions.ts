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

const GESTURE_ACTIONS = [
  { id: "goBack", defaultName: "Go Back" },
  { id: "goForward", defaultName: "Go Forward" },
  { id: "reload", defaultName: "Reload" },
  { id: "closeTab", defaultName: "Close Tab" },
  { id: "newTab", defaultName: "New Tab" },
  { id: "duplicateTab", defaultName: "Duplicate Tab" },
  { id: "reloadAllTabs", defaultName: "Reload All Tabs" },
  { id: "reopenClosedTab", defaultName: "Reopen Closed Tab" },
] as const;

export const useAvailableActions = () => {
  const { t } = useTranslation();
  const [actions, setActions] = useState<ActionOption[]>([]);

  useEffect(() => {
    const translatedActions = GESTURE_ACTIONS.map(({ id, defaultName }) => ({
      id,
      name: t(`mouseGesture.actions.${id}`, defaultName),
    }));

    setActions(translatedActions);
  }, [t]);

  return actions;
};
