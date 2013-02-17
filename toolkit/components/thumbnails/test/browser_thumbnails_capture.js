/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests ensure that capturing a sites's thumbnail, saving it and
 * retrieving it from the cache works.
 */
function runTests() {
  // Create a tab with a red background.
  yield addTab("data:text/html,<body bgcolor=ff0000></body>");
  yield captureAndCheckColor(255, 0, 0, "we have a red thumbnail");

  // Load a page with a green background.
  yield navigateTo("data:text/html,<body bgcolor=00ff00></body>");
  yield captureAndCheckColor(0, 255, 0, "we have a green thumbnail");

  // Load a page with a blue background.
  yield navigateTo("data:text/html,<body bgcolor=0000ff></body>");
  yield captureAndCheckColor(0, 0, 255, "we have a blue thumbnail");
}
