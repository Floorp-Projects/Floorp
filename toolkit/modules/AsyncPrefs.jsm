/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["AsyncPrefs"];

const {interfaces: Ci, utils: Cu, classes: Cc} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

const kInChildProcess = Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;

const kAllowedPrefs = new Set([
  // NB: please leave the testing prefs at the top, and sort the rest alphabetically if you add
  // anything.
  "testing.allowed-prefs.some-bool-pref",
  "testing.allowed-prefs.some-char-pref",
  "testing.allowed-prefs.some-int-pref",

  "narrate.rate",
  "narrate.voice",

  "reader.font_size",
  "reader.font_type",
  "reader.color_scheme",
  "reader.content_width",
  "reader.line_height",
]);

const kPrefTypeMap = new Map([
  ["boolean", Services.prefs.PREF_BOOL],
  ["number", Services.prefs.PREF_INT],
  ["string", Services.prefs.PREF_STRING],
]);

function maybeReturnErrorForReset(pref) {
  if (!kAllowedPrefs.has(pref)) {
    return `Resetting pref ${pref} from content is not allowed.`;
  }
  return false;
}

function maybeReturnErrorForSet(pref, value) {
  if (!kAllowedPrefs.has(pref)) {
    return `Setting pref ${pref} from content is not allowed.`;
  }

  let valueType = typeof value;
  if (!kPrefTypeMap.has(valueType)) {
    return `Can't set pref ${pref} to value of type ${valueType}.`;
  }
  let prefType = Services.prefs.getPrefType(pref);
  if (prefType != Services.prefs.PREF_INVALID &&
      prefType != kPrefTypeMap.get(valueType)) {
    return `Can't set pref ${pref} to a value with type ${valueType} that doesn't match the pref's type ${prefType}.`;
  }
  return false;
}

var AsyncPrefs;
if (kInChildProcess) {
  let gUniqueId = 0;
  let gMsgMap = new Map();

  AsyncPrefs = {
    set: Task.async(function(pref, value) {
      let error = maybeReturnErrorForSet(pref, value);
      if (error) {
        return Promise.reject(error);
      }

      let msgId = ++gUniqueId;
      return new Promise((resolve, reject) => {
        gMsgMap.set(msgId, {resolve, reject});
        Services.cpmm.sendAsyncMessage("AsyncPrefs:SetPref", {pref, value, msgId});
      });
    }),

    reset: Task.async(function(pref) {
      let error = maybeReturnErrorForReset(pref);
      if (error) {
        return Promise.reject(error);
      }

      let msgId = ++gUniqueId;
      return new Promise((resolve, reject) => {
        gMsgMap.set(msgId, {resolve, reject});
        Services.cpmm.sendAsyncMessage("AsyncPrefs:ResetPref", {pref, msgId});
      });
    }),

    receiveMessage(msg) {
      let promiseRef = gMsgMap.get(msg.data.msgId);
      if (promiseRef) {
        gMsgMap.delete(msg.data.msgId);
        if (msg.data.success) {
          promiseRef.resolve();
        } else {
          promiseRef.reject(msg.data.message);
        }
      }
    },

    init() {
      Services.cpmm.addMessageListener("AsyncPrefs:PrefSetFinished", this);
      Services.cpmm.addMessageListener("AsyncPrefs:PrefResetFinished", this);
    },
  };
} else {
  AsyncPrefs = {
    methodForType: {
      number: "setIntPref",
      boolean: "setBoolPref",
      string: "setCharPref",
    },

    set: Task.async(function(pref, value) {
      let error = maybeReturnErrorForSet(pref, value);
      if (error) {
        return Promise.reject(error);
      }
      let methodToUse = this.methodForType[typeof value];
      try {
        Services.prefs[methodToUse](pref, value);
        return Promise.resolve(value);
      } catch (ex) {
        Cu.reportError(ex);
        return Promise.reject(ex.message);
      }
    }),

    reset: Task.async(function(pref) {
      let error = maybeReturnErrorForReset(pref);
      if (error) {
        return Promise.reject(error);
      }

      try {
        Services.prefs.clearUserPref(pref);
        return Promise.resolve();
      } catch (ex) {
        Cu.reportError(ex);
        return Promise.reject(ex.message);
      }
    }),

    receiveMessage(msg) {
      if (msg.name == "AsyncPrefs:SetPref") {
        this.onPrefSet(msg);
      } else {
        this.onPrefReset(msg);
      }
    },

    onPrefReset(msg) {
      let {pref, msgId} = msg.data;
      this.reset(pref).then(function() {
        msg.target.sendAsyncMessage("AsyncPrefs:PrefResetFinished", {msgId, success: true});
      }, function(msg) {
        msg.target.sendAsyncMessage("AsyncPrefs:PrefResetFinished", {msgId, success: false, message: msg});
      });
    },

    onPrefSet(msg) {
      let {pref, value, msgId} = msg.data;
      this.set(pref, value).then(function() {
        msg.target.sendAsyncMessage("AsyncPrefs:PrefSetFinished", {msgId, success: true});
      }, function(msg) {
        msg.target.sendAsyncMessage("AsyncPrefs:PrefSetFinished", {msgId, success: false, message: msg});
      });
    },

    init() {
      Services.ppmm.addMessageListener("AsyncPrefs:SetPref", this);
      Services.ppmm.addMessageListener("AsyncPrefs:ResetPref", this);
    }
  };
}

AsyncPrefs.init();

