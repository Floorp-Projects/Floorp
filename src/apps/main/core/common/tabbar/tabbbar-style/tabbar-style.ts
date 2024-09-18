/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect } from "solid-js";
import { gTabbarStyleFunctions } from "./tabbbar-style-functions";
import { checkPaddingEnabled } from "./titilebar-padding";
import { handleOnWheel } from "./tabbar-on-wheel";

export class gTabbarStyleClass {
  private static instance: gTabbarStyleClass;
  public static getInstance() {
    if (!gTabbarStyleClass.instance) {
      gTabbarStyleClass.instance = new gTabbarStyleClass();
    }
    return gTabbarStyleClass.instance;
  }

  private get tabbarWindowManageContainer() {
    return document.querySelector(
      "#TabsToolbar > .titlebar-buttonbox-container",
    );
  }

  constructor() {
    this.tabbarWindowManageContainer?.setAttribute(
      "id",
      "floorp-tabbar-window-manage-container",
    );

    gTabbarStyleFunctions.applyTabbarStyle();
    checkPaddingEnabled();

    const tabBrowserTabs = document.querySelector(
      "#tabbrowser-tabs",
    ) as XULElement;
    if (tabBrowserTabs) {
      tabBrowserTabs.on_wheel = (event: WheelEvent) => {
        handleOnWheel(event, tabBrowserTabs);
      };
    }

    createEffect(() => {
      gTabbarStyleFunctions.applyTabbarStyle();
      checkPaddingEnabled();
    });
  }
}
