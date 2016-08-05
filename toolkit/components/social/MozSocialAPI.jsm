/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SocialService", "resource://gre/modules/SocialService.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Social", "resource:///modules/Social.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Chat", "resource:///modules/Chat.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils", "resource://gre/modules/PrivateBrowsingUtils.jsm");

this.EXPORTED_SYMBOLS = [
  "MozSocialAPI", "openChatWindow", "findChromeWindowForChats", "closeAllChatWindows",
  "hookWindowCloseForPanelClose"
];

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
      Services.obs.removeObserver(injectController, "document-element-inserted");
    }
  }
};

// Called on document-element-inserted, checks that the API should be injected,
// and then calls attachToWindow as appropriate
function injectController(doc, topic, data) {
  try {
    let window = doc.defaultView;
    if (!window || PrivateBrowsingUtils.isContentWindowPrivate(window))
      return;

    // Do not attempt to load the API into about: error pages
    if (doc.documentURIObject.scheme == "about") {
      return;
    }

    let containingBrowser = window.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIWebNavigation)
                                  .QueryInterface(Ci.nsIDocShell)
                                  .chromeEventHandler;
    // limit injecting into social panels or same-origin browser tabs if
    // social.debug.injectIntoTabs is enabled
    let allowTabs = false;
    try {
      allowTabs = containingBrowser.contentWindow == window &&
                  Services.prefs.getBoolPref("social.debug.injectIntoTabs");
    } catch (e) {}

    let origin = containingBrowser.getAttribute("origin");
    if (!allowTabs && !origin) {
      return;
    }

    // we always handle window.close on social content, even if they are not
    // "enabled".
    hookWindowCloseForPanelClose(window);

    SocialService.getProvider(doc.nodePrincipal.origin, function(provider) {
      if (provider && provider.enabled) {
        attachToWindow(provider, window);
      }
    });
  } catch (e) {
    Cu.reportError("MozSocialAPI injectController: unable to attachToWindow for " + doc.location + ": " + e);
  }
}

// Loads mozSocial support functions associated with provider into targetWindow
function attachToWindow(provider, targetWindow) {
  // If the loaded document isn't from the provider's origin (or a protocol
  // that inherits the principal), don't attach the mozSocial API.
  let targetDocURI = targetWindow.document.documentURIObject;
  if (!provider.isSameOrigin(targetDocURI)) {
    let msg = "MozSocialAPI: not attaching mozSocial API for " + provider.origin +
              " to " + targetDocURI.spec + " since origins differ."
    Services.console.logStringMessage(msg);
    return;
  }

  let mozSocialObj = {
    openChatWindow: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: function(toURL, callback) {
        let url = targetWindow.document.documentURIObject.resolve(toURL);
        let dwu = getChromeWindow(targetWindow)
          .QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIDOMWindowUtils);
        openChatWindow(targetWindow, provider, url, chatWin => {
          if (chatWin && dwu.isHandlingUserInput)
            chatWin.focus();
          if (callback)
            callback(!!chatWin);
        });
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
        if (!provider.isSameOrigin(url))
          return;
        chromeWindow.SocialFlyout.open(url, offset, callback);
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
    // allow a provider to share to other providers through the browser
    share: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: function(data) {
        let chromeWindow = getChromeWindow(targetWindow);
        if (!chromeWindow.SocialShare || chromeWindow.SocialShare.shareButton.hidden)
          throw new Error("Share is unavailable");
        // ensure user action initates the share
        let dwu = chromeWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDOMWindowUtils);
        if (!dwu.isHandlingUserInput)
          throw new Error("Attempt to share without user input");

        // limit to a few params we want to support for now
        let dataOut = {};
        for (let sub of ["url", "title", "description", "source"]) {
          dataOut[sub] = data[sub];
        }
        if (data.image)
          dataOut.previews = [data.image];

        chromeWindow.SocialShare.sharePage(null, dataOut);
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
}

function hookWindowCloseForPanelClose(targetWindow) {
  let _mozSocialDOMWindowClose;

  if ("messageManager" in targetWindow) {
    let _mozSocialSwapped;
    let mm = targetWindow.messageManager;
    mm.sendAsyncMessage("Social:HookWindowCloseForPanelClose");
    mm.addMessageListener("Social:DOMWindowClose", _mozSocialDOMWindowClose = function() {
      targetWindow.removeEventListener("SwapDocShells", _mozSocialSwapped);
      closePanel(targetWindow);
    });

    targetWindow.addEventListener("SwapDocShells", _mozSocialSwapped = function(ev) {
      targetWindow.removeEventListener("SwapDocShells", _mozSocialSwapped);

      targetWindow = ev.detail;
      targetWindow.messageManager.addMessageListener("Social:DOMWindowClose", _mozSocialDOMWindowClose);
    });
    return;
  }

  // We allow window.close() to close the panel, so add an event handler for
  // this, then cancel the event (so the window itself doesn't die) and
  // close the panel instead.
  // However, this is typically affected by the dom.allow_scripts_to_close_windows
  // preference, but we can avoid that check by setting a flag on the window.
  let dwu = targetWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindowUtils);
  dwu.allowScriptsToClose();

  targetWindow.addEventListener("DOMWindowClose", _mozSocialDOMWindowClose = function(evt) {
    let elt = targetWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIWebNavigation)
                .QueryInterface(Ci.nsIDocShell)
                .chromeEventHandler;
    closePanel(elt);
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

function closePanel(elt) {
  while (elt) {
    if (elt.localName == "panel") {
      elt.hidePopup();
      break;
    } else if (elt.localName == "chatbox") {
      elt.close();
      break;
    }
    elt = elt.parentNode;
  }
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

this.openChatWindow =
 function openChatWindow(contentWindow, provider, url, callback, mode) {
  let fullURI = provider.resolveUri(url);
  if (!provider.isSameOrigin(fullURI)) {
    Cu.reportError("Failed to open a social chat window - the requested URL is not the same origin as the provider.");
    return;
  }

  let chatbox = Chat.open(contentWindow, {
    origin: provider.origin,
    title: provider.name,
    url: fullURI.spec,
    mode: mode
  });
  if (callback) {
    chatbox.promiseChatLoaded.then(() => {
      callback(chatbox);
    });
  }
}

this.closeAllChatWindows = function closeAllChatWindows(provider) {
  return Chat.closeAll(provider.origin);
}
