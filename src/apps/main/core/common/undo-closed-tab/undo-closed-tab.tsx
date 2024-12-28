/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BrowserActionUtils } from "@core/utils/browser-action";
import iconStyle from "./icon.css?inline";

import i18next from "i18next";
import "../../../i18n/config";
import { addI18nObserver } from "../../../i18n/config";
import { createRootHMR } from "@nora/solid-xul";

const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs",
);

export class UndoClosedTab {
  private StyleElement = () => {
    return <style>{iconStyle}</style>;
  };

  constructor() {
    BrowserActionUtils.createToolbarClickActionButton(
      "undo-closed-tab",
      null,
      () => {
        window.undoCloseTab();
      },
      this.StyleElement(),
      CustomizableUI.AREA_NAVBAR,
      2,
      (aNode: XULElement) => {
        const tooltip = document?.createXULElement("tooltip") as XULElement;
        tooltip.id = "undo-closed-tab-tooltip";
        tooltip.hasbeenopened = "false";

        document?.getElementById("mainPopupSet")?.appendChild(tooltip);

        aNode.tooltip = "undo-closed-tab-tooltip";

        createRootHMR(
          () => {
            addI18nObserver((locale) => {
              aNode.label = i18next.t("undo-closed-tab.label", {
                lng: locale,
                ns: "undo"
              });
              tooltip.label = i18next.t("undo-closed-tab.tooltiptext", {
                lng: locale,
                ns: "undo"
              });
            });
          },
          import.meta.hot,
        );
      },
    );
  }
}
