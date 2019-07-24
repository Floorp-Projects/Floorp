/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
function _pdfPaintHandler() {
  content.window.addEventListener(
    "pagerendered",
    e => {
      sendAsyncMessage("PageLoader:LoadEvent", {
        time: e.detail.timestamp,
        name: "pdfpaint",
      });
    },
    { once: true }
  );
}

addEventListener(
  "load",
  // eslint-disable-next-line no-undef
  contentLoadHandlerCallback(_pdfPaintHandler),
  true
);
