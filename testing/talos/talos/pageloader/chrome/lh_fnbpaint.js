/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var gRetryCounter = 0;


function _contentFNBPaintHandler() {
  var x = content.window.performance.timing.timeToNonBlankPaint;
  if (typeof x == "undefined") {
    sendAsyncMessage("PageLoader:FNBPaintError", {});
  }
  if (x > 0) {
    sendAsyncMessage("PageLoader:LoadEvent", {"fnbpaint": x});
  } else {
    gRetryCounter += 1;
    if (gRetryCounter <= 10) {
      dump("fnbpaint is not yet available (0), retry number " + gRetryCounter + "...\n");
      content.setTimeout(_contentFNBPaintHandler, 100);
    } else {
      dump("unable to get a value for fnbpaint after " + gRetryCounter + " retries\n");
      sendAsyncMessage("PageLoader:FNBPaintError", {});
    }
  }
}

addEventListener("load", contentLoadHandlerCallback(_contentFNBPaintHandler), true); // eslint-disable-line no-undef
