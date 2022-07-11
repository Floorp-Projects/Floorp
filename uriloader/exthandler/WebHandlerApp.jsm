/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

function nsWebHandlerApp() {}

nsWebHandlerApp.prototype = {
  classDescription: "A web handler for protocols and content",
  classID: Components.ID("8b1ae382-51a9-4972-b930-56977a57919d"),
  contractID: "@mozilla.org/uriloader/web-handler-app;1",
  QueryInterface: ChromeUtils.generateQI(["nsIWebHandlerApp", "nsIHandlerApp"]),

  _name: null,
  _detailedDescription: null,
  _uriTemplate: null,

  // nsIHandlerApp

  get name() {
    return this._name;
  },

  set name(aName) {
    this._name = aName;
  },

  get detailedDescription() {
    return this._detailedDescription;
  },

  set detailedDescription(aDesc) {
    this._detailedDescription = aDesc;
  },

  equals(aHandlerApp) {
    if (!aHandlerApp) {
      throw Components.Exception("", Cr.NS_ERROR_NULL_POINTER);
    }

    if (
      aHandlerApp instanceof Ci.nsIWebHandlerApp &&
      aHandlerApp.uriTemplate &&
      this.uriTemplate &&
      aHandlerApp.uriTemplate == this.uriTemplate
    ) {
      return true;
    }
    return false;
  },

  launchWithURI(aURI, aBrowsingContext) {
    // XXX need to strip passwd & username from URI to handle, as per the
    // WhatWG HTML5 draft.  nsSimpleURL, which is what we're going to get,
    // can't do this directly.  Ideally, we'd fix nsStandardURL to make it
    // possible to turn off all of its quirks handling, and use that...

    let { scheme } = aURI;
    if (scheme == "ftp" || scheme == "ftps" || scheme == "sftp") {
      // FTP URLs are parsed by nsStandardURL, so clearing the username and
      // password does not throw.
      aURI = aURI
        .mutate()
        .setUserPass("")
        .finalize();
    }

    // encode the URI to be handled
    var escapedUriSpecToHandle = encodeURIComponent(aURI.spec);

    // insert the encoded URI and create the object version.
    var uriSpecToSend = this.uriTemplate.replace("%s", escapedUriSpecToHandle);
    var uriToSend = Services.io.newURI(uriSpecToSend);

    let policy = WebExtensionPolicy.getByURI(uriToSend);
    let privateAllowed = !policy || policy.privateBrowsingAllowed;

    // If we're in a frame, check if we're a built-in scheme, in which case,
    // override the target browsingcontext. It's not a good idea to try to
    // load mail clients or other apps with potential for logged in data into
    // iframes, and in any case it's unlikely to work due to framing
    // restrictions employed by the target site.
    if (aBrowsingContext && aBrowsingContext != aBrowsingContext.top) {
      if (!scheme.startsWith("web+") && !scheme.startsWith("ext+")) {
        aBrowsingContext = null;
      }
    }

    // if we have a context, use the URI loader to load there
    if (aBrowsingContext) {
      if (aBrowsingContext.usePrivateBrowsing && !privateAllowed) {
        throw Components.Exception(
          "Extension not allowed in private windows.",
          Cr.NS_ERROR_FILE_NOT_FOUND
        );
      }

      let triggeringPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
      Services.tm.dispatchToMainThread(() =>
        aBrowsingContext.loadURI(uriSpecToSend, { triggeringPrincipal })
      );
      return;
    }

    let win = Services.wm.getMostRecentWindow("navigator:browser");

    // If this is an extension handler, check private browsing access.
    if (!privateAllowed && lazy.PrivateBrowsingUtils.isWindowPrivate(win)) {
      throw Components.Exception(
        "Extension not allowed in private windows.",
        Cr.NS_ERROR_FILE_NOT_FOUND
      );
    }

    // If we get an exception, there are several possible reasons why:
    // a) this gecko embedding doesn't provide an nsIBrowserDOMWindow
    //    implementation (i.e. doesn't support browser-style functionality),
    //    so we need to kick the URL out to the OS default browser.  This is
    //    the subject of bug 394479.
    // b) this embedding does provide an nsIBrowserDOMWindow impl, but
    //    there doesn't happen to be a browser window open at the moment; one
    //    should be opened.  It's not clear whether this situation will really
    //    ever occur in real life.  If it does, the only API that I can find
    //    that seems reasonably likely to work for most embedders is the
    //    command line handler.
    // c) something else went wrong
    //
    // It's not clear how one would differentiate between the three cases
    // above, so for now we don't catch the exception.

    // openURI
    win.browserDOMWindow.openURI(
      uriToSend,
      null, // no window.opener
      Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW,
      Ci.nsIBrowserDOMWindow.OPEN_NEW,
      Services.scriptSecurityManager.getSystemPrincipal()
    );
  },

  // nsIWebHandlerApp

  get uriTemplate() {
    return this._uriTemplate;
  },

  set uriTemplate(aURITemplate) {
    this._uriTemplate = aURITemplate;
  },
};

var EXPORTED_SYMBOLS = ["nsWebHandlerApp"];
