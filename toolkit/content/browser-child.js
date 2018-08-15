/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://gre/modules/WebProgressChild.jsm");

this.WebProgress = new WebProgressChild(this);

addEventListener("DOMTitleChanged", function(aEvent) {
  if (!aEvent.isTrusted || aEvent.target.defaultView != content)
    return;
  sendAsyncMessage("DOMTitleChanged", { title: content.document.title });
}, false);

addEventListener("ImageContentLoaded", function(aEvent) {
  if (content.document instanceof Ci.nsIImageDocument) {
    let req = content.document.imageRequest;
    if (!req.image)
      return;
    sendAsyncMessage("ImageDocumentLoaded", { width: req.image.width,
                                              height: req.image.height });
  }
}, false);

// We may not get any responses to Browser:Init if the browser element
// is torn down too quickly.
var outerWindowID = content.windowUtils.outerWindowID;
sendAsyncMessage("Browser:Init", {outerWindowID});
