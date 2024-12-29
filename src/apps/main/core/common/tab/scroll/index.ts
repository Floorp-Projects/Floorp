/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "@core/common/designs/configs";
import { createEffect } from "solid-js";

type XULTabElement = XULElement & {
  onwheel?: EventHandler;
  advanceSelectedTab: (aDelta: number, aWrap: boolean) => void;
  on_wheel?: EventHandler;
};

export class TabScroll {
  private handleOnWheel = (
    event: WheelEvent,
    tabBrowserTabs: XULTabElement,
  ) => {
    if (Services.prefs.getBoolPref("toolkit.tabbox.switchByScrolling")) {
      if (event.deltaY > 0 !== config().tab.tabScroll.reverse) {
        tabBrowserTabs?.advanceSelectedTab(
          1,
          config().tab.tabScroll.reverse,
        );
      } else {
        tabBrowserTabs?.advanceSelectedTab(-1, config().tab.tabScroll.wrap);
      }
      event.preventDefault();
      event.stopPropagation();
    }
  };

  constructor() {
    const tabBrowserTabs = document?.querySelector(
      "#tabbrowser-tabs",
    ) as XULTabElement;
    if (tabBrowserTabs) {
      tabBrowserTabs.on_wheel = (event: WheelEvent) => {
        this.handleOnWheel(event, tabBrowserTabs);
      };
    }

    createEffect(() => {
      const isEnable = config().tab.tabScroll.enabled;
      Services.prefs.setBoolPref("toolkit.tabbox.switchByScrolling", isEnable);
    });
  }
}
