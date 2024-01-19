/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function waitForAnimationFrames() {
  // Wait for 2 animation frames in hopes it's actually done resizing.
  return new Promise(resolve =>
    window.requestAnimationFrame(() => window.requestAnimationFrame(resolve))
  );
}

async function mouseMoveAndWait(elem) {
  let mouseMovePromise = BrowserTestUtils.waitForEvent(elem, "mousemove");
  EventUtils.synthesizeMouseAtCenter(elem, { type: "mousemove" });
  await mouseMovePromise;
  await TestUtils.waitForTick();
}

function closeEnough(actual, expected) {
  return expected - 1 < actual && actual < expected + 1;
}

async function resizeWindow(x, y) {
  let resizePromise = BrowserTestUtils.waitForEvent(window, "resize");
  window.resizeTo(
    x + window.outerWidth - window.innerWidth,
    y + window.outerHeight - window.innerHeight
  );
  await resizePromise;

  await waitForAnimationFrames();

  ok(
    closeEnough(window.innerWidth, x),
    `Window innerWidth ${window.innerWidth} is close enough to ${x}`
  );
  ok(
    closeEnough(window.innerHeight, y),
    `Window innerHeight ${window.innerHeight} is close enough to ${y}`
  );
}

async function waitForExpectedSize(helper, x, y) {
  // Wait a few frames, this is generally enough for the resize to happen.
  await waitForAnimationFrames();

  let isExpectedSize = () => {
    let box = helper.dialog._box;
    info(`Dialog is ${box.clientWidth}x${box.clientHeight}`);
    if (closeEnough(box.clientWidth, x) && closeEnough(box.clientHeight, y)) {
      // Make sure there's an assertion so the test passes.
      ok(true, `${box.clientWidth} close enough to ${x}`);
      ok(true, `${box.clientHeight} close enough to ${y}`);
      return true;
    }
    return false;
  };

  if (isExpectedSize()) {
    // We can stop now if we hit the expected size.
    return;
  }

  // In verify and debug runs sometimes this takes longer than expected,
  // fallback to the slow method.
  await TestUtils.waitForCondition(isExpectedSize, `Wait for ${x}x${y}`);
}

async function checkPreviewNavigationVisibility(expected) {
  function isHidden(elem) {
    // BTU.isHidden can't handle shadow DOM elements
    return !elem.getBoundingClientRect().height;
  }

  let previewStack = document.querySelector(".previewStack");
  let paginationElem = document.querySelector(".printPreviewNavigation");
  // move the mouse to a known position, then back to the preview to show the paginator
  await mouseMoveAndWait(gURLBar.textbox);
  await mouseMoveAndWait(previewStack);

  ok(
    BrowserTestUtils.isVisible(paginationElem),
    "The preview pagination toolbar is visible"
  );
  for (let [id, visible] of Object.entries(expected)) {
    let elem = paginationElem.shadowRoot.querySelector(`#${id}`);
    if (visible) {
      ok(!isHidden(elem), `Navigation element ${id} is visible`);
    } else {
      ok(isHidden(elem), `Navigation element ${id} is hidden`);
    }
  }
}

add_task(async function testResizing() {
  if (window.windowState != window.STATE_NORMAL) {
    todo_is(
      window.windowState,
      window.STATE_NORMAL,
      "windowState should be STATE_NORMAL"
    );
    // On Windows the size of the window decoration depends on the size mode.
    // Trying to set the inner size of a maximized window changes the size mode
    // but calculates the new window size with the maximized window
    // decorations. On Linux a maximized window can also cause problems when
    // the window was maximized recently and the corresponding resize event is
    // still outstanding.
    window.restore();
    // On Linux we would have to wait for the resize event here, but the
    // restored and maximized size can also be equal. Brute forcing a resize
    // to a specific size works around that.
    await BrowserTestUtils.waitForCondition(async () => {
      let width = window.screen.availWidth * 0.75;
      let height = window.screen.availHeight * 0.75;
      window.resizeTo(width, height);
      return (
        closeEnough(window.outerWidth, width) &&
        closeEnough(window.outerHeight, height)
      );
    });
  }

  await PrintHelper.withTestPage(async helper => {
    let { innerWidth, innerHeight } = window;

    await resizeWindow(500, 400);

    await helper.startPrint();

    let chromeHeight = window.windowUtils.getBoundsWithoutFlushing(
      document.getElementById("browser")
    ).top;

    let initialWidth = 500 - 8;
    let initialHeight = 400 - 16 - chromeHeight + 5;

    await waitForExpectedSize(helper, initialWidth, initialHeight);

    // check the preview pagination state for this window size
    await checkPreviewNavigationVisibility({
      navigateHome: false,
      navigatePrevious: false,
      navigateNext: false,
      navigateEnd: false,
      sheetIndicator: true,
    });

    await resizeWindow(600, 500);

    await checkPreviewNavigationVisibility({
      navigateHome: true,
      navigatePrevious: true,
      navigateNext: true,
      navigateEnd: true,
      sheetIndicator: true,
    });

    // 100 wider for window, add back the old 4px padding, it's now 16px * 2.
    let updatedWidth = initialWidth + 100 + 8 - 32;
    await waitForExpectedSize(helper, updatedWidth, initialHeight + 100);

    await resizeWindow(1100, 900);

    await waitForExpectedSize(helper, 1000, 650);

    await checkPreviewNavigationVisibility({
      navigateHome: true,
      navigatePrevious: true,
      navigateNext: true,
      navigateEnd: true,
      sheetIndicator: true,
    });

    await helper.closeDialog();

    await resizeWindow(innerWidth, innerHeight);
  });
});
