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
                           { x = 0, y = 0, zoom = 0, frame = null }) {
  let window = getFrame(browser, {frame});
  let topLevelUtils = browser.contentWindow.windowUtils;
  if (zoom) {
    topLevelUtils.setResolutionAndScaleTo(zoom);
  }
  // The root content document has a distinction between visual and layout
  // scroll positions. We want to set the visual one.
  // For frames, there is no such distinction and scrollToVisual() does
  // not support them, so use window.scrollTo().
  if (frame !== null) {
    window.scrollTo(x, y);
  } else {
    topLevelUtils.scrollToVisual(x, y, topLevelUtils.UPDATE_TYPE_MAIN_THREAD,
                                 topLevelUtils.SCROLL_MODE_INSTANT);
  }
}

function checkScroll(browser, data) {
  let {x, y, zoom} = data;
  let scrollPos = getScrollPosition(browser, data);

  if (data.hasOwnProperty("x")) {
    is(scrollPos.x, x, "scrollX set correctly");
  }
  if (data.hasOwnProperty("y")) {
    is(scrollPos.y, y, "scrollY set correctly");
  }
  if (zoom) {
    ok(fuzzyEquals(scrollPos.zoom, zoom), "zoom set correctly");
  }
}

function getScrollPosition(browser, data = {}) {
  let utils = getFrame(browser, data).windowUtils;
  let x = {}, y = {};

  let zoom = utils.getResolution();
  utils.getVisualViewportOffset(x, y);

  return { x: x.value, y: y.value, zoom };
}

function getScrollString({ x = 0, y = 0 }) {
  return x + "," + y;
}

function presStateToCSSPx(presState) {
  // App units aren't commonly exposed to JS, so we can't just call a helper function
  // and have to convert them ourselves instead.
  // Conversion factor taken from gfx/src/AppUnits.h.
  const APP_UNITS_PER_CSS_PX = 60;

  let state = {...presState};
  if (state.scroll) {
    let scroll = state.scroll.split(",").map(pos => parseInt(pos, 10) || 0);
    scroll = scroll.map(appUnits => Math.round(appUnits / APP_UNITS_PER_CSS_PX));
    state.scroll = getScrollString({ x: scroll[0], y: scroll[1] });
  }

  return state;
}
