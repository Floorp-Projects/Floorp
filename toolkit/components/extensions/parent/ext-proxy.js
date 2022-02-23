/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "ProxyChannelFilter",
  "resource://gre/modules/ProxyChannelFilter.jsm"
);
var { ExtensionPreferencesManager } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPreferencesManager.jsm"
);

var { ExtensionError } = ExtensionUtils;
var { getSettingsAPI } = ExtensionPreferencesManager;

const proxySvc = Ci.nsIProtocolProxyService;

const PROXY_TYPES_MAP = new Map([
  ["none", proxySvc.PROXYCONFIG_DIRECT],
  ["autoDetect", proxySvc.PROXYCONFIG_WPAD],
  ["system", proxySvc.PROXYCONFIG_SYSTEM],
  ["manual", proxySvc.PROXYCONFIG_MANUAL],
  ["autoConfig", proxySvc.PROXYCONFIG_PAC],
]);

const DEFAULT_PORTS = new Map([
  ["http", 80],
  ["ssl", 443],
  ["socks", 1080],
]);

ExtensionPreferencesManager.addSetting("proxy.settings", {
  permission: "proxy",
  prefNames: [
    "network.proxy.type",
    "network.proxy.http",
    "network.proxy.http_port",
    "network.proxy.share_proxy_settings",
    "network.proxy.ssl",
    "network.proxy.ssl_port",
    "network.proxy.socks",
    "network.proxy.socks_port",
    "network.proxy.socks_version",
    "network.proxy.socks_remote_dns",
    "network.proxy.no_proxies_on",
    "network.proxy.autoconfig_url",
    "signon.autologin.proxy",
    "network.http.proxy.respect-be-conservative",
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
      "network.http.proxy.respect-be-conservative": value.respectBeConservative,
    };

    for (let prop of ["http", "ssl", "socks"]) {
      if (value[prop]) {
        let url = new URL(`http://${value[prop]}`);
        prefs[`network.proxy.${prop}`] = url.hostname;
        // Only fall back to defaults if no port provided.
        let [, rawPort] = value[prop].split(":");
        let port = parseInt(rawPort, 10) || DEFAULT_PORTS.get(prop);
        prefs[`network.proxy.${prop}_port`] = port;
      }
    }

    return prefs;
  },
});

function registerProxyFilterEvent(
  context,
  extension,
  fire,
  filterProps,
  extraInfoSpec = []
) {
  let listener = data => {
    return fire.sync(data);
  };

  let filter = { ...filterProps };
  if (filter.urls) {
    let perms = new MatchPatternSet([
      ...extension.allowedOrigins.patterns,
      ...extension.optionalOrigins.patterns,
    ]);
    filter.urls = new MatchPatternSet(filter.urls);

    if (!perms.overlapsAll(filter.urls)) {
      Cu.reportError(
        "The proxy.onRequest filter doesn't overlap with host permissions."
      );
    }
  }

  let proxyFilter = new ProxyChannelFilter(
    context,
    extension,
    listener,
    filter,
    extraInfoSpec
  );
  return {
    unregister: () => {
      proxyFilter.destroy();
    },
    convert(_fire, _context) {
      fire = _fire;
      proxyFilter.context = _context;
    },
  };
}

this.proxy = class extends ExtensionAPI {
  primeListener(extension, event, fire, params) {
    if (event === "onRequest") {
      return registerProxyFilterEvent(undefined, extension, fire, ...params);
    }
  }

  getAPI(context) {
    let { extension } = context;

    return {
      proxy: {
        onRequest: new EventManager({
          context,
          name: `proxy.onRequest`,
          persistent: {
            module: "proxy",
            event: "onRequest",
          },
          register: (fire, filter, info) => {
            return registerProxyFilterEvent(
              context,
              context.extension,
              fire,
              filter,
              info
            ).unregister;
          },
        }).api(),

        // Leaving as non-persistent.  By itself it's not useful since proxy-error
        // is emitted from the proxy filter.
        onError: new EventManager({
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
        }).api(),

        settings: Object.assign(
          getSettingsAPI({
            context,
            name: "proxy.settings",
            callback() {
              let prefValue = Services.prefs.getIntPref("network.proxy.type");
              let proxyConfig = {
                proxyType: Array.from(PROXY_TYPES_MAP.entries()).find(
                  entry => entry[1] === prefValue
                )[0],
                autoConfigUrl: Services.prefs.getCharPref(
                  "network.proxy.autoconfig_url"
                ),
                autoLogin: Services.prefs.getBoolPref("signon.autologin.proxy"),
                proxyDNS: Services.prefs.getBoolPref(
                  "network.proxy.socks_remote_dns"
                ),
                httpProxyAll: Services.prefs.getBoolPref(
                  "network.proxy.share_proxy_settings"
                ),
                socksVersion: Services.prefs.getIntPref(
                  "network.proxy.socks_version"
                ),
                passthrough: Services.prefs.getCharPref(
                  "network.proxy.no_proxies_on"
                ),
              };

              if (extension.isPrivileged) {
                proxyConfig.respectBeConservative = Services.prefs.getBoolPref(
                  "network.http.proxy.respect-be-conservative"
                );
              }

              for (let prop of ["http", "ssl", "socks"]) {
                let host = Services.prefs.getCharPref(`network.proxy.${prop}`);
                let port = Services.prefs.getIntPref(
                  `network.proxy.${prop}_port`
                );
                proxyConfig[prop] = port ? `${host}:${port}` : host;
              }

              return proxyConfig;
            },
            // proxy.settings is unsupported on android.
            validate() {
              if (AppConstants.platform == "android") {
                throw new ExtensionError(
                  `proxy.settings is not supported on android.`
                );
              }
            },
          }),
          {
            set: details => {
              if (AppConstants.platform === "android") {
                throw new ExtensionError(
                  "proxy.settings is not supported on android."
                );
              }

              if (!extension.privateBrowsingAllowed) {
                throw new ExtensionError(
                  "proxy.settings requires private browsing permission."
                );
              }

              if (!Services.policies.isAllowed("changeProxySettings")) {
                throw new ExtensionError(
                  "Proxy settings are being managed by the Policies manager."
                );
              }

              let value = details.value;

              // proxyType is optional and it should default to "system" when missing.
              if (value.proxyType == null) {
                value.proxyType = "system";
              }

              if (!PROXY_TYPES_MAP.has(value.proxyType)) {
                throw new ExtensionError(
                  `${value.proxyType} is not a valid value for proxyType.`
                );
              }

              if (value.httpProxyAll) {
                // Match what about:preferences does with proxy settings
                // since the proxy service does not check the value
                // of share_proxy_settings.
                value.ssl = value.http;
              }

              for (let prop of ["http", "ssl", "socks"]) {
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
                      `${value[prop]} is not a valid value for ${prop}.`
                    );
                  }
                }
              }

              if (value.proxyType === "autoConfig" || value.autoConfigUrl) {
                try {
                  new URL(value.autoConfigUrl);
                } catch (e) {
                  throw new ExtensionError(
                    `${value.autoConfigUrl} is not a valid value for autoConfigUrl.`
                  );
                }
              }

              if (value.socksVersion !== undefined) {
                if (
                  !Number.isInteger(value.socksVersion) ||
                  value.socksVersion < 4 ||
                  value.socksVersion > 5
                ) {
                  throw new ExtensionError(
                    `${value.socksVersion} is not a valid value for socksVersion.`
                  );
                }
              }

              if (
                value.respectBeConservative !== undefined &&
                !extension.isPrivileged &&
                Services.prefs.getBoolPref(
                  "network.http.proxy.respect-be-conservative"
                ) != value.respectBeConservative
              ) {
                throw new ExtensionError(
                  `respectBeConservative can be set by privileged extensions only.`
                );
              }

              return ExtensionPreferencesManager.setSetting(
                extension.id,
                "proxy.settings",
                value
              );
            },
          }
        ),
      },
    };
  }
};
