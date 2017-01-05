/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the local testing infrastructure.
 */

"use strict";

/* import-globals-from loader.js */

/**
 * Tests the truth assertion function.
 */
add_task(function* test_assert_truth() {
  Assert.ok(1 != 2);
});

/**
 * Tests the equality assertion function.
 */
add_task(function* test_assert_equality() {
  Assert.equal(1 + 1, 2);
});

/**
 * Uses some of the utility functions provided by the framework.
 */
add_task(function* test_utility_functions() {
  // The "print" function is useful to log information that is not known before.
  let randomString = "R" + Math.floor(Math.random() * 10);
  Output.print("The random contents will be '" + randomString + "'.");

  // Create the text file with the random contents.
  let path = yield TestUtils.getTempFile("test-infrastructure.txt");
  yield OS.File.writeAtomic(path, new TextEncoder().encode(randomString));

  // Test a few utility functions.
  yield TestUtils.waitForTick();
  yield TestUtils.waitMs(50);

  let promiseMyNotification = TestUtils.waitForNotification("my-topic");
  Services.obs.notifyObservers(null, "my-topic", "");
  yield promiseMyNotification;

  // Check the file size.  The file will be deleted automatically later.
  Assert.equal((yield OS.File.stat(path)).size, randomString.length);
});

/**
 * This type of test has access to the content declared above.
 */
add_task(function* test_content() {
  Assert.equal($("paragraph").innerHTML, "Paragraph contents.");

  let promiseMyEvent = TestUtils.waitForEvent($("paragraph"), "MyEvent");

  let event = document.createEvent("CustomEvent");
  event.initCustomEvent("MyEvent", true, false, {});
  $("paragraph").dispatchEvent(event);

  yield promiseMyEvent;
});
