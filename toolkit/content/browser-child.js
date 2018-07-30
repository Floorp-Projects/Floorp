/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

ChromeUtils.import("resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://gre/modules/WebProgressChild.jsm");

this.WebProgress = new WebProgressChild(this);

addEventListener("DOMTitleChanged", function(aEvent) {
  if (!aEvent.isTrusted || aEvent.target.defaultView != content)
    return;
  sendAsyncMessage("DOMTitleChanged", { title: content.document.title });
}, false);

addEventListener("DOMWindowClose", function(aEvent) {
  if (!aEvent.isTrusted)
    return;
  sendAsyncMessage("DOMWindowClose");
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

/**
 * Remote createAboutBlankContentViewer request handler.
 */
addMessageListener("Browser:CreateAboutBlank", function(aMessage) {
  if (!content.document || content.document.documentURI != "about:blank") {
    throw new Error("Can't create a content viewer unless on about:blank");
  }
  let principal = aMessage.data;
  principal = BrowserUtils.principalWithMatchingOA(principal, content.document.nodePrincipal);
  docShell.createAboutBlankContentViewer(principal);
});

addMessageListener("InPermitUnload", msg => {
  let inPermitUnload = docShell.contentViewer && docShell.contentViewer.inPermitUnload;
  sendAsyncMessage("InPermitUnload", {id: msg.data.id, inPermitUnload});
});

addMessageListener("PermitUnload", msg => {
  sendAsyncMessage("PermitUnload", {id: msg.data.id, kind: "start"});

  let permitUnload = true;
  if (docShell && docShell.contentViewer) {
    permitUnload = docShell.contentViewer.permitUnload(msg.data.aPermitUnloadFlags);
  }

  sendAsyncMessage("PermitUnload", {id: msg.data.id, kind: "end", permitUnload});
});

// We may not get any responses to Browser:Init if the browser element
// is torn down too quickly.
var outerWindowID = content.windowUtils.outerWindowID;
sendAsyncMessage("Browser:Init", {outerWindowID});
