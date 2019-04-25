/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the Picture-in-Picture toggle can appear and be clicked
 * when the video is overlaid with transparent elements.
 */
add_task(async () => {
  const PAGE = TEST_ROOT + "test-transparent-overlay-1.html";
  await testToggle(PAGE, {
    "video-transparent-background": { canToggle: true },
    "video-alpha-background": { canToggle: true },
  });
});
