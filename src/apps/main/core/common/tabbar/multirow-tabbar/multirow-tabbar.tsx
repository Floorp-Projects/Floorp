/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "../../designs/configs.ts";
import { createEffect } from "solid-js";

export class MultirowTabbarClass {
  private get arrowScrollbox(): XULElement | null {
    return document?.querySelector("#tabbrowser-arrowscrollbox") || null;
  }

  private get tabsToolbar(): XULElement | null {
    return document?.getElementById("TabsToolbar") as XULElement | null;
  }

  private get scrollboxPart(): XULElement | null {
    return this.arrowScrollbox
      ? this.arrowScrollbox.shadowRoot?.querySelector(
        "[part='items-wrapper']",
      ) || null
      : null;
  }

  private get aTabHeight(): number {
    return (
      document?.querySelector(".tabbrowser-tab:not([hidden='true'])")
        ?.clientHeight || 30
    );
  }

  private get isMaxRowEnabled(): boolean {
    return config().tabbar.multiRowTabBar.maxRowEnabled;
  }
  private get getMultirowTabMaxHeight(): number {
    return config().tabbar.multiRowTabBar.maxRow * this.aTabHeight;
  }

  private setMultirowTabMaxHeight() {
    console.log("setMultirowTabMaxHeight", this.isMaxRowEnabled);

    if (!this.isMaxRowEnabled) {
      return;
    }
    this.scrollboxPart?.setAttribute(
      "style",
      `max-height: ${this.getMultirowTabMaxHeight}px !important;`,
    );
    this.arrowScrollbox?.style.setProperty(
      "max-height",
      `${this.getMultirowTabMaxHeight}px`,
    );
  }

  private removeMultirowTabMaxHeight() {
    this.scrollboxPart?.removeAttribute("style");
    this.arrowScrollbox?.style.removeProperty("max-height");
  }

  private enableMultirowTabbar() {
    console.log("Enabling multirow tabbar");
    this.scrollboxPart?.setAttribute("style", "flex-wrap: wrap;");
    this.tabsToolbar?.setAttribute("multibar", "true");
  }

  private disableMultirowTabbar() {
    console.log("Disabling multirow tabbar");
    this.scrollboxPart?.removeAttribute("style");
    this.tabsToolbar?.removeAttribute("multibar");
  }

  constructor() {
    createEffect(() => {
      if (config().tabbar.tabbarStyle === "multirow") {
        this.enableMultirowTabbar();
        this.setMultirowTabMaxHeight();
      } else {
        this.disableMultirowTabbar();
        this.removeMultirowTabMaxHeight();
      }
    });
  }
}
