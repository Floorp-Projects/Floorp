/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Helper functions and APIs for update tests.
 * Generally, most update tests will try to install an update and run
 * tests both before and after the update occurs. To support this, update
 * tests support three different lifecycle functions: "preUpdate",
 * "applyUpdate", and "postUpdate".
 *
 * Here's an example:
 *
 * // This test will apply an update, and run tests before and after is applied
 * function preUpdate() {
 *   myPreUpdate();
 * }
 * // use the default applyUpdate implementation
 * function postUpdate() {
 *   myPostUpdate();
 * }
 *
 * When B2GUpdateTestCase.execute_update_test is called from python, this is the
 * general flow:
 *
 * 1. The preUpdate function is called to run any initial tests and optionally
 *    get an update ready to apply. If the "apply" argument is None, then steps
 *    2 and 3 are skipped.
 * 2. The applyUpdate function is called to apply the update. There is a default
 *    implementation provided that can be overridden.
 * 3. The postUpdate function is called to run any tests after the update has
 *    been applied.
 * See b2g_update_test.py for more information about using 'apply'
 */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const APPLY_TIMEOUT = 10;

let browser = Services.wm.getMostRecentWindow("navigator:browser");
if (!browser) {
  log("Warning: No content browser");
}

let shell = browser.shell;
function getContentWindow() {
  return shell.contentBrowser.contentWindow;
}

function sendContentEvent(type, detail) {
  detail = detail || {};
  detail.type = type;

  let content = getContentWindow();
  shell.sendEvent(content, "mozContentEvent",
                   ObjectWrapper.wrap(detail, content));
  return true;
}

function addChromeEventListener(type, listener) {
  let content = getContentWindow();
  content.addEventListener("mozChromeEvent", function chromeListener(evt) {
    if (!evt.detail || evt.detail.type !== type) {
      return;
    }

    let remove = listener(evt);
    if (remove) {
      content.removeEventListener(chromeListener);
    }
  });
}

function createUpdatePrompt() {
  return Cc["@mozilla.org/updates/update-prompt;1"].
         createInstance(Ci.nsIUpdatePrompt);
}

let oldPrefs = {};

function getPrefByType(pref, prefType) {
  // The value may not exist, so swallow errors here
  try {
    switch (prefType) {
      case "string":
        return Services.prefs.getCharPref(pref);
      case "number":
        return Services.prefs.getIntPref(pref);
      case "boolean":
        return Services.prefs.getBoolPref(pref);
    }
  } catch (e) {}
  return undefined;
}

function setPref(pref, value) {
  switch (typeof(value)) {
    case "string":
      Services.prefs.setCharPref(pref, value);
      break;
    case "number":
      Services.prefs.setIntPref(pref, value);
      break;
    case "boolean":
      Services.prefs.setBoolPref(pref, value);
      break;
  }
}

function getPrefTypeDefaultValue(prefType) {
  switch (prefType) {
    case "string":
      return null;
    case "number":
      return 0;
    case "boolean":
      return false;
  }
  return undefined;
}

function setPrefs() {
  if (!updateArgs) {
    return;
  }

  let prefs = updateArgs.prefs;
  if (!prefs) {
    return;
  }

  let keys = Object.keys(prefs);
  for (let i = 0; i < keys.length; i++) {
    let key = keys[i];
    let value = prefs[key];
    let oldValue = getPrefByType(key, typeof(value));
    if (oldValue !== undefined) {
      oldPrefs[key] = oldValue;
    }

    setPref(key, value);
  }
}

function cleanPrefs() {
  if (!updateArgs) {
    return;
  }

  let prefs = updateArgs.prefs;
  if (!prefs) {
    return;
  }

  let keys = Object.keys(prefs);
  for (let i = 0; i < keys.length; i++) {
    let key = keys[i];
    let value = prefs[key];
    let oldValue = oldPrefs[key];
    if (oldValue === undefined) {
      oldValue = getPrefTypeDefaultValue(typeof(value));
      if (oldValue === undefined) {
        log("Warning: Couldn't unset pref " + key +
            ", unknown type for: " + value);
        continue;
      }
    }

    setPref(key, oldValue);
  }
}

function getStartBuild() {
  let start = updateArgs.start;
  ok(start, "Start build not found in updateArgs");
  return start;
}

function getFinishBuild() {
  let finish = updateArgs.finish;
  ok(finish, "Finish build not found in updateArgs");
  return finish;
}

function isFinishUpdate(update) {
  let finish = getFinishBuild();

  is(update.appVersion, finish.app_version,
     "update app version should be finish app version: " + finish.app_version);
  is(update.buildID, finish.app_build_id,
     "update build ID should be finish app build ID: " + finish.app_build_id);
}

function isStartToFinishUpdate(update) {
  let start = getStartBuild();

  is(update.previousAppVersion, start.app_version,
     "update previous app version should be start app version: " +
     start.app_version);
  isFinishUpdate(update);
}

function statusSettingIs(value, next) {
  Services.settings.createLock().get("gecko.updateStatus", {
    handle: function(name, v) {
      is(v, value);
      next();
    },
    handleError: function(error) {
      fail(error);
    }
  });
}

function applyUpdate() {
  sendContentEvent("update-prompt-apply-result", {
    result: "restart"
  });
}

function runUpdateTest(stage) {
  switch (stage) {
    case "pre-update":
      if (preUpdate) {
        preUpdate();
      }
      break;
    case "apply-update":
      if (applyUpdate) {
        applyUpdate();
      }
      break;
    case "post-update":
      if (postUpdate) {
        postUpdate();
      }
      break;
  }
}

let updateArgs;
if (__marionetteParams) {
  updateArgs = __marionetteParams[0];
}

function cleanUp() {
  cleanPrefs();
  finish();
  //marionetteScriptFinished();
}

setPrefs();
