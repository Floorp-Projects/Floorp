/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BrowserActionUtils } from "@core/utils/browser-action";
import iconStyle from "./icon.css?inline";

import type { TCustomizableUI } from "@types-gecko/CustomizableUI";

const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs",
) as { CustomizableUI: typeof TCustomizableUI };

export class ReverseSidebarPosition {
  private StyleElement = () => {
    return <style>{iconStyle}</style>;
  };

  constructor() {
    BrowserActionUtils.createToolbarClickActionButton(
      "sidebar-reverse-position-toolbar",
      "sidebar-reverse-position-toolbar",
      () => {
        window.SidebarUI.reversePosition();
      },
      this.StyleElement(),
      CustomizableUI.AREA_NAVBAR,
      1,
      () => {
        // Sidebar button should right side of navbar
        CustomizableUI.addWidgetToArea(
          "sidebar-button",
          CustomizableUI.AREA_NAVBAR,
          0,
        );
      },
    );
  }
}
