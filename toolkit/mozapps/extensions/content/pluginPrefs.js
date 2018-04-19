/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});

const PREFS = {
  "pluginFlashBlockingCheckbox":
    { pref: "plugins.flashBlock.enabled", invert: false },
  "pluginEnableProtectedModeCheckbox":
    { pref: "dom.ipc.plugins.flash.disable-protected-mode", invert: true },
};

function init() {
  for (let id of Object.keys(PREFS)) {
    let checkbox = document.getElementById(id);
    var prefVal = Services.prefs.getBoolPref(PREFS[id].pref);
    checkbox.checked = PREFS[id].invert ? !prefVal : prefVal;
    checkbox.addEventListener("command", () => {
      Services.prefs.setBoolPref(PREFS[id].pref,
                                 PREFS[id].invert ? !checkbox.checked : checkbox.checked);
    });
  }
}

window.addEventListener("load", init, { once: true });
