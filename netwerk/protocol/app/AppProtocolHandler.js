/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

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
      this._appInfo[aId] = appsService.getAppInfo(aId);
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
    let url = aURI.QueryInterface(Ci.nsIURL);
    let appId = aURI.host;
    let fileSpec = url.filePath;

    // Build a jar channel and masquerade as an app:// URI.
    let appInfo = this.getAppInfo(appId);
    if (!appInfo) {
      // That should not happen, so dump() inconditionnally.
      // We create a dummy channel instead of throwing to let the
      // downstream user get a 404 error.
      dump("!! got no appInfo for " + appId + "\n");
      return new DummyChannel();
    }

    let uri;
    if (this._runningInParent || appInfo.isCoreApp) {
      // In-parent and CoreApps can directly access files, so use jar:file://
      uri = "jar:file://" + appInfo.path + "/application.zip!" + fileSpec;
    } else {
      // non-CoreApps in child need to ask parent for file handle, use jar:ipcfile://
      uri = "jar:remoteopenfile://" + appInfo.path + "/application.zip!" + fileSpec;
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

/**
  * This dummy channel implementation only provides enough functionality
  * to return a fake 404 error when the caller asks for an app:// URL
  * containing an unknown appId.
  */
function DummyChannel() {
  this.originalURI = Services.io.newURI("app://unknown/nothing.html", null, null);
  this.URI = Services.io.newURI("app://unknown/nothing.html", null, null);
}

DummyChannel.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRequest,
                                         Ci.nsIChannel,
                                         Ci.nsIJARChannel]),

  // nsIRequest
  name: "dummy_app_channel",

  isPending: function dc_isPending() {
    return this._pending;
  },

  status: Cr.NS_ERROR_FILE_NOT_FOUND,

  cancel: function dc_cancel() {
  },

  suspend: function dc_suspend() {
    this._suspendCount++;
  },

  resume: function dc_resume() {
    if (this._suspendCount <= 0)
      throw Cr.NS_ERROR_UNEXPECTED;

    if (--this._suspendCount == 0 && this._pending) {
      this._dispatch();
    }
  },

  loadGroup: null,
  loadFlags: Ci.nsIRequest.LOAD_NORMAL,

  // nsIChannel
  owner: null,
  notificationCallbacks: null,
  securityInfo: null,
  contentType: null,
  contentCharset: null,
  contentLength: 0,
  contentDisposition: Ci.nsIChannel.DISPOSITION_INLINE,
  contentDispositionFilename: "",

  _pending: false,
  _suspendCount: 0,
  _listener: null,
  _context: null,

  open: function dc_open() {
    return Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  _dispatch: function dc_dispatch() {
    let request = this;

    Services.tm.currentThread.dispatch(
    {
      run: function dc_run() {
        request._listener.onStartRequest(request, request._context);
        request._listener.onStopRequest(request, request._context,
                                        Cr.NS_ERROR_FILE_NOT_FOUND);
        if (request.loadGroup) {
          request.loadGroup.removeRequest(request, request._context,
                                          Cr.NS_ERROR_FILE_NOT_FOUND);
        }
        request._pending = false;
        request.notificationCallbacks = null;
        request._listener = null;
        request._context = null;
      }
    },
    Ci.nsIThread.DISPATCH_NORMAL);
  },

  asyncOpen: function dc_asyncopenfunction(aListener, aContext) {
    if (this.loadGroup) {
      this.loadGroup.addRequest(this, aContext);
    }

    this._listener = aListener;
    this._context = aContext;
    this._pending = true;

    if (!this._suspended) {
      this._dispatch();
    }
  },

  // nsIJarChannel, needed for XHR to turn NS_ERROR_FILE_NOT_FOUND into
  // a 404 error.
  isUnsafe: false,

  setAppURI: function(aURI) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AppProtocolHandler]);
