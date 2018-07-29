/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-unused-vars: ["error", {args: "none"}] */

var EXPORTED_SYMBOLS = ["WebChannelContent"];

ChromeUtils.import("resource://gre/modules/Services.jsm");

function getMessageManager(event) {
  let window = Cu.getGlobalForObject(event.target);

  return window.document.docShell
               .QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIContentFrameMessageManager);
}

var WebChannelContent = {
  // Preference containing the list (space separated) of origins that are
  // allowed to send non-string values through a WebChannel, mainly for
  // backwards compatability. See bug 1238128 for more information.
  URL_WHITELIST_PREF: "webchannel.allowObject.urlWhitelist",

  // Cached list of whitelisted principals, we avoid constructing this if the
  // value in `_lastWhitelistValue` hasn't changed since we constructed it last.
  _cachedWhitelist: [],
  _lastWhitelistValue: "",

  handleEvent(event) {
    if (event.type === "WebChannelMessageToChrome") {
      return this._onMessageToChrome(event);
    }
    return undefined;
  },

  receiveMessage(msg) {
    if (msg.name === "WebChannelMessageToContent") {
      return this._onMessageToContent(msg);
    }
    return undefined;
  },

  _getWhitelistedPrincipals() {
    let whitelist = Services.prefs.getCharPref(this.URL_WHITELIST_PREF);
    if (whitelist != this._lastWhitelistValue) {
      let urls = whitelist.split(/\s+/);
      this._cachedWhitelist = urls.map(origin =>
        Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(origin));
    }
    return this._cachedWhitelist;
  },

  _onMessageToChrome(e) {
    // If target is window then we want the document principal, otherwise fallback to target itself.
    let principal = e.target.nodePrincipal ? e.target.nodePrincipal : e.target.document.nodePrincipal;

    if (e.detail) {
      if (typeof e.detail != "string") {
        // Check if the principal is one of the ones that's allowed to send
        // non-string values for e.detail.  They're whitelisted by site origin,
        // so we compare on originNoSuffix in order to avoid other origin attributes
        // that are not relevant here, such as containers or private browsing.
        let objectsAllowed = this._getWhitelistedPrincipals().some(whitelisted =>
          principal.originNoSuffix == whitelisted.originNoSuffix);
        if (!objectsAllowed) {
          Cu.reportError("WebChannelMessageToChrome sent with an object from a non-whitelisted principal");
          return;
        }
      }

      let mm = getMessageManager(e);

      mm.sendAsyncMessage("WebChannelMessageToChrome", e.detail, { eventTarget: e.target }, principal);
    } else {
      Cu.reportError("WebChannel message failed. No message detail.");
    }
  },

  _onMessageToContent(msg) {
    if (msg.data) {
      // msg.objects.eventTarget will be defined if sending a response to
      // a WebChannelMessageToChrome event. An unsolicited send
      // may not have an eventTarget defined, in this case send to the
      // main content window.
      let eventTarget = msg.objects.eventTarget || msg.target.content;

      // Use nodePrincipal if available, otherwise fallback to document principal.
      let targetPrincipal = eventTarget instanceof Ci.nsIDOMWindow ? eventTarget.document.nodePrincipal : eventTarget.nodePrincipal;

      if (msg.principal.subsumes(targetPrincipal)) {
        // If eventTarget is a window, use it as the targetWindow, otherwise
        // find the window that owns the eventTarget.
        let targetWindow = eventTarget instanceof Ci.nsIDOMWindow ? eventTarget : eventTarget.ownerGlobal;

        eventTarget.dispatchEvent(new targetWindow.CustomEvent("WebChannelMessageToContent", {
          detail: Cu.cloneInto({
            id: msg.data.id,
            message: msg.data.message,
          }, targetWindow),
        }));
      } else {
        Cu.reportError("WebChannel message failed. Principal mismatch.");
      }
    } else {
      Cu.reportError("WebChannel message failed. No message data.");
    }
  },
};
