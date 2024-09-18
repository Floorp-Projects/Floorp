/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "../../designs/configs";

export const handleOnWheel = (
  event: WheelEvent,
  tabBrowserTabs: XULElement,
) => {
  if (Services.prefs.getBoolPref("toolkit.tabbox.switchByScrolling")) {
    if (event.deltaY > 0 !== config().tabbar.tabScroll.reverse) {
      tabBrowserTabs?.advanceSelectedTab(1, config().tabbar.tabScroll.reverse);
    } else {
      tabBrowserTabs?.advanceSelectedTab(-1, config().tabbar.tabScroll.wrap);
    }
    event.preventDefault();
    event.stopPropagation();
  }
};
