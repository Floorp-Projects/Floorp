(function() {
  var accumulatedRect = null;
  var onpaint = [];
  var debug = SpecialPowers.getBoolPref("testing.paint_listener.debug", false);
  const FlushModes = {
    FLUSH: 0,
    NOFLUSH: 1,
  };

  function paintListener(event) {
    if (event.target != window) {
      if (debug) {
        dump("got MozAfterPaint for wrong window\n");
      }
      return;
    }
    var clientRect = event.boundingClientRect;
    var eventRect;
    if (clientRect) {
      eventRect = [
        clientRect.left,
        clientRect.top,
        clientRect.right,
        clientRect.bottom,
      ];
    } else {
      eventRect = [0, 0, 0, 0];
    }
    if (debug) {
      dump("got MozAfterPaint: " + eventRect.join(",") + "\n");
    }
    accumulatedRect = accumulatedRect
      ? [
          Math.min(accumulatedRect[0], eventRect[0]),
          Math.min(accumulatedRect[1], eventRect[1]),
          Math.max(accumulatedRect[2], eventRect[2]),
          Math.max(accumulatedRect[3], eventRect[3]),
        ]
      : eventRect;
    if (debug) {
      dump("Dispatching " + onpaint.length + " onpaint listeners\n");
    }
    while (onpaint.length) {
      window.setTimeout(onpaint.pop(), 0);
    }
  }
  window.addEventListener("MozAfterPaint", paintListener);

  function waitForPaints(callback, subdoc, flushMode) {
    // Wait until paint suppression has ended
    var utils = SpecialPowers.getDOMWindowUtils(window);
    if (utils.paintingSuppressed) {
      if (debug) {
        dump("waiting for paint suppression to end...\n");
      }
      window.setTimeout(function() {
        waitForPaints(callback, subdoc, flushMode);
      }, 0);
      return;
    }

    // The call to getBoundingClientRect will flush pending layout
    // notifications. Sometimes, however, this is undesirable since it can mask
    // bugs where the code under test should be performing the flush.
    if (flushMode === FlushModes.FLUSH) {
      document.documentElement.getBoundingClientRect();
      if (subdoc) {
        subdoc.documentElement.getBoundingClientRect();
      }
    }

    if (utils.isMozAfterPaintPending) {
      if (debug) {
        dump("waiting for paint...\n");
      }
      onpaint.push(function() {
        waitForPaints(callback, subdoc, FlushModes.NOFLUSH);
      });
      if (utils.isTestControllingRefreshes) {
        utils.advanceTimeAndRefresh(0);
      }
      return;
    }

    if (debug) {
      dump("done...\n");
    }
    var result = accumulatedRect || [0, 0, 0, 0];
    accumulatedRect = null;
    callback.apply(null, result);
  }

  window.waitForAllPaintsFlushed = function(callback, subdoc) {
    waitForPaints(callback, subdoc, FlushModes.FLUSH);
  };

  window.waitForAllPaints = function(callback) {
    waitForPaints(callback, null, FlushModes.NOFLUSH);
  };

  window.promiseAllPaintsDone = function(subdoc = null, flush = false) {
    var flushmode = flush ? FlushModes.FLUSH : FlushModes.NOFLUSH;
    return new Promise(function(resolve, reject) {
      // The callback is given the components of the rect, but resolve() can
      // only be given one arg, so we turn it back into an array.
      waitForPaints((l, r, t, b) => resolve([l, r, t, b]), subdoc, flushmode);
    });
  };
})();
