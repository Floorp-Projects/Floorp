/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals Services */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "UserAgentOverrides", "resource://gre/modules/UserAgentOverrides.jsm");
Cu.importGlobalProperties(["URL"]);

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

const EnabledPref = "extensions.gws-and-facebook-chrome-spoof.enabled";

let Enabled = true;

function checkIfEnabled() {
  Enabled = Services.prefs.getBoolPref(EnabledPref, true);
}

this.install = () => {
  Services.prefs.setBoolPref(EnabledPref, true);
};

this.uninstall = () => {
  Services.prefs.clearUserPref(EnabledPref);
};

this.shutdown = () => {
  Services.prefs.removeObserver(EnabledPref, checkIfEnabled);
  Enabled = false;
};

this.startup = () => {
  UserAgentOverrides.addComplexOverride((channel, defaultUA) => {
    if (!Enabled) {
      return false;
    }

    try {
      let domain = new URL(channel.URI.spec).host;
      if (domain.match(TLDsToSpoof)) {
        console.info(OverrideNotice);
        return TargetUA;
      }
    } catch (_) {
    }
    return false;
  });

  Services.prefs.addObserver(EnabledPref, checkIfEnabled);
  checkIfEnabled();
};
