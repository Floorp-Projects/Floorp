/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the Picture-in-Picture toggle can be clicked when overlaid
 * with a transparent button, but not clicked when overlaid with an
 * opaque button.
 */
add_task(async () => {
  const PAGE = TEST_ROOT + "test-button-overlay.html";
  await testToggle(PAGE, {
    "video-partial-transparent-button": { canToggle: true },
    "video-opaque-button": { canToggle: false },
  });
});
