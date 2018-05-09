/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/GeckoViewUtils.jsm");

/* global debug:false, warn:false */
GeckoViewUtils.initLogging("GeckoView.ExternalAppService", this);

ChromeUtils.defineModuleGetter(this, "EventDispatcher",
  "resource://gre/modules/Messaging.jsm");

function ExternalAppService() {
  this.wrappedJSObject = this;
}

ExternalAppService.prototype = {
  classID: Components.ID("{a89eeec6-6608-42ee-a4f8-04d425992f45}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIExternalHelperAppService]),

  doContent(mimeType, request, context, forceSave) {
    const channel = request.QueryInterface(Ci.nsIChannel);
    debug `doContent: uri=${channel.URI.displaySpec}
                      contentType=${channel.contentType}`;

    let filename = null;
    try {
      filename = channel.contentDispositionFilename;
    } catch (e) {
      // This throws NS_ERROR_NOT_AVAILABLE if there is not
      // Content-disposition header.
    }

    GeckoViewUtils.getDispatcherForWindow(context).sendRequest({
      type: "GeckoView:ExternalResponse",
      uri: channel.URI.displaySpec,
      contentType: channel.contentType,
      contentLength: channel.contentLength,
      filename: filename
    });

    request.cancel(Cr.NS_ERROR_ABORT);
    Components.returnCode = Cr.NS_ERROR_ABORT;
  },

  applyDecodingForExtension(ext, encoding) {
    debug `applyDecodingForExtension: extension=${ext}
                                      encoding=${encoding}`;

    // This doesn't matter for us right now because
    // we shouldn't end up reading the stream.
    return true;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ExternalAppService]);
