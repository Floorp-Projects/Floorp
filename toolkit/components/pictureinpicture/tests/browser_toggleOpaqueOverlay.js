/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the Picture-in-Picture toggle is not clickable when
 * overlaid with opaque elements.
 */
add_task(async () => {
  const PAGE = TEST_ROOT + "test-opaque-overlay.html";
  await testToggle(PAGE, {
    "video-full-opacity": { canToggle: false },
    "video-full-opacity-over-toggle": { canToggle: false },
  });
});
