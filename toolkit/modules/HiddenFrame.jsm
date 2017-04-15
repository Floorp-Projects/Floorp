/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["HiddenFrame"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/PromiseUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const XUL_PAGE = "data:application/vnd.mozilla.xul+xml;charset=utf-8,<window%20id='win'/>";

const gAllHiddenFrames = new WeakSet();

let cleanupRegistered = false;
function ensureCleanupRegistered() {
  if (!cleanupRegistered) {
    cleanupRegistered = true;
    Services.obs.addObserver(function() {
      for (let hiddenFrame of ChromeUtils.nondeterministicGetWeakSetKeys(gAllHiddenFrames)) {
        hiddenFrame.destroy();
      }
    }, "xpcom-shutdown");
  }
}


/**
 * An hidden frame object. It takes care of creating a windowless browser and
 * passing the window containing a blank XUL <window> back.
 */
function HiddenFrame() {
}

HiddenFrame.prototype = {
  _frame: null,
  _browser: null,
  _listener: null,
  _webProgress: null,
  _deferred: null,

  /**
   * Gets the |contentWindow| of the hidden frame. Creates the frame if needed.
   * @returns Promise Returns a promise which is resolved when the hidden frame has finished
   *          loading.
   */
  get() {
    if (!this._deferred) {
      this._deferred = PromiseUtils.defer();
      this._create();
    }

    return this._deferred.promise;
  },

  /**
   * Fetch a sync ref to the window inside the frame (needed for the add-on SDK).
   */
  getWindow() {
    this.get();
    this._browser.QueryInterface(Ci.nsIInterfaceRequestor);
    return this._browser.getInterface(Ci.nsIDOMWindow);
  },


  destroy() {
    if (this._browser) {
      if (this._listener) {
        this._webProgress.removeProgressListener(this._listener);
        this._listener = null;
        this._webProgress = null;
      }
      this._frame = null;
      this._deferred = null;

      gAllHiddenFrames.delete(this);
      this._browser.close();
      this._browser = null;
    }
  },

  _create() {
    ensureCleanupRegistered();
    this._browser = Services.appShell.createWindowlessBrowser(true);
    this._browser.QueryInterface(Ci.nsIInterfaceRequestor);
    gAllHiddenFrames.add(this);
    this._webProgress = this._browser.getInterface(Ci.nsIWebProgress);
    this._listener = {
      QueryInterface: XPCOMUtils.generateQI([
        Ci.nsIWebProgressListener, Ci.nsIWebProgressListener2,
        Ci.nsISupportsWeakReference]),
    };
    this._listener.onStateChange = (wbp, request, stateFlags, status) => {
      if (!request) {
        return;
      }
      if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        this._webProgress.removeProgressListener(this._listener);
        this._listener = null;
        this._webProgress = null;
        // Get the window reference via the document.
        this._frame = this._browser.document.ownerGlobal;
        this._deferred.resolve(this._frame);
      }
    };
    this._webProgress.addProgressListener(this._listener, Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT);
    let docShell = this._browser.getInterface(Ci.nsIDocShell);
    docShell.createAboutBlankContentViewer(Services.scriptSecurityManager.getSystemPrincipal());
    docShell.useGlobalHistory = false;
    this._browser.loadURI(XUL_PAGE, 0, null, null, null);
  }
};
