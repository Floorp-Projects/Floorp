/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

Cu.import("resource://gre/modules/ExtensionPreferencesManager.jsm");

var {
  ExtensionError,
} = ExtensionUtils;

const cookieSvc = Ci.nsICookieService;

const cookieBehaviorValues = new Map([
  ["allow_all", cookieSvc.BEHAVIOR_ACCEPT],
  ["reject_third_party", cookieSvc.BEHAVIOR_REJECT_FOREIGN],
  ["reject_all", cookieSvc.BEHAVIOR_REJECT],
  ["allow_visited", cookieSvc.BEHAVIOR_LIMIT_FOREIGN],
]);

const checkScope = scope => {
  if (scope && scope !== "regular") {
    throw new ExtensionError(
      `Firefox does not support the ${scope} settings scope.`);
  }
};

const getPrivacyAPI = (extension, name, callback) => {
  return {
    async get(details) {
      return {
        levelOfControl: details.incognito ?
          "not_controllable" :
          await ExtensionPreferencesManager.getLevelOfControl(
            extension.id, name),
        value: await callback(),
      };
    },
    set(details) {
      checkScope(details.scope);
      return ExtensionPreferencesManager.setSetting(
        extension.id, name, details.value);
    },
    clear(details) {
      checkScope(details.scope);
      return ExtensionPreferencesManager.removeSetting(
        extension.id, name);
    },
  };
};

// Add settings objects for supported APIs to the preferences manager.
ExtensionPreferencesManager.addSetting("network.networkPredictionEnabled", {
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
});

ExtensionPreferencesManager.addSetting("network.peerConnectionEnabled", {
  prefNames: [
    "media.peerconnection.enabled",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

ExtensionPreferencesManager.addSetting("network.webRTCIPHandlingPolicy", {
  prefNames: [
    "media.peerconnection.ice.default_address_only",
    "media.peerconnection.ice.no_host",
    "media.peerconnection.ice.proxy_only",
  ],

  setCallback(value) {
    let prefs = {};
    // Start with all prefs being reset.
    for (let pref of this.prefNames) {
      prefs[pref] = undefined;
    }
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
        prefs["media.peerconnection.ice.proxy_only"] = true;
        break;
    }
    return prefs;
  },
});

ExtensionPreferencesManager.addSetting("services.passwordSavingEnabled", {
  prefNames: [
    "signon.rememberSignons",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

ExtensionPreferencesManager.addSetting("websites.cookieConfig", {
  prefNames: [
    "network.cookie.cookieBehavior",
    "network.cookie.lifetimePolicy",
  ],

  setCallback(value) {
    return {
      "network.cookie.cookieBehavior":
        cookieBehaviorValues.get(value.behavior),
      "network.cookie.lifetimePolicy":
        value.nonPersistentCookies ?
          cookieSvc.ACCEPT_SESSION :
          cookieSvc.ACCEPT_NORMALLY,
    };
  },
});

ExtensionPreferencesManager.addSetting("websites.firstPartyIsolate", {
  prefNames: [
    "privacy.firstparty.isolate",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

ExtensionPreferencesManager.addSetting("websites.hyperlinkAuditingEnabled", {
  prefNames: [
    "browser.send_pings",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

ExtensionPreferencesManager.addSetting("websites.referrersEnabled", {
  prefNames: [
    "network.http.sendRefererHeader",
  ],

  // Values for network.http.sendRefererHeader:
  // 0=don't send any, 1=send only on clicks, 2=send on image requests as well
  // http://searchfox.org/mozilla-central/rev/61054508641ee76f9c49bcf7303ef3cfb6b410d2/modules/libpref/init/all.js#1585
  setCallback(value) {
    return {[this.prefNames[0]]: value ? 2 : 0};
  },
});

ExtensionPreferencesManager.addSetting("websites.resistFingerprinting", {
  prefNames: [
    "privacy.resistFingerprinting",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

ExtensionPreferencesManager.addSetting("websites.trackingProtectionMode", {
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
});

this.privacy = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      privacy: {
        network: {
          networkPredictionEnabled: getPrivacyAPI(
            extension, "network.networkPredictionEnabled",
            () => {
              return Preferences.get("network.predictor.enabled") &&
                Preferences.get("network.prefetch-next") &&
                Preferences.get("network.http.speculative-parallel-limit") > 0 &&
                !Preferences.get("network.dns.disablePrefetch");
            }),
          peerConnectionEnabled: getPrivacyAPI(
            extension, "network.peerConnectionEnabled",
            () => {
              return Preferences.get("media.peerconnection.enabled");
            }),
          webRTCIPHandlingPolicy: getPrivacyAPI(
            extension, "network.webRTCIPHandlingPolicy",
            () => {
              if (Preferences.get("media.peerconnection.ice.proxy_only")) {
                return "disable_non_proxied_udp";
              }

              let default_address_only =
                Preferences.get("media.peerconnection.ice.default_address_only");
              if (default_address_only) {
                if (Preferences.get("media.peerconnection.ice.no_host")) {
                  return "default_public_interface_only";
                }
                return "default_public_and_private_interfaces";
              }

              return "default";
            }),
        },

        services: {
          passwordSavingEnabled: getPrivacyAPI(
            extension, "services.passwordSavingEnabled",
            () => {
              return Preferences.get("signon.rememberSignons");
            }),
        },

        websites: {
          cookieConfig: getPrivacyAPI(
            extension, "websites.cookieConfig",
            () => {
              let prefValue = Preferences.get("network.cookie.cookieBehavior");
              return {
                behavior:
                  Array.from(
                    cookieBehaviorValues.entries()).find(entry => entry[1] === prefValue)[0],
                nonPersistentCookies:
                  Preferences.get("network.cookie.lifetimePolicy") === cookieSvc.ACCEPT_SESSION,
              };
            }),
          firstPartyIsolate: getPrivacyAPI(
            extension, "websites.firstPartyIsolate",
            () => {
              return Preferences.get("privacy.firstparty.isolate");
            }),
          hyperlinkAuditingEnabled: getPrivacyAPI(
            extension, "websites.hyperlinkAuditingEnabled",
            () => {
              return Preferences.get("browser.send_pings");
            }),
          referrersEnabled: getPrivacyAPI(
            extension, "websites.referrersEnabled",
            () => {
              return Preferences.get("network.http.sendRefererHeader") !== 0;
            }),
          resistFingerprinting: getPrivacyAPI(
            extension, "websites.resistFingerprinting",
            () => {
              return Preferences.get("privacy.resistFingerprinting");
            }),
          trackingProtectionMode: getPrivacyAPI(
            extension, "websites.trackingProtectionMode",
            () => {
              if (Preferences.get("privacy.trackingprotection.enabled")) {
                return "always";
              } else if (Preferences.get("privacy.trackingprotection.pbmode.enabled")) {
                return "private_browsing";
              }
              return "never";
            }),

        },
      },
    };
  }
};
