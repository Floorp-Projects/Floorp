/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://webapprt/modules/WebappRT.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// Allow certain origins to load as top-level documents
const allowedOrigins = [
  WebappRT.config.app.origin,
  "https://browserid.org",
  "https://www.facebook.com",
];

function ContentPolicy() {}

ContentPolicy.prototype = {
  classID: Components.ID("{75acd178-3d5a-48a7-bd92-fba383520ae6}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPolicy]),

  shouldLoad: function(contentType, contentLocation, requestOrigin, context, mimeTypeGuess, extra) {
    // Redirect top-level document loads that aren't special schemes to the
    // default browser when trying to load pages outside of allowed origins
    let {prePath, scheme} = contentLocation;
    if (contentType == Ci.nsIContentPolicy.TYPE_DOCUMENT &&
        !/^(about|chrome|resource)$/.test(scheme) &&
        allowedOrigins.indexOf(prePath) == -1) {

      // Send the url to the default browser
      Cc["@mozilla.org/uriloader/external-protocol-service;1"].
        getService(Ci.nsIExternalProtocolService).
        getProtocolHandlerInfo(scheme).
        launchWithURI(contentLocation);

      // Using window.open will first open then navigate, so explicitly close
      if (context.currentURI.spec == "about:blank") {
        context.ownerDocument.defaultView.close();
      };

      return Ci.nsIContentPolicy.REJECT_SERVER;
    }

    return Ci.nsIContentPolicy.ACCEPT;
  },

  shouldProcess: function(contentType, contentLocation, requestOrigin, context, mimeType, extra) {
    return Ci.nsIContentPolicy.ACCEPT;
  },
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentPolicy]);
