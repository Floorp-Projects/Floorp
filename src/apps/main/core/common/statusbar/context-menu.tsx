/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { manager } from "./index.ts";
import i18next from "i18next";
import { createSignal } from "solid-js";
import { addI18nObserver } from "../../../i18n/config.ts";

const TRANSLATION_KEY = "statusbar.toggle";

export function ContextMenu() {
  const [label, setLabel] = createSignal(i18next.t(TRANSLATION_KEY));

  addI18nObserver(() => {
    setLabel(i18next.t(TRANSLATION_KEY, { mark: "(S)" }));
  });

  return (
    <xul:menuitem
      data-l10n-id="status-bar"
      label={label()}
      type="checkbox"
      id="toggle_statusBar"
      data-toolbar-id="nora-statusbar"
      checked={manager.showStatusBar()}
      onCommand={() => manager.setShowStatusBar((prevValue: boolean) => !prevValue)}
    />
  );
}
