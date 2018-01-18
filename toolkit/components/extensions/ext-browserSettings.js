/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

Cu.import("resource://gre/modules/ExtensionPreferencesManager.jsm");

var {
  ExtensionError,
} = ExtensionUtils;

const proxySvc = Ci.nsIProtocolProxyService;

const PROXY_TYPES_MAP = new Map([
  ["none", proxySvc.PROXYCONFIG_DIRECT],
  ["autoDetect", proxySvc.PROXYCONFIG_WPAD],
  ["system", proxySvc.PROXYCONFIG_SYSTEM],
  ["manual", proxySvc.PROXYCONFIG_MANUAL],
  ["autoConfig", proxySvc.PROXYCONFIG_PAC],
]);

const HOMEPAGE_OVERRIDE_SETTING = "homepage_override";
const HOMEPAGE_URL_PREF = "browser.startup.homepage";
const URL_STORE_TYPE = "url_overrides";
const NEW_TAB_OVERRIDE_SETTING = "newTabURL";

const PERM_DENY_ACTION = Services.perms.DENY_ACTION;

const checkUnsupported = (name, unsupportedPlatforms) => {
  if (unsupportedPlatforms.includes(AppConstants.platform)) {
    throw new ExtensionError(
      `${AppConstants.platform} is not a supported platform for the ${name} setting.`);
  }
};

const getSettingsAPI = (extension, name, callback, storeType, readOnly = false, unsupportedPlatforms = []) => {
  return {
    async get(details) {
      checkUnsupported(name, unsupportedPlatforms);
      let levelOfControl = details.incognito ?
        "not_controllable" :
        await ExtensionPreferencesManager.getLevelOfControl(
          extension.id, name, storeType);
      levelOfControl =
        (readOnly && levelOfControl === "controllable_by_this_extension") ?
          "not_controllable" :
          levelOfControl;
      return {
        levelOfControl,
        value: await callback(),
      };
    },
    set(details) {
      checkUnsupported(name, unsupportedPlatforms);
      if (!readOnly) {
        return ExtensionPreferencesManager.setSetting(
          extension.id, name, details.value);
      }
      return false;
    },
    clear(details) {
      checkUnsupported(name, unsupportedPlatforms);
      if (!readOnly) {
        return ExtensionPreferencesManager.removeSetting(extension.id, name);
      }
      return false;
    },
  };
};

// Add settings objects for supported APIs to the preferences manager.
ExtensionPreferencesManager.addSetting("allowPopupsForUserEvents", {
  prefNames: [
    "dom.popup_allowed_events",
  ],

  setCallback(value) {
    let returnObj = {};
    // If the value is true, then reset the pref, otherwise set it to "".
    returnObj[this.prefNames[0]] = value ? undefined : "";
    return returnObj;
  },
});

ExtensionPreferencesManager.addSetting("cacheEnabled", {
  prefNames: [
    "browser.cache.disk.enable",
    "browser.cache.memory.enable",
  ],

  setCallback(value) {
    let returnObj = {};
    for (let pref of this.prefNames) {
      returnObj[pref] = value;
    }
    return returnObj;
  },
});

ExtensionPreferencesManager.addSetting("contextMenuShowEvent", {
  prefNames: [
    "ui.context_menus.after_mouseup",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value === "mouseup"};
  },
});

ExtensionPreferencesManager.addSetting("imageAnimationBehavior", {
  prefNames: [
    "image.animation_mode",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

ExtensionPreferencesManager.addSetting("openBookmarksInNewTabs", {
  prefNames: [
    "browser.tabs.loadBookmarksInTabs",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

ExtensionPreferencesManager.addSetting("openSearchResultsInNewTabs", {
  prefNames: [
    "browser.search.openintab",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

ExtensionPreferencesManager.addSetting("proxyConfig", {
  prefNames: [
    "network.proxy.type",
    "network.proxy.http",
    "network.proxy.http_port",
    "network.proxy.share_proxy_settings",
    "network.proxy.ftp",
    "network.proxy.ftp_port",
    "network.proxy.ssl",
    "network.proxy.ssl_port",
    "network.proxy.socks",
    "network.proxy.socks_port",
    "network.proxy.socks_version",
    "network.proxy.socks_remote_dns",
    "network.proxy.no_proxies_on",
    "network.proxy.autoconfig_url",
    "signon.autologin.proxy",
  ],

  setCallback(value) {
    let prefs = {
      "network.proxy.type": PROXY_TYPES_MAP.get(value.proxyType),
      "signon.autologin.proxy": value.autoLogin,
      "network.proxy.socks_remote_dns": value.proxyDNS,
      "network.proxy.autoconfig_url": value.autoConfigUrl,
      "network.proxy.share_proxy_settings": value.httpProxyAll,
      "network.proxy.socks_version": value.socksVersion,
      "network.proxy.no_proxies_on": value.passthrough,
    };

    for (let prop of ["http", "ftp", "ssl", "socks"]) {
      if (value[prop]) {
        let url = new URL(prop === "socks" ?
                          `http://${value[prop]}` :
                          value[prop]);
        prefs[`network.proxy.${prop}`] = prop === "socks" ?
          url.hostname :
          `${url.protocol}//${url.hostname}`;
        let port = parseInt(url.port, 10);
        prefs[`network.proxy.${prop}_port`] = isNaN(port) ? 0 : port;
      } else {
        prefs[`network.proxy.${prop}`] = undefined;
        prefs[`network.proxy.${prop}_port`] = undefined;
      }
    }

    return prefs;
  },
});

ExtensionPreferencesManager.addSetting("webNotificationsDisabled", {
  prefNames: [
    "permissions.default.desktop-notification",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value ? PERM_DENY_ACTION : undefined};
  },
});

this.browserSettings = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      browserSettings: {
        allowPopupsForUserEvents: getSettingsAPI(
          extension, "allowPopupsForUserEvents",
          () => {
            return Services.prefs.getCharPref("dom.popup_allowed_events") != "";
          }),
        cacheEnabled: getSettingsAPI(
          extension, "cacheEnabled",
          () => {
            return Services.prefs.getBoolPref("browser.cache.disk.enable") &&
              Services.prefs.getBoolPref("browser.cache.memory.enable");
          }),
        contextMenuShowEvent: Object.assign(
          getSettingsAPI(
            extension, "contextMenuShowEvent",
            () => {
              if (AppConstants.platform === "win") {
                return "mouseup";
              }
              let prefValue = Services.prefs.getBoolPref(
                "ui.context_menus.after_mouseup", null);
              return prefValue ? "mouseup" : "mousedown";
            }
          ),
          {
            set: details => {
              if (!["mouseup", "mousedown"].includes(details.value)) {
                throw new ExtensionError(
                  `${details.value} is not a valid value for contextMenuShowEvent.`);
              }
              if (AppConstants.platform === "android" ||
                  (AppConstants.platform === "win" &&
                   details.value === "mousedown")) {
                return false;
              }
              return ExtensionPreferencesManager.setSetting(
                extension.id, "contextMenuShowEvent", details.value);
            },
          }
        ),
        homepageOverride: getSettingsAPI(
          extension, HOMEPAGE_OVERRIDE_SETTING,
          () => {
            return Services.prefs.getComplexValue(
              HOMEPAGE_URL_PREF, Ci.nsIPrefLocalizedString).data;
          }, undefined, true),
        imageAnimationBehavior: getSettingsAPI(
          extension, "imageAnimationBehavior",
          () => {
            return Services.prefs.getCharPref("image.animation_mode");
          }),
        newTabPageOverride: getSettingsAPI(
          extension, NEW_TAB_OVERRIDE_SETTING,
          () => {
            return aboutNewTabService.newTabURL;
          }, URL_STORE_TYPE, true),
        openBookmarksInNewTabs: getSettingsAPI(
          extension, "openBookmarksInNewTabs",
          () => {
            return Services.prefs.getBoolPref("browser.tabs.loadBookmarksInTabs");
          }),
        openSearchResultsInNewTabs: getSettingsAPI(
          extension, "openSearchResultsInNewTabs",
          () => {
            return Services.prefs.getBoolPref("browser.search.openintab");
          }),
        proxyConfig: Object.assign(
          getSettingsAPI(
            extension, "proxyConfig",
            () => {
              let prefValue = Services.prefs.getIntPref("network.proxy.type");
              let proxyConfig = {
                proxyType:
                  Array.from(
                    PROXY_TYPES_MAP.entries()).find(entry => entry[1] === prefValue)[0],
                autoConfigUrl: Services.prefs.getCharPref("network.proxy.autoconfig_url"),
                autoLogin: Services.prefs.getBoolPref("signon.autologin.proxy"),
                proxyDNS: Services.prefs.getBoolPref("network.proxy.socks_remote_dns"),
                httpProxyAll: Services.prefs.getBoolPref("network.proxy.share_proxy_settings"),
                socksVersion: Services.prefs.getIntPref("network.proxy.socks_version"),
                passthrough: Services.prefs.getCharPref("network.proxy.no_proxies_on"),
              };

              for (let prop of ["http", "ftp", "ssl", "socks"]) {
                let url = Services.prefs.getCharPref(`network.proxy.${prop}`);
                let port = Services.prefs.getIntPref(`network.proxy.${prop}_port`);
                proxyConfig[prop] = port ? `${url}:${port}` : url;
              }

              return proxyConfig;
            },
            // proxyConfig is unsupported on android.
            undefined, false, ["android"]
          ),
          {
            set: details => {
              if (AppConstants.platform === "android") {
                throw new ExtensionError(
                  "proxyConfig is not supported on android.");
              }

              let value = details.value;

              if (!PROXY_TYPES_MAP.has(value.proxyType)) {
                throw new ExtensionError(
                  `${value.proxyType} is not a valid value for proxyType.`);
              }

              for (let prop of ["http", "ftp", "ssl", "socks"]) {
                let url = value[prop];
                if (url) {
                  if (prop === "socks") {
                    url = `http://${url}`;
                  }
                  try {
                    new URL(url);
                  } catch (e) {
                    throw new ExtensionError(
                      `${value[prop]} is not a valid value for ${prop}.`);
                  }
                }
              }

              if (value.proxyType === "autoConfig" || value.autoConfigUrl) {
                try {
                  new URL(value.autoConfigUrl);
                } catch (e) {
                  throw new ExtensionError(
                    `${value.autoConfigUrl} is not a valid value for autoConfigUrl.`);
                }
              }

              if (value.socksVersion !== undefined) {
                if (!Number.isInteger(value.socksVersion) ||
                    value.socksVersion < 4 ||
                    value.socksVersion > 5) {
                  throw new ExtensionError(
                    `${value.socksVersion} is not a valid value for socksVersion.`);
                }
              }

              return ExtensionPreferencesManager.setSetting(
                extension.id, "proxyConfig", value);
            },
          }
        ),
        webNotificationsDisabled: getSettingsAPI(
          extension, "webNotificationsDisabled",
          () => {
            let prefValue =
              Services.prefs.getIntPref(
                "permissions.default.desktop-notification", null);
            return prefValue === PERM_DENY_ACTION;
          }),
      },
    };
  }
};
