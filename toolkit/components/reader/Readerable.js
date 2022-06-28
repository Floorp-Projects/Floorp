// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This file and Readability-readerable.js are merged together into
// Readerable.jsm.

/* exported Readerable */
/* import-globals-from Readability-readerable.js */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

function isNodeVisible(node) {
  return node.clientHeight > 0 && node.clientWidth > 0;
}

var Readerable = {
  get isEnabledForParseOnLoad() {
    return this.isEnabled;
  },

  /**
   * Decides whether or not a document is reader-able without parsing the whole thing.
   *
   * @param doc A document to parse.
   * @return boolean Whether or not we should show the reader mode button.
   */
  isProbablyReaderable(doc) {
    // Only care about 'real' HTML documents:
    if (
      doc.mozSyntheticDocument ||
      !doc.defaultView.HTMLDocument.isInstance(doc)
    ) {
      return false;
    }

    let uri = Services.io.newURI(doc.location.href);
    if (!this.shouldCheckUri(uri)) {
      return false;
    }

    return isProbablyReaderable(doc, isNodeVisible);
  },

  _blockedHosts: [
    "amazon.com",
    "github.com",
    "mail.google.com",
    "pinterest.com",
    "reddit.com",
    "twitter.com",
    "youtube.com",
  ],

  shouldCheckUri(uri, isBaseUri = false) {
    if (!["http", "https", "file", "moz-nullprincipal"].includes(uri.scheme)) {
      return false;
    }

    if (!isBaseUri && uri.scheme.startsWith("http")) {
      // Sadly, some high-profile pages have false positives, so bail early for those:
      let { host } = uri;
      if (this._blockedHosts.some(blockedHost => host.endsWith(blockedHost))) {
        // Allow github on non-project pages
        if (
          host == "github.com" &&
          !uri.filePath.includes("/projects") &&
          !uri.filePath.includes("/issues")
        ) {
          return true;
        }
        return false;
      }

      if (uri.filePath == "/") {
        return false;
      }
    }

    return true;
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  Readerable,
  "isEnabled",
  "reader.parse-on-load.enabled",
  true
);
