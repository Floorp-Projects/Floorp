/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { utils: Cu } = Components;

const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

const PREFS = {
  "pluginFlashBlockingCheckbox": "plugins.flashBlock.enabled",
  "pluginEnableProtectedModeCheckbox": "dom.ipc.plugins.flash.disable-protected-mode",
};

function init() {
  for (let id of Object.keys(PREFS)) {
    let checkbox = document.getElementById(id);
    checkbox.checked = Services.prefs.getBoolPref(PREFS[id]);
    checkbox.addEventListener("command", () => {
      Services.prefs.setBoolPref(PREFS[id], checkbox.checked);
    });
  }
}

window.addEventListener("load", init, { once: true });
