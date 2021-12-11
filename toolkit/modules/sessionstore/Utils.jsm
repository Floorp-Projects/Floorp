/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Utils"];

var Utils = Object.freeze({
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
