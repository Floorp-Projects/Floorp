/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SsbPageAction } from "./SsbPageAction";
import { SsbPanelView } from "./SsbPanelView";
import { enabled } from "./config";
import { PwaWindowSupport } from "./pwa-window";
import { SiteSpecificBrowserManager } from "./ssbManager";

export function init() {
  if (!enabled()) return;
  SsbPageAction.getInstance();
  SsbPanelView.getInstance();
  PwaWindowSupport.getInstance();
  SiteSpecificBrowserManager.getInstance();

  import.meta.hot?.accept((m) => {
    m?.init();
  });
}
