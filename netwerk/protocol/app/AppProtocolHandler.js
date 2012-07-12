/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "cpmm", function() {
  return Cc["@mozilla.org/childprocessmessagemanager;1"]
         .getService(Ci.nsIFrameMessageManager)
         .QueryInterface(Ci.nsISyncMessageSender);
});

function AppProtocolHandler() {
  this._basePath = null;
}

AppProtocolHandler.prototype = {
  classID: Components.ID("{b7ad6144-d344-4687-b2d0-b6b9dce1f07f}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIProtocolHandler]),

  scheme: "app",
  defaultPort: -1,
  // Using the same flags as the JAR protocol handler.
  protocolFlags2: Ci.nsIProtocolHandler.URI_NORELATIVE |
                  Ci.nsIProtocolHandler.URI_NOAUTH |
                  Ci.nsIProtocolHandler.URI_LOADABLE_BY_ANYONE,

  get basePath() {
    if (!this._basePath) {
      this._basePath = cpmm.sendSyncMessage("Webapps:GetBasePath", { })[0] + "/";
    }

    return this._basePath;
  },

  newURI: function app_phNewURI(aSpec, aOriginCharset, aBaseURI) {
    let uri = Cc["@mozilla.org/network/standard-url;1"]
              .createInstance(Ci.nsIStandardURL);
    uri.init(Ci.nsIStandardURL.URLTYPE_STANDARD, -1, aSpec, aOriginCharset,
             aBaseURI);
    return uri.QueryInterface(Ci.nsIURI);
  },

  newChannel: function app_phNewChannel(aURI) {
    // We map app://ABCDEF/path/to/file.ext to
    // jar:file:///path/to/profile/webapps/ABCDEF/application.zip!/path/to/file.ext
    let noScheme = aURI.spec.substring(6);
    let firstSlash = noScheme.indexOf("/");

    let appId = noScheme;
    let fileSpec = aURI.path;

    if (firstSlash) {
      appId = noScheme.substring(0, firstSlash);
    }

    // Simulates default behavior of http servers:
    // Adds index.html if the file spec ends in / in /#anchor
    let lastSlash = fileSpec.lastIndexOf("/");
    if (lastSlash == fileSpec.length - 1) {
      fileSpec += "index.html";
    } else if (fileSpec[lastSlash + 1] == '#') {
      let anchor = fileSpec.substring(lastSlash + 1);
      fileSpec = fileSpec.substring(0, lastSlash) + "/index.html" + anchor;
    }

    // Build a jar channel and masquerade as an app:// URI.
    let uri = "jar:file://" + this.basePath + appId + "/application.zip!" + fileSpec;
    let channel = Services.io.newChannel(uri, null, null);
    channel.QueryInterface(Ci.nsIJARChannel).setAppURI(aURI);
    channel.QueryInterface(Ci.nsIChannel).originalURI = aURI;

    return channel;
  },

  allowPort: function app_phAllowPort(aPort, aScheme) {
    return false;
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([AppProtocolHandler]);
