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
    return this.isEnabled || this.isForceEnabled;
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
      !(doc instanceof doc.defaultView.HTMLDocument)
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
    if (!["http", "https"].includes(uri.scheme)) {
      return false;
    }

    if (!isBaseUri) {
      // Sadly, some high-profile pages have false positives, so bail early for those:
      let { host } = uri;
      if (this._blockedHosts.some(blockedHost => host.endsWith(blockedHost))) {
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
XPCOMUtils.defineLazyPreferenceGetter(
  Readerable,
  "isForceEnabled",
  "reader.parse-on-load.force-enabled",
  false
);
