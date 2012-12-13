/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SocialService", "resource://gre/modules/SocialService.jsm");

this.EXPORTED_SYMBOLS = ["MozSocialAPI", "openChatWindow"];

this.MozSocialAPI = {
  _enabled: false,
  _everEnabled: false,
  set enabled(val) {
    let enable = !!val;
    if (enable == this._enabled) {
      return;
    }
    this._enabled = enable;

    if (enable) {
      Services.obs.addObserver(injectController, "document-element-inserted", false);

      if (!this._everEnabled) {
        this._everEnabled = true;
        Services.telemetry.getHistogramById("SOCIAL_ENABLED_ON_SESSION").add(true);
      }

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

    // Do not attempt to load the API into about: error pages
    if (doc.documentURIObject.scheme == "about") {
      return;
    }

    var containingBrowser = window.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIWebNavigation)
                                  .QueryInterface(Ci.nsIDocShell)
                                  .chromeEventHandler;

    let origin = containingBrowser.getAttribute("origin");
    if (!origin) {
      return;
    }

    SocialService.getProvider(origin, function(provider) {
      if (provider && provider.workerURL && provider.enabled) {
        attachToWindow(provider, window);
      }
    });
  } catch(e) {
    Cu.reportError("MozSocialAPI injectController: unable to attachToWindow for " + doc.location + ": " + e);
  }
}

// Loads mozSocial support functions associated with provider into targetWindow
function attachToWindow(provider, targetWindow) {
  // If the loaded document isn't from the provider's origin, don't attach
  // the mozSocial API.
  let origin = provider.origin;
  let targetDocURI = targetWindow.document.documentURIObject;
  if (provider.origin != targetDocURI.prePath) {
    let msg = "MozSocialAPI: not attaching mozSocial API for " + origin +
              " to " + targetDocURI.spec + " since origins differ."
    Services.console.logStringMessage(msg);
    return;
  }

  var port = provider.getWorkerPort(targetWindow);

  let mozSocialObj = {
    // Use a method for backwards compat with existing providers, but we
    // should deprecate this in favor of a simple .port getter.
    getWorker: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: function() {
        return {
          port: port,
          __exposedProps__: {
            port: "r"
          }
        };
      }
    },
    hasBeenIdleFor: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: function() {
        return false;
      }
    },
    openChatWindow: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: function(toURL, callback) {
        let url = targetWindow.document.documentURIObject.resolve(toURL);
        openChatWindow(getChromeWindow(targetWindow), provider, url, callback);
      }
    },
    openPanel: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: function(toURL, offset, callback) {
        let chromeWindow = getChromeWindow(targetWindow);
        if (!chromeWindow.SocialFlyout)
          return;
        let url = targetWindow.document.documentURIObject.resolve(toURL);
        let fullURL = ensureProviderOrigin(provider, url);
        if (!fullURL)
          return;
        chromeWindow.SocialFlyout.open(fullURL, offset, callback);
      }
    },
    closePanel: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: function(toURL, offset, callback) {
        let chromeWindow = getChromeWindow(targetWindow);
        if (!chromeWindow.SocialFlyout || !chromeWindow.SocialFlyout.panel)
          return;
        chromeWindow.SocialFlyout.panel.hidePopup();
      }
    },
    getAttention: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: function() {
        getChromeWindow(targetWindow).getAttention();
      }
    },
    isVisible: {
      enumerable: true,
      configurable: true,
      get: function() {
        return targetWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShell).isActive;
      }
    }
  };

  let contentObj = Cu.createObjectIn(targetWindow);
  Object.defineProperties(contentObj, mozSocialObj);
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

  // We allow window.close() to close the panel, so add an event handler for
  // this, then cancel the event (so the window itself doesn't die) and
  // close the panel instead.
  // However, this is typically affected by the dom.allow_scripts_to_close_windows
  // preference, but we can avoid that check by setting a flag on the window.
  let dwu = targetWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindowUtils);
  dwu.allowScriptsToClose();

  targetWindow.addEventListener("DOMWindowClose", function _mozSocialDOMWindowClose(evt) {
    let elt = targetWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIWebNavigation)
                .QueryInterface(Ci.nsIDocShell)
                .chromeEventHandler;
    while (elt) {
      if (elt.nodeName == "panel") {
        elt.hidePopup();
        break;
      } else if (elt.nodeName == "chatbox") {
        elt.close();
        break;
      }
      elt = elt.parentNode;
    }
    // preventDefault stops the default window.close() function being called,
    // which doesn't actually close anything but causes things to get into
    // a bad state (an internal 'closed' flag is set and debug builds start
    // asserting as the window is used.).
    // None of the windows we inject this API into are suitable for this
    // default close behaviour, so even if we took no action above, we avoid
    // the default close from doing anything.
    evt.preventDefault();
  }, true);
}

function schedule(callback) {
  Services.tm.mainThread.dispatch(callback, Ci.nsIThread.DISPATCH_NORMAL);
}

function getChromeWindow(contentWin) {
  return contentWin.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIWebNavigation)
                   .QueryInterface(Ci.nsIDocShellTreeItem)
                   .rootTreeItem
                   .QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindow);
}

function ensureProviderOrigin(provider, url) {
  // resolve partial URLs and check prePath matches
  let uri;
  let fullURL;
  try {
    fullURL = Services.io.newURI(provider.origin, null, null).resolve(url);
    uri = Services.io.newURI(fullURL, null, null);
  } catch (ex) {
    Cu.reportError("mozSocial: failed to resolve window URL: " + url + "; " + ex);
    return null;
  }

  if (provider.origin != uri.prePath) {
    Cu.reportError("mozSocial: unable to load new location, " +
                   provider.origin + " != " + uri.prePath);
    return null;
  }
  return fullURL;
}

function isWindowGoodForChats(win) {
  return win.SocialChatBar && win.SocialChatBar.isAvailable;
}

function findChromeWindowForChats(preferredWindow) {
  if (preferredWindow && isWindowGoodForChats(preferredWindow))
    return preferredWindow;
  // no good - so let's go hunting.  We are now looking for a navigator:browser
  // window that is suitable and already has chats open, or failing that,
  // any suitable navigator:browser window.
  let first, best, enumerator;
  // *sigh* - getZOrderDOMWindowEnumerator is broken everywhere other than
  // Windows.  We use BROKEN_WM_Z_ORDER as that is what the c++ code uses
  // and a few bugs recommend searching mxr for this symbol to identify the
  // workarounds - we want this code to be hit in such searches.
  const BROKEN_WM_Z_ORDER = Services.appinfo.OS != "WINNT";
  if (BROKEN_WM_Z_ORDER) {
    // this is oldest to newest and no way to change the order.
    enumerator = Services.wm.getEnumerator("navigator:browser");
  } else {
    // here we explicitly ask for bottom-to-top so we can use the same logic
    // where BROKEN_WM_Z_ORDER is true.
    enumerator = Services.wm.getZOrderDOMWindowEnumerator("navigator:browser", false);
  }
  while (enumerator.hasMoreElements()) {
    let win = enumerator.getNext();
    if (win && isWindowGoodForChats(win)) {
      first = win;
      if (win.SocialChatBar.hasChats)
        best = win;
    }
  }
  return best || first;
}

this.openChatWindow =
 function openChatWindow(chromeWindow, provider, url, callback, mode) {
  chromeWindow = findChromeWindowForChats(chromeWindow);
  if (!chromeWindow)
    return;
  let fullURL = ensureProviderOrigin(provider, url);
  if (!fullURL)
    return;
  chromeWindow.SocialChatBar.openChat(provider, fullURL, callback, mode);
  // getAttention is ignored if the target window is already foreground, so
  // we can call it unconditionally.
  chromeWindow.getAttention();
}
