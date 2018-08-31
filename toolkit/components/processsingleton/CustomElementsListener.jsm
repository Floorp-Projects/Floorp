/* -*-  indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");

// Set up Custom Elements for XUL and XHTML documents before anything else
// happens. Anything loaded here should be considered part of core XUL functionality.
// Any window-specific elements can be registered via <script> tags at the
// top of individual documents.
Services.obs.addObserver({
  observe(doc) {
    if (doc.nodePrincipal.isSystemPrincipal && (
      doc.contentType == "application/vnd.mozilla.xul+xml" ||
      doc.contentType == "application/xhtml+xml"
    )) {
      Services.scriptloader.loadSubScript(
        "chrome://global/content/customElements.js", doc.ownerGlobal);
    }
  },
}, "document-element-inserted");
