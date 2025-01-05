/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "@core/utils/base";
import { SsbPageAction } from "./SsbPageAction";
import { SsbPanelView } from "./SsbPanelView";
import { enabled } from "./config";
import { PwaWindowSupport } from "./pwa-window";
import { SiteSpecificBrowserManager } from "./ssbManager";

@noraComponent(import.meta.hot)
export default class Pwa extends NoraComponentBase {
  init() {
    if (!enabled()) return;
    SsbPageAction.getInstance();
    SsbPanelView.getInstance();
    PwaWindowSupport.getInstance();
    SiteSpecificBrowserManager.getInstance();
  }
}
