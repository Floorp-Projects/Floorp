/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// 'arguments' is defined by the marionette harness.
/* global arguments */

const { PlacesUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesUtils.sys.mjs"
);

let resolve = arguments[3];

try {
  // Get the number of current bookmarks
  PlacesUtils.promiseBookmarksTree(PlacesUtils.bookmarks.unfiledGuid, {
    includeItemIds: true,
  }).then(root => {
    let count = root.itemsCount;
    let maxBookmarks = arguments[2];
    // making sure we don't exceed the maximum number of bookmarks
    if (count >= maxBookmarks) {
      let toRemove = count - maxBookmarks + 1;
      console.log("We've reached the maximum number of bookmarks");
      console.log("Removing  " + toRemove);
      let children = root.children;
      for (let i = 0, p = Promise.resolve(); i < toRemove; i++) {
        p = p.then(
          _ =>
            new Promise(resolve =>
              PlacesUtils.bookmarks.remove(children[i].guid).then(res => {
                console.log("removed one bookmark");
                resolve(res);
              })
            )
        );
      }
    }
    // now adding the bookmark
    PlacesUtils.bookmarks
      .insert({
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        url: arguments[0],
        title: arguments[1],
      })
      .then(res => {
        resolve(res);
      });
  });
} catch (error) {
  let res = { logs: {}, result: 1, result_message: error.toString() };
  resolve(res);
}
