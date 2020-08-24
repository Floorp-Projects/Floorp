/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function waitForAnimationFrames() {
  // Wait for 2 animation frames in hopes it's actually done resizing.
  return new Promise(resolve =>
    window.requestAnimationFrame(() => window.requestAnimationFrame(resolve))
  );
}

function closeEnough(actual, expected) {
  return expected - 1 < actual && actual < expected + 1;
}

async function resizeWindow(x, y) {
  window.innerWidth = x;
  window.innerHeight = y;

  await waitForAnimationFrames();

  await TestUtils.waitForCondition(
    () => {
      info(`window is ${window.innerWidth} x ${window.innerHeight}`);
      if (
        closeEnough(window.innerWidth, x) &&
        closeEnough(window.innerHeight, y)
      ) {
        return true;
      }
      window.innerWidth = x;
      window.innerHeight = y;
      return false;
    },
    `Wait for ${x}x${y}`,
    250
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
      ok(`${box.clientWidth} close enough to ${x}`);
      ok(`${box.clientHeight} close enough to ${y}`);
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

add_task(async function testResizing() {
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

    await resizeWindow(600, 500);

    // 100 wider for window, add back the old 4px padding, it's now 16px * 2.
    let updatedWidth = initialWidth + 100 + 8 - 32;
    await waitForExpectedSize(helper, updatedWidth, initialHeight + 100);

    await resizeWindow(1100, 900);

    await waitForExpectedSize(helper, 1000, 650);

    await helper.closeDialog();

    await resizeWindow(innerWidth, innerHeight);
  });
});
