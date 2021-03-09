/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm"
);

// This message is used to measure content process startup performance in Talos
// tests.
sendAsyncMessage("Content:BrowserChildReady", {
  time: Services.telemetry.msSystemNow(),
});

// This is here for now until we find a better way of forcing an about:blank load
// with a particular principal that doesn't involve the message manager. We can't
// do this with JS Window Actors for now because JS Window Actors are tied to the
// document principals themselves, so forcing the load with a new principal is
// self-destructive in that case.
addMessageListener("BrowserElement:CreateAboutBlank", message => {
  if (!content.document || content.document.documentURI != "about:blank") {
    throw new Error("Can't create a content viewer unless on about:blank");
  }
  let { principal, partitionedPrincipal } = message.data;
  principal = BrowserUtils.principalWithMatchingOA(
    principal,
    content.document.nodePrincipal
  );
  partitionedPrincipal = BrowserUtils.principalWithMatchingOA(
    partitionedPrincipal,
    content.document.partitionedPrincipal
  );
  docShell.createAboutBlankContentViewer(principal, partitionedPrincipal);
});
