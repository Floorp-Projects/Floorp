/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var gRetryCounter = 0;
var gErr = "Abort: firstNonBlankPaint value is not available after loading the page";


function _contentFNBPaintHandler() {
  var x = content.window.performance.timing.timeToNonBlankPaint;
  if (typeof x == "undefined") {
    sendAsyncMessage("PageLoader:Error", {"msg": gErr});
  }
  if (x > 0) {
    sendAsyncMessage("PageLoader:LoadEvent", {"time": x,
                                              "name": "fnbpaint"});
  } else {
    gRetryCounter += 1;
    if (gRetryCounter <= 10) {
      dump("fnbpaint is not yet available (0), retry number " + gRetryCounter + "...\n");
      content.setTimeout(_contentFNBPaintHandler, 100);
    } else {
      dump("unable to get a value for fnbpaint after " + gRetryCounter + " retries\n");
      sendAsyncMessage("PageLoader:Error", {"msg": gErr});
    }
  }
}

addEventListener("load", contentLoadHandlerCallback(_contentFNBPaintHandler), true); // eslint-disable-line no-undef
