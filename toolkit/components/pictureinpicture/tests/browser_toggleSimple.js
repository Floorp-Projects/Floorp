/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that we show the Picture-in-Picture toggle on video
 * elements when hovering them with the mouse cursor, and that
 * clicking on them causes the Picture-in-Picture window to
 * open if the toggle isn't being occluded. This test tests videos
 * both with and without controls.
 */
add_task(async () => {
  await testToggle(TEST_PAGE, {
    "with-controls": { canToggle: true },
    "no-controls": { canToggle: true },
  });
});
