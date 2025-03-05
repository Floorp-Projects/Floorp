/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal } from "solid-js";
import type {} from "solid-styled-jsx";
import { config } from "@core/common/designs/configs.ts";

export class TabColorManager {
  _enableTabColor = createSignal(config().globalConfigs.faviconColor);
  enableTabColor = this._enableTabColor[0];
  setEnableTabColor = this._enableTabColor[1];

  constructor() {
    if (!globalThis.gFloorp) {
      globalThis.gFloorp = {};
    }
    globalThis.gFloorp.tabColor = {
      setEnable: this.setEnableTabColor,
    };
  }

  init() {
    createEffect(() => {
      const currentConfig = config();
      console.log(currentConfig);
      if (currentConfig.globalConfigs.faviconColor !== this.enableTabColor()) {
        this.setEnableTabColor(currentConfig.globalConfigs.faviconColor);
      }
    });
  }
}
