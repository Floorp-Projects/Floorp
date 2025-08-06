/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { manager } from "./index.ts";
import i18next from "i18next";
import { createSignal } from "solid-js";
import { addI18nObserver } from "../../../i18n/config.ts";

export function QRCodePanel() {
  const [title, setTitle] = createSignal(
    i18next.t("qrcode-generate-page-action-title"),
  );

  addI18nObserver(() => {
    setTitle(i18next.t("qrcode-generate-page-action-title"));
  });

  return (
    <xul:panel
      id="qrcode-panel"
      type="arrow"
      position="bottomright topright"
      class="rounded-lg"
      onPopupShowing={() => manager.handlePopupShowing()}
    >
      <xul:vbox id="qrcode-box" class="p-0">
        <xul:vbox class="panel-header border-b p-3 rounded-t-lg">
          <xul:label
            data-l10n-id="qrcode-generate-page-action-title"
            class="m-0 text-xl font-semibold text-gray-800 dark:text-gray-200"
          >
            {title()}
          </xul:label>
        </xul:vbox>

        <xul:toolbarseparator class="border-t" />

        <xul:vbox
          id="qrcode-img-vbox"
          class="p-2 flex items-center justify-center rounded-lg m-3 shadow-inner min-h-64"
        />
      </xul:vbox>
    </xul:panel>
  );
}
