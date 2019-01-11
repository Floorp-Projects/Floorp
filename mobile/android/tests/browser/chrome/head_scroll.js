/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function fuzzyEquals(a, b) {
  return (Math.abs(a - b) < 1e-6);
}

function getFrame(browser, { frame = null }) {
  let window;
  if (frame !== null) {
    window = browser.contentWindow.frames[frame];
  } else {
    window = browser.contentWindow;
  }
  return window;
}

function setScrollPosition(browser,
                           { x = 0, y = 0, zoom = 0, frame }) {
  let window = getFrame(browser, {frame});
  if (zoom) {
    browser.contentWindow.windowUtils.setResolutionAndScaleTo(zoom);
  }
  window.scrollTo(x, y);
}

function checkScroll(browser, data) {
  let {x, y, zoom} = data;
  let utils = getFrame(browser, data).windowUtils;

  let actualX = {}, actualY = {};
  let actualZoom = utils.getResolution();
  utils.getScrollXY(false, actualX, actualY);

  if (data.hasOwnProperty("x")) {
    is(actualX.value, x, "scrollX set correctly");
  }
  if (data.hasOwnProperty("y")) {
    is(actualY.value, y, "scrollY set correctly");
  }
  if (zoom) {
    ok(fuzzyEquals(actualZoom, zoom), "zoom set correctly");
  }
}

function getScrollString({ x = 0, y = 0 }) {
  return x + "," + y;
}
