/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://webapprt/modules/WebappRT.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyGetter(this, "gAppBrowser",
                            () => document.getElementById("content"));

#ifdef MOZ_CRASHREPORTER
XPCOMUtils.defineLazyServiceGetter(this, "gCrashReporter",
                                   "@mozilla.org/toolkit/crash-reporter;1",
                                   "nsICrashReporter");
#endif

var progressListener = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),
  onLocationChange: function onLocationChange(progress, request, location,
                                              flags) {

    // Close tooltip (code adapted from /browser/base/content/browser.js)
    let pageTooltip = document.getElementById("contentAreaTooltip");
    let tooltipNode = pageTooltip.triggerNode;
    if (tooltipNode) {
      // Optimise for the common case
      if (progress.isTopLevel) {
        pageTooltip.hidePopup();
      }
      else {
        for (let tooltipWindow = tooltipNode.ownerDocument.defaultView;
             tooltipWindow != tooltipWindow.parent;
             tooltipWindow = tooltipWindow.parent) {
          if (tooltipWindow == progress.DOMWindow) {
            pageTooltip.hidePopup();
            break;
          }
        }
      }
    }

    let isSameOrigin = (location.prePath === WebappRT.config.app.origin);

    // Set the title of the window to the name of the webapp, adding the origin
    // of the page being loaded if it's from a different origin than the app
    // (per security bug 741955, which specifies that other-origin pages loaded
    // in runtime windows must be identified in chrome).
    let title = WebappRT.localeManifest.name;
    if (!isSameOrigin) {
      title = location.prePath + " - " + title;
    }
    document.documentElement.setAttribute("title", title);

#ifndef XP_WIN
#ifndef XP_MACOSX
    if (isSameOrigin) {
      // On non-Windows platforms, we open new windows in fullscreen mode
      // if the opener window is in fullscreen mode, so we hide the menubar;
      // but on Mac we don't need to hide the menubar.
      if (document.mozFullScreenElement) {
        document.getElementById("main-menubar").style.display = "none";
      }
    }
#endif
#endif
  },

  onStateChange: function onStateChange(aProgress, aRequest, aFlags, aStatus) {
    if (aRequest instanceof Ci.nsIChannel &&
        aFlags & Ci.nsIWebProgressListener.STATE_START &&
        aFlags & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT) {
      updateCrashReportURL(aRequest.URI);
    }
  }
};

function onOpenWindow(event) {
  if (event.detail.name === "_blank") {
    let uri = Services.io.newURI(event.detail.url, null, null);

    // Prevent the default handler so nsContentTreeOwner.ProvideWindow
    // doesn't create the window itself.
    event.preventDefault();

    // Direct the URL to the browser.
    Cc["@mozilla.org/uriloader/external-protocol-service;1"].
    getService(Ci.nsIExternalProtocolService).
    getProtocolHandlerInfo(uri.scheme).
    launchWithURI(uri);
  }

  // Otherwise, don't do anything to make nsContentTreeOwner.ProvideWindow
  // create the window itself and return it to the window.open caller.
}

function onDOMContentLoaded() {
  window.removeEventListener("DOMContentLoaded", onDOMContentLoaded, false);
  // The initial window's app ID is set by Startup.jsm before the app
  // is loaded, so this code only handles subsequent windows that are opened
  // by the app via window.open calls.  We do this on DOMContentLoaded
  // in order to ensure it gets set before the window's content is loaded.
  if (gAppBrowser.docShell.appId === Ci.nsIScriptSecurityManager.NO_APP_ID) {
    // Set the principal to the correct app ID.  Since this is a subsequent
    // window, we know that WebappRT.configPromise has been resolved, so we
    // don't have to yield to it before accessing WebappRT.appID.
    gAppBrowser.docShell.setIsApp(WebappRT.appID);
  }
}
window.addEventListener("DOMContentLoaded", onDOMContentLoaded, false);

function onLoad() {
  window.removeEventListener("load", onLoad, false);

  gAppBrowser.addProgressListener(progressListener,
                                  Ci.nsIWebProgress.NOTIFY_LOCATION |
                                  Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT);

  updateMenuItems();

  gAppBrowser.addEventListener("mozbrowseropenwindow", onOpenWindow);
}
window.addEventListener("load", onLoad, false);

function onUnload() {
  gAppBrowser.removeProgressListener(progressListener);
  gAppBrowser.removeEventListener("mozbrowseropenwindow", onOpenWindow);
}
window.addEventListener("unload", onUnload, false);

// Fullscreen handling.

#ifndef XP_MACOSX
document.addEventListener('mozfullscreenchange', function() {
  if (document.mozFullScreenElement) {
    document.getElementById("main-menubar").style.display = "none";
  } else {
    document.getElementById("main-menubar").style.display = "";
  }
}, false);
#endif

// On Mac, we dynamically create the label for the Quit menuitem, using
// a string property to inject the name of the webapp into it.
var updateMenuItems = Task.async(function*() {
#ifdef XP_MACOSX
  yield WebappRT.configPromise;

  let manifest = WebappRT.localeManifest;
  let bundle =
    Services.strings.createBundle("chrome://webapprt/locale/webapp.properties");
  let quitLabel = bundle.formatStringFromName("quitApplicationCmdMac.label",
                                              [manifest.name], 1);
  let hideLabel = bundle.formatStringFromName("hideApplicationCmdMac.label",
                                              [manifest.name], 1);
  document.getElementById("menu_FileQuitItem").setAttribute("label", quitLabel);
  document.getElementById("menu_mac_hide_app").setAttribute("label", hideLabel);
#endif
});

#ifndef XP_MACOSX
var gEditUIVisible = true;
#endif

function updateEditUIVisibility() {
#ifndef XP_MACOSX
  let editMenuPopupState = document.getElementById("menu_EditPopup").state;
  let contextMenuPopupState = document.getElementById("contentAreaContextMenu").state;

  // The UI is visible if the Edit menu is opening or open, if the context menu
  // is open, or if the toolbar has been customized to include the Cut, Copy,
  // or Paste toolbar buttons.
  gEditUIVisible = editMenuPopupState == "showing" ||
                   editMenuPopupState == "open" ||
                   contextMenuPopupState == "showing" ||
                   contextMenuPopupState == "open";

  // If UI is visible, update the edit commands' enabled state to reflect
  // whether or not they are actually enabled for the current focus/selection.
  if (gEditUIVisible) {
    goUpdateGlobalEditMenuItems();
  }

  // Otherwise, enable all commands, so that keyboard shortcuts still work,
  // then lazily determine their actual enabled state when the user presses
  // a keyboard shortcut.
  else {
    goSetCommandEnabled("cmd_undo", true);
    goSetCommandEnabled("cmd_redo", true);
    goSetCommandEnabled("cmd_cut", true);
    goSetCommandEnabled("cmd_copy", true);
    goSetCommandEnabled("cmd_paste", true);
    goSetCommandEnabled("cmd_selectAll", true);
    goSetCommandEnabled("cmd_delete", true);
    goSetCommandEnabled("cmd_switchTextDirection", true);
  }
#endif
}

function updateCrashReportURL(aURI) {
#ifdef MOZ_CRASHREPORTER
  if (!gCrashReporter.enabled)
    return;

  let uri = aURI.clone();
  // uri.userPass throws on protocols without the concept of authentication,
  // like about:, which tests can load, so we catch and ignore an exception.
  try {
    if (uri.userPass != "") {
      uri.userPass = "";
    }
  } catch (e) {}

  gCrashReporter.annotateCrashReport("URL", uri.spec);
#endif
}

// Context menu handling code.
// At the moment there isn't any built-in menu, we only support HTML5 custom
// menus.

var gContextMenu = null;

XPCOMUtils.defineLazyGetter(this, "PageMenu", function() {
  let tmp = {};
  Cu.import("resource://gre/modules/PageMenu.jsm", tmp);
  return new tmp.PageMenu();
});

function showContextMenu(aEvent, aXULMenu) {
  if (aEvent.target != aXULMenu) {
    return true;
  }

  gContextMenu = new nsContextMenu(aXULMenu);
  if (gContextMenu.shouldDisplay) {
    updateEditUIVisibility();
  }

  return gContextMenu.shouldDisplay;
}

function hideContextMenu(aEvent, aXULMenu) {
  if (aEvent.target != aXULMenu) {
    return;
  }

  gContextMenu = null;

  updateEditUIVisibility();
}

function nsContextMenu(aXULMenu) {
  this.initMenu(aXULMenu);
}

nsContextMenu.prototype = {
  initMenu: function(aXULMenu) {
    this.hasPageMenu = PageMenu.maybeBuildAndAttachMenu(document.popupNode,
                                                        aXULMenu);
    this.shouldDisplay = this.hasPageMenu;
  },
};
