/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the Picture-in-Picture toggle can appear and be clicked
 * when the video is overlaid with elements that have zero and partial
 * opacity. Also tests the site-specific toggle visibility threshold to
 * ensure that we can configure opacities that can't be clicked through.
 */
add_task(async () => {
  const PAGE = TEST_ROOT + "test-transparent-overlay-2.html";
  await testToggle(PAGE, {
    "video-zero-opacity": { canToggle: true },
    "video-partial-opacity": { canToggle: true },
  });

  // Now set a toggle visibility threshold to 0.4 and ensure that the
  // partially obscured toggle can't be clicked.
  Services.ppmm.sharedData.set(SHARED_DATA_KEY, {
    "*://example.com/*": { visibilityThreshold: 0.4 },
  });
  Services.ppmm.sharedData.flush();

  await testToggle(PAGE, {
    "video-zero-opacity": { canToggle: true },
    "video-partial-opacity": { canToggle: false },
  });

  Services.ppmm.sharedData.set(SHARED_DATA_KEY, {});
  Services.ppmm.sharedData.flush();
});
