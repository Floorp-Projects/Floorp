/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals Services */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

const TLDsToSpoof = /(^(www|encrypted|maps)\.google\.)|((.*\.facebook|.*\.fbcdn|.*\.fbsbx)\.(com|net)$)/;

const OverrideNotice = "The user agent string has been overridden to get the Chrome experience on this site.";

const defaultUA = Cc["@mozilla.org/network/protocol;1?name=http"].
                    getService(Ci.nsIHttpProtocolHandler).userAgent;

const RunningFirefoxVersion = (defaultUA.match(/Firefox\/([0-9.]+)/) || ["", "58.0"])[1];
const RunningAndroidVersion = defaultUA.match(/Android\/[0-9.]+/) || "Android 6.0";

const ChromeMajorVersionToMimic = `${parseInt(RunningFirefoxVersion) + 4}.0.0.0`;

const ChromePhoneUA = `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; Nexus 5 Build/MRA58N) FxQuantum/${RunningFirefoxVersion} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${ChromeMajorVersionToMimic} Mobile Safari/537.36`;
const ChromeTabletUA = `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; Nexus 7 Build/JSS15Q) FxQuantum/${RunningFirefoxVersion} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${ChromeMajorVersionToMimic} Safari/537.36`;

const IsPhone = defaultUA.includes("Mobile");

const TargetUA = IsPhone ? ChromePhoneUA : ChromeTabletUA;

const EnabledPrefBranch = "extensions.gws-and-facebook-chrome-spoof.";
const EnabledPref = `${EnabledPrefBranch}enabled`;

const Observer = {
  observe: function HTTP_on_modify_request(aSubject, aTopic, aData) {
    let channel = aSubject.QueryInterface(Ci.nsIHttpChannel);
    if (channel && channel.URI.asciiHost.match(TLDsToSpoof)) {
      channel.setRequestHeader("User-Agent", TargetUA, false);
      console.info(OverrideNotice);
    }
  }
};

function enable() {
  try {
    Services.obs.addObserver(Observer, "http-on-modify-request");
  } catch (_) {
  }
}

function disable() {
  try {
    Services.obs.removeObserver(Observer, "http-on-modify-request");
  } catch (_) {
  }
}

function checkIfEnabled() {
  try {
    if (Services.prefs.getBoolPref(EnabledPref)) {
      enable();
    } else {
      disable();
    }
  } catch (_) {
    // If the pref does not exist yet, we specify a default value of true.
    // (the "reset" option in about:config removes prefs without defaults,
    // and our add-on will not be informed, forcing the user to re-create
    // the pref manually to disable the add-on).
    Services.prefs.getDefaultBranch(EnabledPrefBranch).setBoolPref("enabled", true);
    enable();
  }
}

this.install = () => {
  Services.prefs.setBoolPref(EnabledPref, true);
};

this.uninstall = () => {
  Services.prefs.getDefaultBranch(null).deleteBranch(EnabledPrefBranch);
};

this.shutdown = () => {
  Services.prefs.removeObserver(EnabledPref, checkIfEnabled);
  disable();
};

this.startup = () => {
  Services.prefs.addObserver(EnabledPref, checkIfEnabled);
  checkIfEnabled();
};
