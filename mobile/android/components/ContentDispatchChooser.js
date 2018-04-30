/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Messaging.jsm");

function ContentDispatchChooser() {}

ContentDispatchChooser.prototype =
{
  classID: Components.ID("5a072a22-1e66-4100-afc1-07aed8b62fc5"),

  QueryInterface: ChromeUtils.generateQI([Ci.nsIContentDispatchChooser]),

  get protoSvc() {
    if (!this._protoSvc) {
      this._protoSvc = Cc["@mozilla.org/uriloader/external-protocol-service;1"].getService(Ci.nsIExternalProtocolService);
    }
    return this._protoSvc;
  },

  _getChromeWin: function getChromeWin() {
    try {
      return Services.wm.getMostRecentWindow("navigator:browser");
    } catch (e) {
      throw Cr.NS_ERROR_FAILURE;
    }
  },

  _closeBlankWindow: function(aWindow) {
    if (!aWindow || aWindow.history.length) {
      return;
    }
    if (!aWindow.location.href || aWindow.location.href === "about:blank") {
      aWindow.close();
    }
  },

  ask: function ask(aHandler, aWindowContext, aURI, aReason) {
    let window = null;
    try {
      if (aWindowContext)
        window = aWindowContext.getInterface(Ci.nsIDOMWindow);
    } catch (e) { /* it's OK to not have a window */ }

    // The current list is based purely on the scheme. Redo the query using the url to get more
    // specific results.
    aHandler = this.protoSvc.getProtocolHandlerInfoFromOS(aURI.spec, {});

    // The first handler in the set is the Android Application Chooser (which will fall back to a default if one is set)
    // If we have more than one option, let the OS handle showing a list (if needed).
    if (aHandler.possibleApplicationHandlers.length > 1) {
      aHandler.launchWithURI(aURI, aWindowContext);
      this._closeBlankWindow(window);

    } else {
      // xpcshell tests do not have an Android Bridge but we require Android
      // Bridge when using Messaging so we guard against this case. xpcshell
      // tests also do not have a window, so we use this state to guard.
      let win = this._getChromeWin();
      if (!win) {
        return;
      }

      let msg = {
        type: "Intent:OpenNoHandler",
        uri: aURI.spec,
      };

      EventDispatcher.instance.sendRequestForResult(msg).then(() => {
        // Java opens an app on success: take no action.
        this._closeBlankWindow(window);

      }, (data) => {
        if (data.isFallback) {
          // We always want to open a fallback url
          window.location.href = data.uri;
          return;
        }

        // We couldn't open this. If this was from a click, it's likely that we just
        // want this to fail silently. If the user entered this on the address bar, though,
        // we want to show the neterror page.
        let dwu = window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
        let millis = dwu.millisSinceLastUserInput;
        if (millis < 0 || millis >= 1000) {
          window.document.docShell.displayLoadError(Cr.NS_ERROR_UNKNOWN_PROTOCOL, aURI, null);
        } else {
          this._closeBlankWindow(window);
        }
      });
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentDispatchChooser]);
