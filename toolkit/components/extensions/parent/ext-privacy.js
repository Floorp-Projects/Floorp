/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionPreferencesManager } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPreferencesManager.sys.mjs"
);

var { ExtensionError } = ExtensionUtils;
var { getSettingsAPI, getPrimedSettingsListener } = ExtensionPreferencesManager;

const cookieSvc = Ci.nsICookieService;

const getIntPref = p => Services.prefs.getIntPref(p, undefined);
const getBoolPref = p => Services.prefs.getBoolPref(p, undefined);

const TLS_MIN_PREF = "security.tls.version.min";
const TLS_MAX_PREF = "security.tls.version.max";

const cookieBehaviorValues = new Map([
  ["allow_all", cookieSvc.BEHAVIOR_ACCEPT],
  ["reject_third_party", cookieSvc.BEHAVIOR_REJECT_FOREIGN],
  ["reject_all", cookieSvc.BEHAVIOR_REJECT],
  ["allow_visited", cookieSvc.BEHAVIOR_LIMIT_FOREIGN],
  ["reject_trackers", cookieSvc.BEHAVIOR_REJECT_TRACKER],
  [
    "reject_trackers_and_partition_foreign",
    cookieSvc.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
  ],
]);

function isTLSMinVersionLowerOrEQThan(version) {
  return (
    Services.prefs.getDefaultBranch("").getIntPref(TLS_MIN_PREF) <= version
  );
}

const TLS_VERSIONS = [
  { version: 1, name: "TLSv1", settable: isTLSMinVersionLowerOrEQThan(1) },
  { version: 2, name: "TLSv1.1", settable: isTLSMinVersionLowerOrEQThan(2) },
  { version: 3, name: "TLSv1.2", settable: true },
  { version: 4, name: "TLSv1.3", settable: true },
];

// Add settings objects for supported APIs to the preferences manager.
ExtensionPreferencesManager.addSetting("network.networkPredictionEnabled", {
  permission: "privacy",
  prefNames: [
    "network.predictor.enabled",
    "network.prefetch-next",
    "network.http.speculative-parallel-limit",
    "network.dns.disablePrefetch",
  ],

  setCallback(value) {
    return {
      "network.http.speculative-parallel-limit": value ? undefined : 0,
      "network.dns.disablePrefetch": !value,
      "network.predictor.enabled": value,
      "network.prefetch-next": value,
    };
  },

  getCallback() {
    return (
      getBoolPref("network.predictor.enabled") &&
      getBoolPref("network.prefetch-next") &&
      getIntPref("network.http.speculative-parallel-limit") > 0 &&
      !getBoolPref("network.dns.disablePrefetch")
    );
  },
});

ExtensionPreferencesManager.addSetting("network.globalPrivacyControl", {
  permission: "privacy",
  prefNames: ["privacy.globalprivacycontrol.enabled"],
  readOnly: true,

  setCallback(value) {
    return {
      "privacy.globalprivacycontrol.enabled": value,
    };
  },

  getCallback() {
    return getBoolPref("privacy.globalprivacycontrol.enabled");
  },
});

ExtensionPreferencesManager.addSetting("network.httpsOnlyMode", {
  permission: "privacy",
  prefNames: [
    "dom.security.https_only_mode",
    "dom.security.https_only_mode_pbm",
  ],
  readOnly: true,

  setCallback(value) {
    let prefs = {
      "dom.security.https_only_mode": false,
      "dom.security.https_only_mode_pbm": false,
    };

    switch (value) {
      case "always":
        prefs["dom.security.https_only_mode"] = true;
        break;

      case "private_browsing":
        prefs["dom.security.https_only_mode_pbm"] = true;
        break;

      case "never":
        break;
    }

    return prefs;
  },

  getCallback() {
    if (getBoolPref("dom.security.https_only_mode")) {
      return "always";
    }
    if (getBoolPref("dom.security.https_only_mode_pbm")) {
      return "private_browsing";
    }
    return "never";
  },
});

ExtensionPreferencesManager.addSetting("network.peerConnectionEnabled", {
  permission: "privacy",
  prefNames: ["media.peerconnection.enabled"],

  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    return getBoolPref("media.peerconnection.enabled");
  },
});

ExtensionPreferencesManager.addSetting("network.webRTCIPHandlingPolicy", {
  permission: "privacy",
  prefNames: [
    "media.peerconnection.ice.default_address_only",
    "media.peerconnection.ice.no_host",
    "media.peerconnection.ice.proxy_only_if_behind_proxy",
    "media.peerconnection.ice.proxy_only",
  ],

  setCallback(value) {
    let prefs = {};
    switch (value) {
      case "default":
        // All prefs are already set to be reset.
        break;

      case "default_public_and_private_interfaces":
        prefs["media.peerconnection.ice.default_address_only"] = true;
        break;

      case "default_public_interface_only":
        prefs["media.peerconnection.ice.default_address_only"] = true;
        prefs["media.peerconnection.ice.no_host"] = true;
        break;

      case "disable_non_proxied_udp":
        prefs["media.peerconnection.ice.default_address_only"] = true;
        prefs["media.peerconnection.ice.no_host"] = true;
        prefs["media.peerconnection.ice.proxy_only_if_behind_proxy"] = true;
        break;

      case "proxy_only":
        prefs["media.peerconnection.ice.proxy_only"] = true;
        break;
    }
    return prefs;
  },

  getCallback() {
    if (getBoolPref("media.peerconnection.ice.proxy_only")) {
      return "proxy_only";
    }

    let default_address_only = getBoolPref(
      "media.peerconnection.ice.default_address_only"
    );
    if (default_address_only) {
      let no_host = getBoolPref("media.peerconnection.ice.no_host");
      if (no_host) {
        if (
          getBoolPref("media.peerconnection.ice.proxy_only_if_behind_proxy")
        ) {
          return "disable_non_proxied_udp";
        }
        return "default_public_interface_only";
      }
      return "default_public_and_private_interfaces";
    }

    return "default";
  },
});

ExtensionPreferencesManager.addSetting("services.passwordSavingEnabled", {
  permission: "privacy",
  prefNames: ["signon.rememberSignons"],

  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    return getBoolPref("signon.rememberSignons");
  },
});

ExtensionPreferencesManager.addSetting("websites.cookieConfig", {
  permission: "privacy",
  prefNames: ["network.cookie.cookieBehavior"],

  setCallback(value) {
    const cookieBehavior = cookieBehaviorValues.get(value.behavior);

    // Intentionally use Preferences.get("network.cookie.cookieBehavior") here
    // to read the "real" preference value.
    const needUpdate =
      cookieBehavior !== getIntPref("network.cookie.cookieBehavior");
    if (
      needUpdate &&
      getBoolPref("privacy.firstparty.isolate") &&
      cookieBehavior === cookieSvc.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
    ) {
      throw new ExtensionError(
        `Invalid cookieConfig '${value.behavior}' when firstPartyIsolate is enabled`
      );
    }

    if (typeof value.nonPersistentCookies === "boolean") {
      Cu.reportError(
        "'nonPersistentCookies' has been deprecated and it has no effect anymore."
      );
    }

    return {
      "network.cookie.cookieBehavior": cookieBehavior,
    };
  },

  getCallback() {
    let prefValue = getIntPref("network.cookie.cookieBehavior");
    return {
      behavior: Array.from(cookieBehaviorValues.entries()).find(
        entry => entry[1] === prefValue
      )[0],
      // Bug 1754924 - this property is now deprecated.
      nonPersistentCookies: false,
    };
  },
});

ExtensionPreferencesManager.addSetting("websites.firstPartyIsolate", {
  permission: "privacy",
  prefNames: ["privacy.firstparty.isolate"],

  setCallback(value) {
    // Intentionally use Preferences.get("network.cookie.cookieBehavior") here
    // to read the "real" preference value.
    const cookieBehavior = getIntPref("network.cookie.cookieBehavior");

    const needUpdate = value !== getBoolPref("privacy.firstparty.isolate");
    if (
      needUpdate &&
      value &&
      cookieBehavior === cookieSvc.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
    ) {
      const behavior = Array.from(cookieBehaviorValues.entries()).find(
        entry => entry[1] === cookieBehavior
      )[0];
      throw new ExtensionError(
        `Can't enable firstPartyIsolate when cookieBehavior is '${behavior}'`
      );
    }

    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    return getBoolPref("privacy.firstparty.isolate");
  },
});

ExtensionPreferencesManager.addSetting("websites.hyperlinkAuditingEnabled", {
  permission: "privacy",
  prefNames: ["browser.send_pings"],

  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    return getBoolPref("browser.send_pings");
  },
});

ExtensionPreferencesManager.addSetting("websites.referrersEnabled", {
  permission: "privacy",
  prefNames: ["network.http.sendRefererHeader"],

  // Values for network.http.sendRefererHeader:
  // 0=don't send any, 1=send only on clicks, 2=send on image requests as well
  // http://searchfox.org/mozilla-central/rev/61054508641ee76f9c49bcf7303ef3cfb6b410d2/modules/libpref/init/all.js#1585
  setCallback(value) {
    return { [this.prefNames[0]]: value ? 2 : 0 };
  },

  getCallback() {
    return getIntPref("network.http.sendRefererHeader") !== 0;
  },
});

ExtensionPreferencesManager.addSetting("websites.resistFingerprinting", {
  permission: "privacy",
  prefNames: ["privacy.resistFingerprinting"],

  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    return getBoolPref("privacy.resistFingerprinting");
  },
});

ExtensionPreferencesManager.addSetting("websites.trackingProtectionMode", {
  permission: "privacy",
  prefNames: [
    "privacy.trackingprotection.enabled",
    "privacy.trackingprotection.pbmode.enabled",
  ],

  setCallback(value) {
    // Default to private browsing.
    let prefs = {
      "privacy.trackingprotection.enabled": false,
      "privacy.trackingprotection.pbmode.enabled": true,
    };

    switch (value) {
      case "private_browsing":
        break;

      case "always":
        prefs["privacy.trackingprotection.enabled"] = true;
        break;

      case "never":
        prefs["privacy.trackingprotection.pbmode.enabled"] = false;
        break;
    }

    return prefs;
  },

  getCallback() {
    if (getBoolPref("privacy.trackingprotection.enabled")) {
      return "always";
    } else if (getBoolPref("privacy.trackingprotection.pbmode.enabled")) {
      return "private_browsing";
    }
    return "never";
  },
});

ExtensionPreferencesManager.addSetting("network.tlsVersionRestriction", {
  permission: "privacy",
  prefNames: [TLS_MIN_PREF, TLS_MAX_PREF],

  setCallback(value) {
    function tlsStringToVersion(string) {
      const version = TLS_VERSIONS.find(a => a.name === string);
      if (version && version.settable) {
        return version.version;
      }

      throw new ExtensionError(
        `Setting TLS version ${string} is not allowed for security reasons.`
      );
    }

    const prefs = {};

    if (value.minimum) {
      prefs[TLS_MIN_PREF] = tlsStringToVersion(value.minimum);
    }

    if (value.maximum) {
      prefs[TLS_MAX_PREF] = tlsStringToVersion(value.maximum);
    }

    // If minimum has passed and it's greater than the max value.
    if (prefs[TLS_MIN_PREF]) {
      const max = prefs[TLS_MAX_PREF] || getIntPref(TLS_MAX_PREF);
      if (max < prefs[TLS_MIN_PREF]) {
        throw new ExtensionError(
          `Setting TLS min version grater than the max version is not allowed.`
        );
      }
    }

    // If maximum has passed and it's lower than the min value.
    else if (prefs[TLS_MAX_PREF]) {
      const min = getIntPref(TLS_MIN_PREF);
      if (min > prefs[TLS_MAX_PREF]) {
        throw new ExtensionError(
          `Setting TLS max version lower than the min version is not allowed.`
        );
      }
    }

    return prefs;
  },

  getCallback() {
    function tlsVersionToString(pref) {
      const value = getIntPref(pref);
      const version = TLS_VERSIONS.find(a => a.version === value);
      if (version) {
        return version.name;
      }
      return "unknown";
    }

    return {
      minimum: tlsVersionToString(TLS_MIN_PREF),
      maximum: tlsVersionToString(TLS_MAX_PREF),
    };
  },

  validate(extension) {
    if (!extension.isPrivileged) {
      throw new ExtensionError(
        "tlsVersionRestriction can be set by privileged extensions only."
      );
    }
  },
});

this.privacy = class extends ExtensionAPI {
  primeListener(event, fire) {
    let { extension } = this;
    let listener = getPrimedSettingsListener({
      extension,
      name: event,
    });
    return listener(fire);
  }

  getAPI(context) {
    function makeSettingsAPI(name) {
      return getSettingsAPI({
        context,
        module: "privacy",
        name,
      });
    }

    return {
      privacy: {
        network: {
          networkPredictionEnabled: makeSettingsAPI(
            "network.networkPredictionEnabled"
          ),
          globalPrivacyControl: makeSettingsAPI("network.globalPrivacyControl"),
          httpsOnlyMode: makeSettingsAPI("network.httpsOnlyMode"),
          peerConnectionEnabled: makeSettingsAPI(
            "network.peerConnectionEnabled"
          ),
          webRTCIPHandlingPolicy: makeSettingsAPI(
            "network.webRTCIPHandlingPolicy"
          ),
          tlsVersionRestriction: makeSettingsAPI(
            "network.tlsVersionRestriction"
          ),
        },

        services: {
          passwordSavingEnabled: makeSettingsAPI(
            "services.passwordSavingEnabled"
          ),
        },

        websites: {
          cookieConfig: makeSettingsAPI("websites.cookieConfig"),
          firstPartyIsolate: makeSettingsAPI("websites.firstPartyIsolate"),
          hyperlinkAuditingEnabled: makeSettingsAPI(
            "websites.hyperlinkAuditingEnabled"
          ),
          referrersEnabled: makeSettingsAPI("websites.referrersEnabled"),
          resistFingerprinting: makeSettingsAPI(
            "websites.resistFingerprinting"
          ),
          trackingProtectionMode: makeSettingsAPI(
            "websites.trackingProtectionMode"
          ),
        },
      },
    };
  }
};
