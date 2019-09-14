/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the Picture-in-Picture toggle is clickable even if the
 * video element has pointer-events: none.
 */
add_task(async () => {
  const PAGE = TEST_ROOT + "test-pointer-events-none.html";
  await testToggle(PAGE, {
    "with-controls": { canToggle: true },
    "no-controls": { canToggle: true },
  });
});
