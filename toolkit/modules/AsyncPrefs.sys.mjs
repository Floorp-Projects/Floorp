/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kInChildProcess =
  Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;

const kAllowedPrefs = new Set([
  // NB: please leave the testing prefs at the top, and sort the rest alphabetically if you add
  // anything.
  "testing.allowed-prefs.some-bool-pref",
  "testing.allowed-prefs.some-char-pref",
  "testing.allowed-prefs.some-int-pref",

  "browser.contentblocking.report.hide_vpn_banner",
  "browser.contentblocking.report.show_mobile_app",

  "browser.shopping.experience2023.optedIn",
  "browser.shopping.experience2023.active",
  "browser.shopping.experience2023.ads.userEnabled",

  "narrate.rate",
  "narrate.voice",

  "reader.font_size",
  "reader.font_type",
  "reader.color_scheme",
  "reader.content_width",
  "reader.line_height",

  "security.tls.version.enable-deprecated",
  "security.xfocsp.errorReporting.automatic",

  "network.trr.display_fallback_warning",
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
  if (
    prefType != Services.prefs.PREF_INVALID &&
    prefType != kPrefTypeMap.get(valueType)
  ) {
    return `Can't set pref ${pref} to a value with type ${valueType} that doesn't match the pref's type ${prefType}.`;
  }
  return false;
}

export class AsyncPrefsChild extends JSProcessActorChild {
  set(pref, value) {
    let error = maybeReturnErrorForSet(pref, value);
    if (error) {
      return Promise.reject(error);
    }

    return this.sendQuery("AsyncPrefs:SetPref", {
      pref,
      value,
    });
  }

  reset(pref) {
    let error = maybeReturnErrorForReset(pref);
    if (error) {
      return Promise.reject(error);
    }

    return this.sendQuery("AsyncPrefs:ResetPref", { pref });
  }
}

export var AsyncPrefs = {
  set(pref, value) {
    if (kInChildProcess) {
      return ChromeUtils.domProcessChild
        .getActor("AsyncPrefs")
        .set(pref, value);
    }
    return AsyncPrefsParent.set(pref, value);
  },

  reset(pref, value) {
    if (kInChildProcess) {
      return ChromeUtils.domProcessChild.getActor("AsyncPrefs").reset(pref);
    }
    return AsyncPrefsParent.reset(pref);
  },
};

const methodForType = {
  number: "setIntPref",
  boolean: "setBoolPref",
  string: "setCharPref",
};

export class AsyncPrefsParent extends JSProcessActorParent {
  static set(pref, value) {
    let error = maybeReturnErrorForSet(pref, value);
    if (error) {
      return Promise.reject(error);
    }
    let methodToUse = methodForType[typeof value];
    try {
      Services.prefs[methodToUse](pref, value);
    } catch (ex) {
      console.error(ex);
      return Promise.reject(ex.message);
    }

    return Promise.resolve(value);
  }

  static reset(pref) {
    let error = maybeReturnErrorForReset(pref);
    if (error) {
      return Promise.reject(error);
    }

    try {
      Services.prefs.clearUserPref(pref);
    } catch (ex) {
      console.error(ex);
      return Promise.reject(ex.message);
    }

    return Promise.resolve();
  }

  receiveMessage(msg) {
    if (msg.name == "AsyncPrefs:SetPref") {
      return AsyncPrefsParent.set(msg.data.pref, msg.data.value);
    }
    return AsyncPrefsParent.reset(msg.data.pref);
  }
}
