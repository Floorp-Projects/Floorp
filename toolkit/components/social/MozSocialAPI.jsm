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
    },
    openServiceWindow: function(toURL, name, options) {
      return openServiceWindow(provider, targetWindow, toURL, name, options);
    },
    getAttention: function() {
      let mainWindow = targetWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                         .getInterface(Components.interfaces.nsIWebNavigation)
                         .QueryInterface(Components.interfaces.nsIDocShellTreeItem)
                         .rootTreeItem
                         .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                         .getInterface(Components.interfaces.nsIDOMWindow);
      mainWindow.getAttention();
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

function openServiceWindow(provider, contentWindow, url, name, options) {
  // resolve partial URLs and check prePath matches
  let uri;
  let fullURL;
  try {
    fullURL = contentWindow.document.documentURIObject.resolve(url);
    uri = Services.io.newURI(fullURL, null, null);
  } catch (ex) {
    Cu.reportError("openServiceWindow: failed to resolve window URL: " + url + "; " + ex);
    return null;
  }

  if (provider.origin != uri.prePath) {
    Cu.reportError("openServiceWindow: unable to load new location, " +
                   provider.origin + " != " + uri.prePath);
    return null;
  }

  function getChromeWindow(contentWin) {
    return contentWin.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIWebNavigation)
                     .QueryInterface(Ci.nsIDocShellTreeItem)
                     .rootTreeItem
                     .QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindow);

  }
  let chromeWindow = Services.ww.getWindowByName("social-service-window-" + name,
                                                 getChromeWindow(contentWindow));
  let tabbrowser = chromeWindow && chromeWindow.gBrowser;
  if (tabbrowser &&
      tabbrowser.selectedBrowser.getAttribute("origin") == provider.origin) {
    return tabbrowser.contentWindow;
  }

  let serviceWindow = contentWindow.openDialog(fullURL, name,
                                               "chrome=no,dialog=no" + options);

  // Get the newly opened window's containing XUL window
  chromeWindow = getChromeWindow(serviceWindow);

  // set the window's name and origin attribute on its browser, so that it can
  // be found via getWindowByName
  chromeWindow.name = "social-service-window-" + name;
  chromeWindow.gBrowser.selectedBrowser.setAttribute("origin", provider.origin);

  // we dont want the default title the browser produces, we'll fixup whenever
  // it changes.
  serviceWindow.addEventListener("DOMTitleChanged", function() {
    let sep = xulWindow.document.documentElement.getAttribute("titlemenuseparator");
    xulWindow.document.title = provider.name + sep + serviceWindow.document.title;
  });

  return serviceWindow;
}
