/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal } from "solid-js";
import type {} from "solid-styled-jsx";

export const [showStatusbar, setShowStatusbar] = createSignal(
  Services.prefs.getBoolPref("browser.display.statusbar", false),
);

createEffect(() => {
  Services.prefs.setBoolPref("browser.display.statusbar", showStatusbar());
  const statuspanel_label = document.getElementById("statuspanel-label");
  if (showStatusbar()) {
    document.getElementById("status-text")?.appendChild(statuspanel_label!);
  } else {
    document.getElementById("statuspanel")?.appendChild(statuspanel_label!);
  }
});

export const gFloorpStatusBar = {
  init() {
    window.CustomizableUI.registerArea("statusBar", {
      type: window.CustomizableUI.TYPE_TOOLBAR,
      defaultPlacements: ["screenshot-button", "fullscreen-button"],
    });
    window.CustomizableUI.registerToolbarNode(
      document.getElementById("statusBar"),
    );

    //move elem to bottom of window
    document.body?.appendChild(document.getElementById("statusBar")!);
    this.observeStatusbar();
  },

  observeStatusbar() {
    Services.prefs.addObserver("browser.display.statusbar", () =>
      setShowStatusbar(() =>
        Services.prefs.getBoolPref("browser.display.statusbar", false),
      ),
    );
  },
};
