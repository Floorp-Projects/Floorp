/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-unused-vars: ["error", {args: "none"}] */

var EXPORTED_SYMBOLS = ["WebChannelChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { ContentDOMReference } = ChromeUtils.import(
  "resource://gre/modules/ContentDOMReference.jsm"
);

// Preference containing the list (space separated) of origins that are
// allowed to send non-string values through a WebChannel, mainly for
// backwards compatability. See bug 1238128 for more information.
const URL_WHITELIST_PREF = "webchannel.allowObject.urlWhitelist";

let _cachedWhitelist = null;

const CACHED_PREFS = {};
XPCOMUtils.defineLazyPreferenceGetter(
  CACHED_PREFS,
  "URL_WHITELIST",
  URL_WHITELIST_PREF,
  "",
  // Null this out so we update it.
  () => (_cachedWhitelist = null)
);

class WebChannelChild extends JSWindowActorChild {
  handleEvent(event) {
    if (event.type === "WebChannelMessageToChrome") {
      return this._onMessageToChrome(event);
    }
    return undefined;
  }

  receiveMessage(msg) {
    if (msg.name === "WebChannelMessageToContent") {
      return this._onMessageToContent(msg);
    }
    return undefined;
  }

  _getWhitelistedPrincipals() {
    if (!_cachedWhitelist) {
      let urls = CACHED_PREFS.URL_WHITELIST.split(/\s+/);
      _cachedWhitelist = urls.map(origin =>
        Services.scriptSecurityManager.createContentPrincipalFromOrigin(origin)
      );
    }
    return _cachedWhitelist;
  }

  _onMessageToChrome(e) {
    // If target is window then we want the document principal, otherwise fallback to target itself.
    let principal = e.target.nodePrincipal
      ? e.target.nodePrincipal
      : e.target.document.nodePrincipal;

    if (e.detail) {
      if (typeof e.detail != "string") {
        // Check if the principal is one of the ones that's allowed to send
        // non-string values for e.detail.  They're whitelisted by site origin,
        // so we compare on originNoSuffix in order to avoid other origin attributes
        // that are not relevant here, such as containers or private browsing.
        let objectsAllowed = this._getWhitelistedPrincipals().some(
          whitelisted => principal.originNoSuffix == whitelisted.originNoSuffix
        );
        if (!objectsAllowed) {
          Cu.reportError(
            "WebChannelMessageToChrome sent with an object from a non-whitelisted principal"
          );
          return;
        }
      }

      let eventTarget =
        e.target instanceof Ci.nsIDOMWindow
          ? null
          : ContentDOMReference.get(e.target);
      this.sendAsyncMessage("WebChannelMessageToChrome", {
        contentData: e.detail,
        eventTarget,
        principal,
      });
    } else {
      Cu.reportError("WebChannel message failed. No message detail.");
    }
  }

  _onMessageToContent(msg) {
    if (msg.data && this.contentWindow) {
      // msg.objects.eventTarget will be defined if sending a response to
      // a WebChannelMessageToChrome event. An unsolicited send
      // may not have an eventTarget defined, in this case send to the
      // main content window.
      let { eventTarget, principal } = msg.data;
      if (!eventTarget) {
        eventTarget = this.contentWindow;
      } else {
        eventTarget = ContentDOMReference.resolve(eventTarget);
      }
      if (!eventTarget) {
        Cu.reportError("WebChannel message failed. No target.");
        return;
      }

      // Use nodePrincipal if available, otherwise fallback to document principal.
      let targetPrincipal =
        eventTarget instanceof Ci.nsIDOMWindow
          ? eventTarget.document.nodePrincipal
          : eventTarget.nodePrincipal;

      if (principal.subsumes(targetPrincipal)) {
        let targetWindow = this.contentWindow;
        eventTarget.dispatchEvent(
          new targetWindow.CustomEvent("WebChannelMessageToContent", {
            detail: Cu.cloneInto(
              {
                id: msg.data.id,
                message: msg.data.message,
              },
              targetWindow
            ),
          })
        );
      } else {
        Cu.reportError("WebChannel message failed. Principal mismatch.");
      }
    } else {
      Cu.reportError("WebChannel message failed. No message data.");
    }
  }
}
