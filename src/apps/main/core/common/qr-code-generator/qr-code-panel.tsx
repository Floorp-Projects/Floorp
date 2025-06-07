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

  const handlePopupShowing = () => {
    // gFloorpPageAction.qrCode.onPopupShowing() equivalent
    const url = manager.currentUrl();
    if (url) {
      setTimeout(async () => {
        const container = document?.getElementById(
          "qrcode-img-vbox",
        ) as HTMLElement;
        if (container) {
          await manager.generateQRCode(url, container);
        }
      }, 100);
    }
  };

  return (
    <xul:panel
      id="qrcode-panel"
      type="arrow"
      position="bottomright topright"
      class="rounded-lg"
      onPopupShowing={handlePopupShowing}
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

        {
          /* {manager.currentUrl() && (
          <xul:vbox class="px-4 pb-2 space-y-2">
            <xul:label class="text-sm font-medium text-gray-600 dark:text-gray-400">
              URL:
            </xul:label>
            <xul:description class="w-full p-2 bg-gray-100 dark:bg-gray-600 border border-gray-300 dark:border-gray-500 rounded-md text-xs text-gray-800 dark:text-gray-200 break-all max-h-16 overflow-y-auto font-mono">
              {manager.currentUrl()}
            </xul:description>
          </xul:vbox>
        )} */
        }

        {
          /* <xul:hbox class="flex gap-2 p-4 pt-2 justify-center">
          <xul:button
            label="PNG"
            class="px-3 py-1.5 bg-blue-500 hover:bg-blue-600 text-white text-xs rounded-md font-medium transition-colors shadow-sm"
            onCommand={() => manager.downloadQRCode("png")}
          />
          <xul:button
            label="JPEG"
            class="px-3 py-1.5 bg-green-500 hover:bg-green-600 text-white text-xs rounded-md font-medium transition-colors shadow-sm"
            onCommand={() => manager.downloadQRCode("jpeg")}
          />
          <xul:button
            label="SVG"
            class="px-3 py-1.5 bg-purple-500 hover:bg-purple-600 text-white text-xs rounded-md font-medium transition-colors shadow-sm"
            onCommand={() => manager.downloadQRCode("svg")}
          />
        </xul:hbox> */
        }
      </xul:vbox>
    </xul:panel>
  );
}
