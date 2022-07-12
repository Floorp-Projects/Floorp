/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module consolidates various code and data update requests, so flags
 * can be set, Telemetry collected, etc. in a central place.
 */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "ProxyService",
  "@mozilla.org/network/protocol-proxy-service;1",
  "nsIProtocolProxyService"
);

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ExtensionPreferencesManager:
    "resource://gre/modules/ExtensionPreferencesManager.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "CaptivePortalService",
  "@mozilla.org/network/captive-portal-service;1",
  "nsICaptivePortalService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gNetworkLinkService",
  "@mozilla.org/network/network-link-service;1",
  "nsINetworkLinkService"
);

var EXPORTED_SYMBOLS = ["ServiceRequest"];

const PROXY_CONFIG_TYPES = [
  "direct",
  "manual",
  "pac",
  "unused", // nsIProtocolProxyService.idl skips index 3.
  "wpad",
  "system",
];

function recordEvent(service, source = {}) {
  try {
    Services.telemetry.setEventRecordingEnabled("service_request", true);
    Services.telemetry.recordEvent(
      "service_request",
      "bypass",
      "proxy_info",
      service,
      source
    );
  } catch (err) {
    // If the telemetry throws just log the error so it doesn't break any
    // functionality.
    Cu.reportError(err);
  }
}

// If proxy.settings is used to change the proxy, an extension will
// be "in control".  This returns the id of that extension.
async function getControllingExtension() {
  if (
    !WebExtensionPolicy.getActiveExtensions().some(p =>
      p.permissions.includes("proxy")
    )
  ) {
    return undefined;
  }
  // Is this proxied by an extension that set proxy prefs?
  let setting = await lazy.ExtensionPreferencesManager.getSetting(
    "proxy.settings"
  );
  return setting?.id;
}

async function getProxySource(proxyInfo) {
  // sourceId is set when using proxy.onRequest
  if (proxyInfo.sourceId) {
    return {
      source: proxyInfo.sourceId,
      type: "api",
    };
  }
  let type = PROXY_CONFIG_TYPES[lazy.ProxyService.proxyConfigType] || "unknown";

  // If we have a policy it will have set the prefs.
  if (
    Services.policies &&
    Services.policies.status === Services.policies.ACTIVE
  ) {
    let policies = Services.policies.getActivePolicies()?.filter(p => p.Proxy);
    if (policies?.length) {
      return {
        source: "policy",
        type,
      };
    }
  }

  let source = await getControllingExtension();
  return {
    source: source || "prefs",
    type,
  };
}

/**
 * ServiceRequest is intended to be a drop-in replacement for current users
 * of XMLHttpRequest.
 *
 * @param {Object} options - Options for underlying XHR, e.g. { mozAnon: bool }
 */
class ServiceRequest extends XMLHttpRequest {
  constructor(options) {
    super(options);
  }
  /**
   * Opens an XMLHttpRequest, and sets the NSS "beConservative" flag.
   * Requests are always async.
   *
   * @param {String} method - HTTP method to use, e.g. "GET".
   * @param {String} url - URL to open.
   * @param {Object} options - Additional options { bypassProxy: bool }.
   */
  open(method, url, options) {
    super.open(method, url, true);

    if (super.channel instanceof Ci.nsIHttpChannelInternal) {
      let internal = super.channel.QueryInterface(Ci.nsIHttpChannelInternal);
      // Disable cutting edge features, like TLS 1.3, where middleboxes might brick us
      internal.beConservative = true;
      // Disable use of proxy for this request if necessary.
      if (options?.bypassProxy && this.bypassProxyEnabled) {
        internal.bypassProxy = true;
      }
    }
  }

  get bypassProxy() {
    let { channel } = this;
    return channel.QueryInterface(Ci.nsIHttpChannelInternal).bypassProxy;
  }

  get isProxied() {
    let { channel } = this;
    return !!(channel instanceof Ci.nsIProxiedChannel && channel.proxyInfo);
  }

  get bypassProxyEnabled() {
    return Services.prefs.getBoolPref("network.proxy.allow_bypass", true);
  }

  static async logProxySource(channel, service) {
    if (channel.proxyInfo) {
      let source = await getProxySource(channel.proxyInfo);
      recordEvent(service, source);
    }
  }

  static get isOffline() {
    try {
      return (
        Services.io.offline ||
        lazy.CaptivePortalService.state ==
          lazy.CaptivePortalService.LOCKED_PORTAL ||
        !lazy.gNetworkLinkService.isLinkUp
      );
    } catch (ex) {
      // we cannot get state, assume the best.
    }
    return false;
  }
}
