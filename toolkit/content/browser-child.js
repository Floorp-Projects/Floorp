/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

ChromeUtils.defineModuleGetter(
  this,
  "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm"
);

try {
  docShell
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIBrowserChild)
    .beginSendingWebProgressEventsToParent();
} catch (e) {
  // In responsive design mode, we do not have a BrowserChild for the in-parent
  // document.
}

// This message is used to measure content process startup performance in Talos
// tests.
sendAsyncMessage("Content:BrowserChildReady", {
  time: Services.telemetry.msSystemNow(),
});

addEventListener(
  "DOMTitleChanged",
  function(aEvent) {
    if (!aEvent.isTrusted || aEvent.target.defaultView != content) {
      return;
    }
    sendAsyncMessage("DOMTitleChanged", { title: content.document.title });
  },
  false
);

addEventListener(
  "ImageContentLoaded",
  function(aEvent) {
    if (content.document instanceof Ci.nsIImageDocument) {
      let req = content.document.imageRequest;
      if (!req.image) {
        return;
      }
      sendAsyncMessage("ImageDocumentLoaded", {
        width: req.image.width,
        height: req.image.height,
      });
    }
  },
  false
);

// This is here for now until we find a better way of forcing an about:blank load
// with a particular principal that doesn't involve the message manager. We can't
// do this with JS Window Actors for now because JS Window Actors are tied to the
// document principals themselves, so forcing the load with a new principal is
// self-destructive in that case.
addMessageListener("BrowserElement:CreateAboutBlank", message => {
  if (!content.document || content.document.documentURI != "about:blank") {
    throw new Error("Can't create a content viewer unless on about:blank");
  }
  let { principal, storagePrincipal } = message.data;
  principal = BrowserUtils.principalWithMatchingOA(
    principal,
    content.document.nodePrincipal
  );
  storagePrincipal = BrowserUtils.principalWithMatchingOA(
    storagePrincipal,
    content.document.effectiveStoragePrincipal
  );
  docShell.createAboutBlankContentViewer(principal, storagePrincipal);
});

// We may not get any responses to Browser:Init if the browser element
// is torn down too quickly.
var outerWindowID = docShell.outerWindowID;
var browsingContextId = docShell.browsingContext.id;
sendAsyncMessage("Browser:Init", { outerWindowID, browsingContextId });
