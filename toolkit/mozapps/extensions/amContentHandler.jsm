/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const XPI_CONTENT_TYPE = "application/x-xpinstall";
const MSG_INSTALL_ADDON = "WebInstallerInstallAddonFromWebpage";

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

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
  handleContent(aMimetype, aContext, aRequest) {
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

    const {triggeringPrincipal} = aRequest.loadInfo;

    let sourceHost;

    try {
      sourceHost = triggeringPrincipal.URI.host;
    } catch (err) {
      // Ignore errors when retrieving the host for the principal (e.g. null principals raise
      // an NS_ERROR_FAILURE when principal.URI.host is accessed).
    }

    let install = {
      uri: uri.spec,
      hash: null,
      name: null,
      icon: null,
      mimetype: XPI_CONTENT_TYPE,
      triggeringPrincipal,
      callbackID: -1,
      method: "link",
      sourceHost,
    };

    if (Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
      // When running in the main process this might be a frame inside an
      // in-content UI page, walk up to find the first frame element in a chrome
      // privileged document
      let element = window.frameElement;
      while (element && !element.ownerDocument.nodePrincipal.isSystemPrincipal)
        element = element.ownerGlobal.frameElement;

      if (element) {
        let listener = Cc["@mozilla.org/addons/integration;1"].getService();
        listener.wrappedJSObject.receiveMessage({
          name: MSG_INSTALL_ADDON,
          target: element,
          data: install,
        });
        return;
      }
    }

    // Fall back to sending through the message manager
    let messageManager = window.docShell.messageManager;

    messageManager.sendAsyncMessage(MSG_INSTALL_ADDON, install);
  },

  classID: Components.ID("{7beb3ba8-6ec3-41b4-b67c-da89b8518922}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIContentHandler]),

  log(aMsg) {
    let msg = "amContentHandler.js: " + (aMsg.join ? aMsg.join("") : aMsg);
    Services.console.logStringMessage(msg);
    dump(msg + "\n");
  },
};

var EXPORTED_SYMBOLS = ["amContentHandler"];
