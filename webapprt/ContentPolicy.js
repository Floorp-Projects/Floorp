/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* We used to use this component to heuristically determine which links to
 * direct to the user's default browser, but we no longer use that heuristic.
 * It will come in handy when we implement support for trusted apps, however,
 * so we left it in (although it is disabled in the manifest).
*/

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function ContentPolicy() {}

ContentPolicy.prototype = {
  classID: Components.ID("{75acd178-3d5a-48a7-bd92-fba383520ae6}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPolicy]),

  shouldLoad: function(contentType, contentLocation, requestOrigin, context, mimeTypeGuess, extra) {

    return Ci.nsIContentPolicy.ACCEPT;

    // When rejecting a load due to a content policy violation, use this code
    // to close the window opened by window.open, if any.
    //if (context.currentURI.spec == "about:blank") {
    //  context.ownerDocument.defaultView.close();
    //};
  },

  shouldProcess: function(contentType, contentLocation, requestOrigin, context, mimeType, extra) {
    return Ci.nsIContentPolicy.ACCEPT;
  },
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentPolicy]);
