/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

function AppProtocolHandler() {
  this._appInfo = [];
  this._runningInParent = Cc["@mozilla.org/xre/runtime;1"]
                            .getService(Ci.nsIXULRuntime)
                            .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

AppProtocolHandler.prototype = {
  classID: Components.ID("{b7ad6144-d344-4687-b2d0-b6b9dce1f07f}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIProtocolHandler]),

  scheme: "app",
  defaultPort: -1,
  // Don't allow loading from other protocols, and only from app:// if webapps is granted
  protocolFlags: Ci.nsIProtocolHandler.URI_NOAUTH |
                 Ci.nsIProtocolHandler.URI_DANGEROUS_TO_LOAD |
                 Ci.nsIProtocolHandler.URI_CROSS_ORIGIN_NEEDS_WEBAPPS_PERM,

  getAppInfo: function app_phGetAppInfo(aId) {

    if (!this._appInfo[aId]) {
      let reply = cpmm.sendSyncMessage("Webapps:GetAppInfo", { id: aId });
      this._appInfo[aId] = reply[0];
    }
    return this._appInfo[aId];
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

    // Build a jar channel and masquerade as an app:// URI.
    let appInfo = this.getAppInfo(appId);
    let uri;
    if (this._runningInParent || appInfo.isCoreApp) {
      // In-parent and CoreApps can directly access files, so use jar:file://
      uri = "jar:file://" + appInfo.basePath + appId + "/application.zip!" + fileSpec;
    } else {
      // non-CoreApps in child need to ask parent for file handle, use jar:ipcfile://
      uri = "jar:remoteopenfile://" + appInfo.basePath + appId + "/application.zip!" + fileSpec;
    }
    let channel = Services.io.newChannel(uri, null, null);
    channel.QueryInterface(Ci.nsIJARChannel).setAppURI(aURI);
    channel.QueryInterface(Ci.nsIChannel).originalURI = aURI;

    return channel;
  },

  allowPort: function app_phAllowPort(aPort, aScheme) {
    return false;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AppProtocolHandler]);
