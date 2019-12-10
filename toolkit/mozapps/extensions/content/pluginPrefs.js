/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

const PREFS = {
  pluginFlashBlockingCheckbox: {
    pref: "plugins.flashBlock.enabled",
    invert: false,
  },
  pluginEnableProtectedModeCheckbox: {
    pref: "dom.ipc.plugins.flash.disable-protected-mode",
    invert: true,
  },
};

async function renderPluginMetadata(id) {
  let plugin = await AddonManager.getAddonByID(id);
  if (!plugin) {
    return;
  }

  let libLabel = document.getElementById("pluginLibraries");
  libLabel.textContent = plugin.pluginLibraries.join(", ");

  let typeLabel = document.getElementById("pluginMimeTypes"),
    types = [];
  for (let type of plugin.pluginMimeTypes) {
    let extras = [type.description.trim(), type.suffixes]
      .filter(x => x)
      .join(": ");
    types.push(type.type + (extras ? " (" + extras + ")" : ""));
  }
  typeLabel.textContent = types.join(",\n");
  let showProtectedModePref = canDisableFlashProtectedMode(plugin);
  document.getElementById(
    "pluginEnableProtectedMode"
  ).hidden = !showProtectedModePref;

  // Disable flash blocking when Fission is enabled (See Bug 1584931).
  document.getElementById(
    "pluginFlashBlocking"
  ).hidden = canDisableFlashBlocking();
}

// Protected mode is win32-only, not win64
function canDisableFlashProtectedMode(aPlugin) {
  return aPlugin.isFlashPlugin && Services.appinfo.XPCOMABI == "x86-msvc";
}

function canDisableFlashBlocking() {
  return Services.prefs.getBoolPref("fission.autostart");
}

function init() {
  let params = new URLSearchParams(location.hash.slice(1));
  renderPluginMetadata(params.get("id"));

  for (let id of Object.keys(PREFS)) {
    let checkbox = document.getElementById(id);
    var prefVal = Services.prefs.getBoolPref(PREFS[id].pref);
    checkbox.checked = PREFS[id].invert ? !prefVal : prefVal;
    checkbox.addEventListener("change", () => {
      Services.prefs.setBoolPref(
        PREFS[id].pref,
        PREFS[id].invert ? !checkbox.checked : checkbox.checked
      );
    });
  }
}

window.addEventListener("load", init, { once: true });
