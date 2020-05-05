/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

function nsWebHandlerApp() {}

nsWebHandlerApp.prototype = {
  classDescription: "A web handler for protocols and content",
  classID: Components.ID("8b1ae382-51a9-4972-b930-56977a57919d"),
  contractID: "@mozilla.org/uriloader/web-handler-app;1",
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIWebHandlerApp,
    Ci.nsIHandlerApp,
  ]),

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

  launchWithURI(aURI, aWindowContext) {
    // XXX need to strip passwd & username from URI to handle, as per the
    // WhatWG HTML5 draft.  nsSimpleURL, which is what we're going to get,
    // can't do this directly.  Ideally, we'd fix nsStandardURL to make it
    // possible to turn off all of its quirks handling, and use that...

    // encode the URI to be handled
    var escapedUriSpecToHandle = encodeURIComponent(aURI.spec);

    // insert the encoded URI and create the object version.
    var uriSpecToSend = this.uriTemplate.replace("%s", escapedUriSpecToHandle);
    var uriToSend = Services.io.newURI(uriSpecToSend);

    let policy = WebExtensionPolicy.getByURI(uriToSend);
    let privateAllowed = !policy || policy.privateBrowsingAllowed;

    // if we have a window context, use the URI loader to load there
    if (aWindowContext) {
      try {
        let remoteWindow = aWindowContext.getInterface(
          Ci.nsIRemoteWindowContext
        );
        if (remoteWindow.usePrivateBrowsing && !privateAllowed) {
          throw Components.Exception(
            "Extension not allowed in private windows.",
            Cr.NS_ERROR_FILE_NOT_FOUND
          );
        }
        // getInterface throws if the object doesn't implement the given
        // interface, so this try/catch statement is more of an if.
        // If aWindowContext refers to a remote docshell, send the load
        // request to the correct process.
        remoteWindow.openURI(uriToSend);
        return;
      } catch (e) {
        if (e.result != Cr.NS_NOINTERFACE) {
          throw e;
        }
      }

      try {
        let isPrivate = aWindowContext
          .getInterface(Ci.nsIDocShell)
          .QueryInterface(Ci.nsILoadContext).usePrivateBrowsing;
        if (isPrivate && !privateAllowed) {
          throw Components.Exception(
            "Extension not allowed in private windows.",
            Cr.NS_ERROR_FILE_NOT_FOUND
          );
        }
      } catch (e) {
        if (e.result != Cr.NS_NOINTERFACE) {
          throw e;
        }
      }

      // create a channel from this URI
      var channel = NetUtil.newChannel({
        uri: uriToSend,
        loadUsingSystemPrincipal: true,
      });
      channel.loadFlags = Ci.nsIChannel.LOAD_DOCUMENT_URI;

      // load the channel
      var uriLoader = Cc["@mozilla.org/uriloader;1"].getService(
        Ci.nsIURILoader
      );

      // XXX ideally, whether to pass the IS_CONTENT_PREFERRED flag should be
      // passed in from above.  Practically, the flag is probably a reasonable
      // default since browsers don't care much, and link click is likely to be
      // the more interesting case for non-browser apps.  See
      // <https://bugzilla.mozilla.org/show_bug.cgi?id=392957#c9> for details.
      uriLoader.openURI(
        channel,
        Ci.nsIURILoader.IS_CONTENT_PREFERRED,
        aWindowContext
      );
      return;
    }

    let win = Services.wm.getMostRecentWindow("navigator:browser");

    // If this is an extension handler, check private browsing access.
    if (!privateAllowed && PrivateBrowsingUtils.isContentWindowPrivate(win)) {
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
