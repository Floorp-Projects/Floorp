/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "@core/utils/base";
import { SsbPageAction } from "./SsbPageAction";
import { SsbPanelView } from "./SsbPanelView";
import { enabled } from "./config";
import { PwaWindowSupport } from "./pwa-window";
import { PwaService } from "./pwaService";
import { ManifestProcesser } from "./manifestProcesser";
import { DataManager } from "./dataStore";
import { SiteSpecificBrowserManager } from "./ssbManager";

@noraComponent(import.meta.hot)
export default class Pwa extends NoraComponentBase {
  static ctx: PwaService | null = null;

  init() {
    if (!enabled()) return;
    const manifestProcesser = new ManifestProcesser();
    const dataManager = new DataManager();
    const ssbManager = new SiteSpecificBrowserManager(manifestProcesser, dataManager);
    const ctx = new PwaService(ssbManager, manifestProcesser, dataManager);

    new SsbPageAction(ctx);
    new SsbPanelView(ctx);
    new PwaWindowSupport(ctx);

    Pwa.ctx = ctx;
  }
}
