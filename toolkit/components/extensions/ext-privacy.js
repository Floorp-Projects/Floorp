/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

Cu.import("resource://gre/modules/ExtensionPreferencesManager.jsm");
var {
  ExtensionError,
} = ExtensionUtils;

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
            extension, name),
        value: await callback(),
      };
    },
    async set(details) {
      checkScope(details.scope);
      return await ExtensionPreferencesManager.setSetting(
        extension, name, details.value);
    },
    async clear(details) {
      checkScope(details.scope);
      return await ExtensionPreferencesManager.removeSetting(
        extension, name);
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

this.privacy = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      privacy: {
        network: {
          networkPredictionEnabled: getPrivacyAPI(extension,
            "network.networkPredictionEnabled",
            () => {
              return Preferences.get("network.predictor.enabled") &&
                Preferences.get("network.prefetch-next") &&
                Preferences.get("network.http.speculative-parallel-limit") > 0 &&
                !Preferences.get("network.dns.disablePrefetch");
            }),
          peerConnectionEnabled: getPrivacyAPI(extension,
            "network.peerConnectionEnabled",
            () => {
              return Preferences.get("media.peerconnection.enabled");
            }),
          webRTCIPHandlingPolicy: getPrivacyAPI(extension,
            "network.webRTCIPHandlingPolicy",
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
          passwordSavingEnabled: getPrivacyAPI(extension,
            "services.passwordSavingEnabled",
            () => {
              return Preferences.get("signon.rememberSignons");
            }),
        },

        websites: {
          hyperlinkAuditingEnabled: getPrivacyAPI(extension,
            "websites.hyperlinkAuditingEnabled",
            () => {
              return Preferences.get("browser.send_pings");
            }),
          referrersEnabled: getPrivacyAPI(extension,
            "websites.referrersEnabled",
            () => {
              return Preferences.get("network.http.sendRefererHeader") !== 0;
            }),
        },
      },
    };
  }
};
