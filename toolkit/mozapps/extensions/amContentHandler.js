/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const XPI_CONTENT_TYPE = "application/x-xpinstall";
const MSG_INSTALL_ADDONS = "WebInstallerInstallAddonsFromWebpage";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

function amContentHandler() {
}

amContentHandler.prototype = {
  /**
   * Handles a new request for an application/x-xpinstall file.
   *
   * @param  aMimetype
   *         The mimetype of the file
   * @param  aContext
   *         The context passed to nsIChannel.asyncOpen
   * @param  aRequest
   *         The nsIRequest dealing with the content
   */
  handleContent: function(aMimetype, aContext, aRequest) {
    if (aMimetype != XPI_CONTENT_TYPE)
      throw Cr.NS_ERROR_WONT_HANDLE_CONTENT;

    if (!(aRequest instanceof Ci.nsIChannel))
      throw Cr.NS_ERROR_WONT_HANDLE_CONTENT;

    let uri = aRequest.URI;

    let window = null;
    let callbacks = aRequest.notificationCallbacks ?
                    aRequest.notificationCallbacks :
                    aRequest.loadGroup.notificationCallbacks;
    if (callbacks)
      window = callbacks.getInterface(Ci.nsIDOMWindow);

    aRequest.cancel(Cr.NS_BINDING_ABORTED);

    let principalToInherit = aRequest.loadInfo.principalToInherit;
    if (!principalToInherit) {
      principalToInherit = aRequest.loadInfo.triggeringPrincipal;
    }

    let installs = {
      uris: [uri.spec],
      hashes: [null],
      names: [null],
      icons: [null],
      mimetype: XPI_CONTENT_TYPE,
      principalToInherit: principalToInherit,
      callbackID: -1
    };

    if (Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
      // When running in the main process this might be a frame inside an
      // in-content UI page, walk up to find the first frame element in a chrome
      // privileged document
      let element = window.frameElement;
      let ssm = Services.scriptSecurityManager;
      while (element && !ssm.isSystemPrincipal(element.ownerDocument.nodePrincipal))
        element = element.ownerDocument.defaultView.frameElement;

      if (element) {
        let listener = Cc["@mozilla.org/addons/integration;1"].
                       getService(Ci.nsIMessageListener);
        listener.wrappedJSObject.receiveMessage({
          name: MSG_INSTALL_ADDONS,
          target: element,
          data: installs,
        });
        return;
      }
    }

    // Fall back to sending through the message manager
    let messageManager = window.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIDocShell)
                               .QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIContentFrameMessageManager);

    messageManager.sendAsyncMessage(MSG_INSTALL_ADDONS, installs);
  },

  classID: Components.ID("{7beb3ba8-6ec3-41b4-b67c-da89b8518922}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentHandler]),

  log : function(aMsg) {
    let msg = "amContentHandler.js: " + (aMsg.join ? aMsg.join("") : aMsg);
    Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).
      logStringMessage(msg);
    dump(msg + "\n");
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([amContentHandler]);
