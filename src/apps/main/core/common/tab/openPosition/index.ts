/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "@core/common/designs/configs";
import { createEffect } from "solid-js";

export class TabOpenPosition {
  private static instance: TabOpenPosition;
  public static getInstance() {
    if (!TabOpenPosition.instance) {
      TabOpenPosition.instance = new TabOpenPosition();
    }
    return TabOpenPosition.instance;
  }

  constructor() {
    createEffect(() => {
      const option = config().tab.tabOpenPosition;
      console.log(option);
      Services.prefs.setIntPref("floorp.browser.tabs.openNewTabPosition", option);
    });
  }
}
