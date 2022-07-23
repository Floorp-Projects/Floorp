/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_sendMessage_error() {
  async function background() {
    let circ = {};
    circ.circ = circ;
    let testCases = [
      // [arguments, expected error string],
      [[], "runtime.sendMessage's message argument is missing"],
      [
        [null, null, null, 42],
        "runtime.sendMessage's last argument is not a function",
      ],
      [[null, null, 1], "runtime.sendMessage's options argument is invalid"],
      [
        [1, null, null],
        "runtime.sendMessage's extensionId argument is invalid",
      ],
      [
        [null, null, null, null, null],
        "runtime.sendMessage received too many arguments",
      ],

      // Even when the parameters are accepted, we still expect an error
      // because there is no onMessage listener.
      [
        [null, null, null],
        "Could not establish connection. Receiving end does not exist.",
      ],

      // Structured cloning doesn't work with DOM objects
      [[null, location, null], "The object could not be cloned."],
      [[null, [circ, location], null], "The object could not be cloned."],
    ];

    // Repeat all tests with the undefined value instead of null.
    for (let [args, expectedError] of testCases.slice()) {
      args = args.map(arg => (arg === null ? undefined : arg));
      testCases.push([args, expectedError]);
    }

    for (let [args, expectedError] of testCases) {
      let description = `runtime.sendMessage(${args.map(String).join(", ")})`;

      await browser.test.assertRejects(
        browser.runtime.sendMessage(...args),
        expectedError,
        `expected error message for ${description}`
      );
    }

    browser.test.notifyPass("sendMessage parameter validation");
  }
  let extensionData = {
    background,
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  await extension.awaitFinish("sendMessage parameter validation");

  await extension.unload();
});
