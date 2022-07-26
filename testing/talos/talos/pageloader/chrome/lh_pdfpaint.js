/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

function _pdfPaintHandler() {
  content.window.addEventListener(
    "pagerendered",
    e => {
      if (e.detail.pageNumber !== 1) {
        sendAsyncMessage("PageLoader:Error", {
          msg: `Error: Expected page 1 got ${e.detail.pageNumber}`,
        });
        return;
      }
      sendAsyncMessage("PageLoader:LoadEvent", {
        time: e.detail.timestamp,
        name: "pdfpaint",
      });
    },
    { once: true }
  );
}

addEventListener("DOMContentLoaded", _pdfPaintHandler, true);
