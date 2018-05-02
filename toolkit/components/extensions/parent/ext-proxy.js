/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(this, "ProxyScriptContext",
                               "resource://gre/modules/ProxyScriptContext.jsm");
ChromeUtils.defineModuleGetter(this, "ProxyChannelFilter",
                               "resource://gre/modules/ProxyScriptContext.jsm");
ChromeUtils.import("resource://gre/modules/ExtensionPreferencesManager.jsm");

var {
  ExtensionError,
} = ExtensionUtils;
var {
  getSettingsAPI,
} = ExtensionPreferencesManager;

// WeakMap[Extension -> ProxyScriptContext]
const proxyScriptContextMap = new WeakMap();

const proxySvc = Ci.nsIProtocolProxyService;

const PROXY_TYPES_MAP = new Map([
  ["none", proxySvc.PROXYCONFIG_DIRECT],
  ["autoDetect", proxySvc.PROXYCONFIG_WPAD],
  ["system", proxySvc.PROXYCONFIG_SYSTEM],
  ["manual", proxySvc.PROXYCONFIG_MANUAL],
  ["autoConfig", proxySvc.PROXYCONFIG_PAC],
]);

ExtensionPreferencesManager.addSetting("proxy.settings", {
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
        let url = new URL(`http://${value[prop]}`);
        prefs[`network.proxy.${prop}`] = url.hostname;
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

function registerProxyFilterEvent(context, extension, fire, filterProps, extraInfoSpec = []) {
  let listener = (data) => {
    return fire.sync(data);
  };

  let filter = {...filterProps};
  if (filter.urls) {
    let perms = new MatchPatternSet([...extension.whiteListedHosts.patterns,
                                     ...extension.optionalOrigins.patterns]);

    filter.urls = new MatchPatternSet(filter.urls);

    if (!perms.overlapsAll(filter.urls)) {
      Cu.reportError("The proxy.onRequest filter doesn't overlap with host permissions.");
    }
  }

  let proxyFilter = new ProxyChannelFilter(context, extension, listener, filter, extraInfoSpec);
  return {
    unregister: () => { proxyFilter.destroy(); },
    convert(_fire, _context) {
      fire = _fire;
      proxyFilter.context = _context;
    },
  };
}

this.proxy = class extends ExtensionAPI {
  onShutdown() {
    let {extension} = this;

    let proxyScriptContext = proxyScriptContextMap.get(extension);
    if (proxyScriptContext) {
      proxyScriptContext.unload();
      proxyScriptContextMap.delete(extension);
    }
  }

  primeListener(extension, event, fire, params) {
    if (event === "onRequest") {
      return registerProxyFilterEvent(undefined, extension, fire, ...params);
    }
  }

  getAPI(context) {
    let {extension} = context;

    // Leaving as non-persistent.  By itself it's not useful since proxy-error
    // is emitted from the proxy filter.
    let onError = new EventManager({
      context,
      name: "proxy.onError",
      register: fire => {
        let listener = (name, error) => {
          fire.async(error);
        };
        extension.on("proxy-error", listener);
        return () => {
          extension.off("proxy-error", listener);
        };
      },
    }).api();

    return {
      proxy: {
        register(url) {
          this.unregister();

          let proxyScriptContext = new ProxyScriptContext(extension, url);
          if (proxyScriptContext.load()) {
            proxyScriptContextMap.set(extension, proxyScriptContext);
          }
        },

        unregister() {
          // Unload the current proxy script if one is loaded.
          if (proxyScriptContextMap.has(extension)) {
            proxyScriptContextMap.get(extension).unload();
            proxyScriptContextMap.delete(extension);
          }
        },

        registerProxyScript(url) {
          this.register(url);
        },

        onRequest: new EventManager({
          context,
          name: `proxy.onRequest`,
          persistent: {
            module: "proxy",
            event: "onRequest",
          },
          register: (fire, filter, info) => {
            return registerProxyFilterEvent(context, context.extension, fire, filter, info).unregister;
          },
        }).api(),

        onError,

        // TODO Bug 1388619 deprecate onProxyError.
        onProxyError: onError,

        settings: Object.assign(
          getSettingsAPI(
            extension.id, "proxy.settings",
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
                let host = Services.prefs.getCharPref(`network.proxy.${prop}`);
                let port = Services.prefs.getIntPref(`network.proxy.${prop}_port`);
                proxyConfig[prop] = port ? `${host}:${port}` : host;
              }

              return proxyConfig;
            },
            // proxy.settings is unsupported on android.
            undefined, false, () => {
              if (AppConstants.platform == "android") {
                throw new ExtensionError(
                  `proxy.settings is not supported on android.`);
              }
            },
          ),
          {
            set: details => {
              if (AppConstants.platform === "android") {
                throw new ExtensionError(
                  "proxy.settings is not supported on android.");
              }

              if (!Services.policies.isAllowed("changeProxySettings")) {
                throw new ExtensionError(
                  "Proxy settings are being managed by the Policies manager.");
              }

              let value = details.value;

              if (!PROXY_TYPES_MAP.has(value.proxyType)) {
                throw new ExtensionError(
                  `${value.proxyType} is not a valid value for proxyType.`);
              }

              if (value.httpProxyAll) {
                // Match what about:preferences does with proxy settings
                // since the proxy service does not check the value
                // of share_proxy_settings.
                for (let prop of ["ftp", "ssl", "socks"]) {
                  value[prop] = value.http;
                }
              }

              for (let prop of ["http", "ftp", "ssl", "socks"]) {
                let host = value[prop];
                if (host) {
                  try {
                    // Fixup in case a full url is passed.
                    if (host.includes("://")) {
                      value[prop] = new URL(host).host;
                    } else {
                      // Validate the host value.
                      new URL(`http://${host}`);
                    }
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
                extension.id, "proxy.settings", value);
            },
          }
        ),
      },
    };
  }
};
