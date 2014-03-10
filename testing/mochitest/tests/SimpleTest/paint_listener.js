(function() {
  var accumulatedRect = null;
  var onpaint = function() {};
  var debug = false;

  function paintListener(event) {
    if (event.target != window)
      return;
    var eventRect =
      [ event.boundingClientRect.left,
        event.boundingClientRect.top,
        event.boundingClientRect.right,
        event.boundingClientRect.bottom ];
    if (debug) {
      dump("got MozAfterPaint: " + eventRect.join(",") + "\n");
    }
    accumulatedRect = accumulatedRect
                    ? [ Math.min(accumulatedRect[0], eventRect[0]),
                        Math.min(accumulatedRect[1], eventRect[1]),
                        Math.max(accumulatedRect[2], eventRect[2]),
                        Math.max(accumulatedRect[3], eventRect[3]) ]
                    : eventRect;
    onpaint();
  }
  window.addEventListener("MozAfterPaint", paintListener, false);

  function waitForAllPaintsFlushed(callback, subdoc) {
    document.documentElement.getBoundingClientRect();
    if (subdoc) {
      subdoc.documentElement.getBoundingClientRect();
    }
    var CI = Components.interfaces;
    var utils = window.QueryInterface(CI.nsIInterfaceRequestor)
                .getInterface(CI.nsIDOMWindowUtils);
    if (!utils.isMozAfterPaintPending) {
      if (debug) {
        dump("done...\n");
      }
      var result = accumulatedRect || [ 0, 0, 0, 0 ];
      accumulatedRect = null;
      onpaint = function() {};
      callback.apply(null, result);
      return;
    }
    if (debug) {
      dump("waiting for paint...\n");
    }
    onpaint = function() { waitForAllPaintsFlushed(callback, subdoc); };
  }
  window.waitForAllPaintsFlushed = waitForAllPaintsFlushed;
})();
