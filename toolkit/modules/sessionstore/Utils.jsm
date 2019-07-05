/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Utils"];

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);

ChromeUtils.defineModuleGetter(
  this,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "eTLDService",
  "@mozilla.org/network/effective-tld-service;1",
  "nsIEffectiveTLDService"
);

var Utils = Object.freeze({
  serializeInputStream(aStream) {
    let data = {
      content: NetUtil.readInputStreamToString(aStream, aStream.available()),
    };

    if (aStream instanceof Ci.nsIMIMEInputStream) {
      data.headers = new Map();
      aStream.visitHeaders((name, value) => {
        data.headers.set(name, value);
      });
    }

    return data;
  },

  /**
   * Returns true if the |url| passed in is part of the given root |domain|.
   * For example, if |url| is "www.mozilla.org", and we pass in |domain| as
   * "mozilla.org", this will return true. It would return false the other way
   * around.
   */
  hasRootDomain(url, domain) {
    let host;

    try {
      host = Services.io.newURI(url).host;
    } catch (e) {
      // The given URL probably doesn't have a host.
      return false;
    }

    return eTLDService.hasRootDomain(host, domain);
  },

  shallowCopy(obj) {
    let retval = {};

    for (let key of Object.keys(obj)) {
      retval[key] = obj[key];
    }

    return retval;
  },

  /**
   * Restores frame tree |data|, starting at the given root |frame|. As the
   * function recurses into descendant frames it will call cb(frame, data) for
   * each frame it encounters, starting with the given root.
   */
  restoreFrameTreeData(frame, data, cb) {
    // Restore data for the root frame.
    // The callback can abort by returning false.
    if (cb(frame, data) === false) {
      return;
    }

    if (!data.hasOwnProperty("children")) {
      return;
    }

    // Recurse into child frames.
    SessionStoreUtils.forEachNonDynamicChildFrame(frame, (subframe, index) => {
      if (data.children[index]) {
        this.restoreFrameTreeData(subframe, data.children[index], cb);
      }
    });
  },
});
