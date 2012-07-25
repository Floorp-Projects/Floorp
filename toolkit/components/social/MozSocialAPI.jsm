/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SocialService", "resource://gre/modules/SocialService.jsm");

const EXPORTED_SYMBOLS = ["MozSocialAPI"];

var MozSocialAPI = {
  _enabled: false,
  set enabled(val) {
    let enable = !!val;
    if (enable == this._enabled) {
      return;
    }
    this._enabled = enable;

    if (enable) {
      Services.obs.addObserver(injectController, "document-element-inserted", false);
    } else {
      Services.obs.removeObserver(injectController, "document-element-inserted", false);
    }
  }
};

// Called on document-element-inserted, checks that the API should be injected,
// and then calls attachToWindow as appropriate
function injectController(doc, topic, data) {
  try {
    let window = doc.defaultView;
    if (!window)
      return;

    var containingBrowser = window.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIWebNavigation)
                                  .QueryInterface(Ci.nsIDocShell)
                                  .chromeEventHandler;

    // If the containing browser isn't one of the social browsers, nothing to
    // do here.
    // XXX this is app-specific, might want some mechanism for consumers to
    // whitelist other IDs/windowtypes  
    if (!(containingBrowser.id == "social-sidebar-browser" ||
          containingBrowser.id == "social-notification-browser")) {
      return;
    }

    let origin = containingBrowser.getAttribute("origin");
    if (!origin) {
      return;
    }

    SocialService.getProvider(origin, function(provider) {
      if (provider && provider.workerURL) {
        attachToWindow(provider, window);
      }
    });
  } catch(e) {
    Cu.reportError("MozSocialAPI injectController: unable to attachToWindow for " + doc.location + ": " + e);
  }
}

// Loads mozSocial support functions associated with provider into targetWindow
function attachToWindow(provider, targetWindow) {
  let origin = provider.origin;
  if (!provider.enabled) {
    throw new Error("MozSocialAPI: cannot attach disabled provider " + origin);
  }

  let targetDocURI = targetWindow.document.documentURIObject;
  if (provider.origin != targetDocURI.prePath) {
    throw new Error("MozSocialAPI: cannot attach " + origin + " to " + targetDocURI.spec);
  }

  var port = provider._getWorkerPort(targetWindow);

  let mozSocialObj = {
    // Use a method for backwards compat with existing providers, but we
    // should deprecate this in favor of a simple .port getter.
    getWorker: function() {
      return {
        port: port,
        __exposedProps__: {
          port: "r"
        }
      };
    },
    hasBeenIdleFor: function () {
      return false;
    }
  };

  let contentObj = Cu.createObjectIn(targetWindow);
  let propList = {};
  for (let prop in mozSocialObj) {
    propList[prop] = {
      enumerable: true,
      configurable: true,
      writable: true,
      value: mozSocialObj[prop]
    };
  }
  Object.defineProperties(contentObj, propList);
  Cu.makeObjectPropsNormal(contentObj);

  targetWindow.navigator.wrappedJSObject.__defineGetter__("mozSocial", function() {
    // We do this in a getter, so that we create these objects
    // only on demand (this is a potential concern, since
    // otherwise we might add one per iframe, and keep them
    // alive for as long as the window is alive).
    delete targetWindow.navigator.wrappedJSObject.mozSocial;
    return targetWindow.navigator.wrappedJSObject.mozSocial = contentObj;
  });

  targetWindow.addEventListener("unload", function () {
    // We want to close the port, but also want the target window to be
    // able to use the port during an unload event they setup - so we
    // set a timer which will fire after the unload events have all fired.
    schedule(function () { port.close(); });
  });
}

function schedule(callback) {
  Services.tm.mainThread.dispatch(callback, Ci.nsIThread.DISPATCH_NORMAL);
}
