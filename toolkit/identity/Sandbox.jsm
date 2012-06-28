/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["Sandbox"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const PREF_DEBUG = "toolkit.identity.debug";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

/**
 * An object that represents a sandbox in an iframe loaded with aURL. The
 * callback provided to the constructor will be invoked when the sandbox is
 * ready to be used. The callback will receive this object as its only argument.
 *
 * You must call free() when you are finished with the sandbox to explicitly
 * free up all associated resources.
 *
 * @param aURL
 *        (string) URL to load in the sandbox.
 *
 * @param aCallback
 *        (function) Callback to be invoked with a Sandbox, when ready.
 */
function Sandbox(aURL, aCallback) {
  // Normalize the URL so the comparison in _makeSandboxContentLoaded works
  this._url = Services.io.newURI(aURL, null, null).spec;
  this._debug = Services.prefs.getBoolPref(PREF_DEBUG);
  this._log("Creating sandbox for: " + this._url);
  this._createFrame();
  this._createSandbox(aCallback);
}

Sandbox.prototype = {

  /**
   * Use the outer window ID as the identifier of the sandbox.
   */
  get id() {
    return this._frame.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
  },

  /**
   * Reload the URL in the sandbox. This is useful to reuse a Sandbox (same
   * id and URL).
   */
  reload: function Sandbox_reload(aCallback) {
    this._log("reload: " + this.id + " : " + this._url);
    this._createSandbox(function createdSandbox(aSandbox) {
      this._log("reloaded sandbox id: ", aSandbox.id);
      aCallback(aSandbox);
    }.bind(this));
  },

  /**
   * Frees the sandbox and releases the iframe created to host it.
   */
  free: function Sandbox_free() {
    this._log("free: " + this.id);
    this._container.removeChild(this._frame);
    this._frame = null;
    this._container = null;
    this._url = null;
  },

  /**
   * Creates an empty, hidden iframe and sets it to the _frame
   * property of this object.
   */
  _createFrame: function Sandbox__createFrame() {
    let hiddenWindow = Services.appShell.hiddenDOMWindow;
    let doc = hiddenWindow.document;

    // Insert iframe in to create docshell.
    let frame = doc.createElementNS(XHTML_NS, "iframe");
    frame.setAttribute("mozbrowser", true);
    frame.setAttribute("mozframetype", "content");
    frame.style.visibility = "collapse";
    doc.documentElement.appendChild(frame);

    let docShell = frame.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                      .getInterface(Ci.nsIWebNavigation)
                                      .QueryInterface(Ci.nsIInterfaceRequestor)
                                      .getInterface(Ci.nsIDocShell);

    // Mark this docShell as a "browserFrame", to break script access to e.g. window.top
    docShell.isBrowserFrame = true;

    // Stop about:blank from being loaded.
    docShell.stop(Ci.nsIWebNavigation.STOP_NETWORK);

    // Disable some types of content
    docShell.allowAuth = false;
    docShell.allowPlugins = false;
    docShell.allowImages = false;
    docShell.allowWindowControl = false;
    // TODO: disable media (bug 759964)

    // Disable stylesheet loading since the document is not visible.
    let markupDocViewer = docShell.contentViewer
                                  .QueryInterface(Ci.nsIMarkupDocumentViewer);
    markupDocViewer.authorStyleDisabled = true;

    // Set instance properties.
    this._frame = frame;
    this._container = doc.documentElement;
  },

  _createSandbox: function Sandbox__createSandbox(aCallback) {
    let self = this;
    function _makeSandboxContentLoaded(event) {
      self._log("_makeSandboxContentLoaded : " + self.id + " : "
                + event.target.location.toString());
      if (event.target != self._frame.contentDocument) {
        return;
      }
      self._frame.removeEventListener(
        "DOMWindowCreated", _makeSandboxContentLoaded, true
      );

      aCallback(self);
    };

    this._frame.addEventListener("DOMWindowCreated",
                                 _makeSandboxContentLoaded,
                                 true);

    // Load the iframe.
    let webNav = this._frame.contentWindow
                            .QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebNavigation);

    webNav.loadURI(
      this._url,
      Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE,
      null, // referrer
      null, // postData
      null  // headers
    );

  },

  _log: function Sandbox__log(...aMessageArgs) {
    if (!this._debug) {
      return;
    }
    dump("Sandbox: " + aMessageArgs.join(" : ") + "\n");
  },

};
