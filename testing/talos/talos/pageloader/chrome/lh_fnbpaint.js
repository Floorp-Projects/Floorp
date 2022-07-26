/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

var gErr =
  "Abort: firstNonBlankPaint value is not available after loading the page";
var gRetryCounter = 0;

function _contentFNBPaintHandler() {
  var x = content.window.performance.timing.timeToNonBlankPaint;
  if (typeof x == "undefined") {
    sendAsyncMessage("PageLoader:Error", { msg: gErr });
  }
  if (x > 0) {
    dump("received fnbpaint value\n");
    sendAsyncMessage("PageLoader:LoadEvent", { time: x, name: "fnbpaint" });
    gRetryCounter = 0;
  } else {
    gRetryCounter += 1;
    if (gRetryCounter <= 10) {
      dump(
        "\nfnbpaint is not yet available (0), retry number " +
          gRetryCounter +
          "...\n"
      );
      content.setTimeout(_contentFNBPaintHandler, 100);
    } else {
      dump(
        "\nunable to get a value for fnbpaint after " +
          gRetryCounter +
          " retries\n"
      );
      sendAsyncMessage("PageLoader:Error", { msg: gErr });
    }
  }
}

addEventListener(
  "load",
  // eslint-disable-next-line no-undef
  contentLoadHandlerCallback(_contentFNBPaintHandler),
  true
);
