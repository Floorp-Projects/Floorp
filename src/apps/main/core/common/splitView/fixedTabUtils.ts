/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SplitView } from "./splitView";
import { fixedSplitViewData } from "./utils/data";
import type { Tab } from "./utils/type";

export namespace FixedTabUtils {
  export function isWorking(): boolean {
    return fixedSplitViewData().fixedTabId !== null;
  }

  export function getFixedTabId(): string | null {
    return fixedSplitViewData().fixedTabId;
  }

  export function getFixedTab(): Tab | null {
    const gSplitView = SplitView.getInstance();
    const tabId = getFixedTabId();
    if (tabId) {
      return gSplitView.getTabById(tabId);
    }
    return null;
  }
}
