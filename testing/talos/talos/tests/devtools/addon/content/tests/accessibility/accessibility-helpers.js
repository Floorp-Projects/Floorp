/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { openToolboxAndLog, reloadPageAndLog } = require("damp-test/tests/head");

const INITIALIZED_EVENT = "Accessibility:Initialized";

const PREF_ACCESSIBILITY_FORCE_DISABLED = "accessibility.force_disabled";

exports.shutdownAccessibilityService = function() {
  // Set PREF_ACCESSIBILITY_FORCE_DISABLED to 1 to force disable
  // accessibility service. This is the only way to guarantee an immediate
  // accessibility service shutdown in all processes. This also prevents
  // accessibility service from starting up in the future.
  Services.prefs.setIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED, 1);
  // Set PREF_ACCESSIBILITY_FORCE_DISABLED back to default value. This will
  // not start accessibility service until the user activates it again. It
  // simply ensures that accessibility service can start again (when value is
  // below 1).
  Services.prefs.clearUserPref(PREF_ACCESSIBILITY_FORCE_DISABLED);
};

exports.openAccessibilityAndLog = function(label) {
  return openToolboxAndLog(`${label}.accessibility`, "accessibility");
};

exports.reloadAccessibilityAndLog = async function(label, toolbox) {
  const onReload = async function() {
    let accessibility = await toolbox.getPanelWhenReady("accessibility");
    await accessibility.panelWin.once(INITIALIZED_EVENT);
  };

  await reloadPageAndLog(`${label}.accessibility`, toolbox, onReload);
};
