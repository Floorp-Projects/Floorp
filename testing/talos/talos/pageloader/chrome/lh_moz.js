/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

function _contentPaintHandler() {
  var utils = content.windowUtils;
  if (utils.isMozAfterPaintPending) {
    addEventListener(
      "MozAfterPaint",
      function afterpaint(e) {
        removeEventListener("MozAfterPaint", afterpaint, true);
        sendAsyncMessage("PageLoader:LoadEvent", {});
      },
      true
    );
  } else {
    sendAsyncMessage("PageLoader:LoadEvent", {});
  }
}

addEventListener(
  "load",
  // eslint-disable-next-line no-undef
  contentLoadHandlerCallback(_contentPaintHandler),
  true
);
