/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 import {
  createEffect,
  createSignal,
  onCleanup
} from "solid-js";
import type { } from "solid-styled-jsx";

export class TabColorManager {
  _enableTabColor = createSignal(
    Services.prefs.getBoolPref("noraneko.tabcolor.enable", false),
  );
  enableTabColor = this._enableTabColor[0];
  setEnableTabColor = this._enableTabColor[1];
  constructor() {
    //? this effect will not called when pref is changed to same value.
    Services.prefs.addObserver(
      "noraneko.tabcolor.enable",
      this.observerTabcolorPref,
    );
    onCleanup(() => {
      Services.prefs.removeObserver(
        "noraneko.tabcolor.enable",
        this.observerTabcolorPref,
      );
    });
    if (!window.gFloorp) {
      window.gFloorp = {};
    }
    window.gFloorp.tabColor = {
      setEnable: this.setEnableTabColor,
    };
  }

  init() {
    createEffect(() => {
      console.log("solid to pref");
      Services.prefs.setBoolPref(
        "noraneko.tabcolor.enable",
        this.enableTabColor(),
      );
    });
  }

  //if we use just method, `this` will be broken
  private observerTabcolorPref = () => {
    console.log("pref to solid");
    this.setEnableTabColor((_prev) => {
      return Services.prefs.getBoolPref("noraneko.tabcolor.enable");
    });
  };
}
