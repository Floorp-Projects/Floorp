/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Utils"];

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);

ChromeUtils.defineModuleGetter(this, "NetUtil",
                               "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "eTLDService",
                                   "@mozilla.org/network/effective-tld-service;1",
                                   "nsIEffectiveTLDService");



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
   * A function that will recursively call |cb| to collect data for all
   * non-dynamic frames in the current frame/docShell tree.
   *
   * @param {mozIDOMWindowProxy} frame A DOM window or content frame for which
   *                                   data will be collected.
   * @param {...function} dataCollectors One or more data collection functions
   *                                     that will be called once for each non-
   *                                     dynamic frame in the given frame tree,
   *                                     and which should return the data they
   *                                     wish to save for that respective frame.
   * @return {object[]} An array with one entry per dataCollector, containing
   *                    the collected data as a nested data structure according
   *                    to the layout of the frame tree, or null if no data was
   *                    returned by the respective dataCollector.
   */
  mapFrameTree(frame, ...dataCollectors) {
    // Collect data for the current frame.
    let objs = dataCollectors.map(dataCollector => dataCollector(frame.document) || {});
    let children = dataCollectors.map(() => []);

    // Recurse into child frames.
    SessionStoreUtils.forEachNonDynamicChildFrame(frame, (subframe, index) => {
      let results = this.mapFrameTree(subframe, ...dataCollectors);
      if (!results) {
        return;
      }

      for (let j = results.length - 1; j >= 0; --j) {
        if (!results[j] || !Object.getOwnPropertyNames(results[j]).length) {
          continue;
        }
        children[j][index] = results[j];
      }
    });

    for (let i = objs.length - 1; i >= 0; --i) {
      if (!children[i].length) {
        continue;
      }
      objs[i].children = children[i];
    }

    return objs.map((obj) => Object.getOwnPropertyNames(obj).length ? obj : null);
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
