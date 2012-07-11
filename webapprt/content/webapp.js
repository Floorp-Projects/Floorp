/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://webapprt/modules/WebappRT.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "gAppBrowser",
                            function() document.getElementById("content"));

let progressListener = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),
  onLocationChange: function onLocationChange(progress, request, location,
                                              flags) {
    // Set the title of the window to the name of the webapp, adding the origin
    // of the page being loaded if it's from a different origin than the app
    // (per security bug 741955, which specifies that other-origin pages loaded
    // in runtime windows must be identified in chrome).
    let title = WebappRT.config.app.manifest.name;
    let origin = location.prePath;
    if (origin != WebappRT.config.app.origin) {
      title = origin + " - " + title;
    }
    document.documentElement.setAttribute("title", title);
  }
};

function onLoad() {
  window.removeEventListener("load", onLoad, false);

  let cmdLineArgs = window.arguments && window.arguments[0] ?
                    window.arguments[0].QueryInterface(Ci.nsIPropertyBag2) :
                    null;

  // In test mode, listen for test app installations and load the -test-mode URL
  // if present.
  if (cmdLineArgs && cmdLineArgs.hasKey("test-mode")) {
    // This observer is only present until the first app gets installed.
    // It adds the progress listener, which can't happen until then because
    // the progress listener needs to access the app manifest, which isn't
    // available beforehand.
    Services.obs.addObserver(function observeOnce(subj, topic, data) {
      Services.obs.removeObserver(observeOnce, "webapprt-test-did-install");
      gAppBrowser.webProgress.
        addProgressListener(progressListener,Ci.nsIWebProgress.NOTIFY_LOCATION);
    }, "webapprt-test-did-install", false);

    // This observer is present for the lifetime of the runtime.
    Services.obs.addObserver(function observe(subj, topic, data) {
      initWindow(false);
    }, "webapprt-test-did-install", false);

    let testURL = cmdLineArgs.get("test-mode");
    if (testURL) {
      gAppBrowser.loadURI(testURL);
    }

    return;
  }

  gAppBrowser.webProgress.
    addProgressListener(progressListener, Ci.nsIWebProgress.NOTIFY_LOCATION);

  initWindow(!!cmdLineArgs);
}
window.addEventListener("load", onLoad, false);

function onUnload() {
  gAppBrowser.removeProgressListener(progressListener);
}
window.addEventListener("unload", onUnload, false);

function initWindow(isMainWindow) {
  let manifest = WebappRT.config.app.manifest;

  updateMenuItems();

  // Listen for clicks to redirect <a target="_blank"> to the browser.
  // This doesn't capture clicks so content can capture them itself and do
  // something different if it doesn't want the default behavior.
  gAppBrowser.addEventListener("click", onContentClick, false, true);

  // Only load the webapp on the initially launched main window
  if (isMainWindow) {
    // Load the webapp's launch URL
    let installRecord = WebappRT.config.app;
    let url = Services.io.newURI(installRecord.origin, null, null);
    if (manifest.launch_path)
      url = Services.io.newURI(manifest.launch_path, null, url);
    gAppBrowser.setAttribute("src", url.spec);
  }
}

/**
 * Direct a click on <a target="_blank"> to the user's default browser.
 *
 * In the long run, it might be cleaner to move this to an extension of
 * nsIWebBrowserChrome3::onBeforeLinkTraversal.
 *
 * @param {DOMEvent} event the DOM event
 **/
function onContentClick(event) {
  let target = event.target;

  if (!(target instanceof HTMLAnchorElement) ||
      target.getAttribute("target") != "_blank") {
    return;
  }

  let uri = Services.io.newURI(target.href,
                               target.ownerDocument.characterSet,
                               null);

  // Direct the URL to the browser.
  Cc["@mozilla.org/uriloader/external-protocol-service;1"].
    getService(Ci.nsIExternalProtocolService).
    getProtocolHandlerInfo(uri.scheme).
    launchWithURI(uri);

  // Prevent the runtime from loading the URL.  We do this after directing it
  // to the browser to give the runtime a shot at handling the URL if we fail
  // to direct it to the browser for some reason.
  event.preventDefault();
}

// On Mac, we dynamically create the label for the Quit menuitem, using
// a string property to inject the name of the webapp into it.
function updateMenuItems() {
#ifdef XP_MACOSX
  let installRecord = WebappRT.config.app;
  let manifest = WebappRT.config.app.manifest;
  let bundle =
    Services.strings.createBundle("chrome://webapprt/locale/webapp.properties");
  let quitLabel = bundle.formatStringFromName("quitApplicationCmdMac.label",
                                              [manifest.name], 1);
  let hideLabel = bundle.formatStringFromName("hideApplicationCmdMac.label",
                                              [manifest.name], 1);
  document.getElementById("menu_FileQuitItem").setAttribute("label", quitLabel);
  document.getElementById("menu_mac_hide_app").setAttribute("label", hideLabel);
#endif
}

function updateEditUIVisibility() {
#ifndef XP_MACOSX
  let editMenuPopupState = document.getElementById("menu_EditPopup").state;

  // The UI is visible if the Edit menu is opening or open, if the context menu
  // is open, or if the toolbar has been customized to include the Cut, Copy,
  // or Paste toolbar buttons.
  gEditUIVisible = editMenuPopupState == "showing" ||
                   editMenuPopupState == "open";

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
