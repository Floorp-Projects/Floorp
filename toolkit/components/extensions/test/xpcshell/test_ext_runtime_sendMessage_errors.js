/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_sendMessage_error() {
  async function background() {
    let circ = {};
    circ.circ = circ;
    let testCases = [
      // [arguments, expected error string],
      [[], "runtime.sendMessage's message argument is missing"],
      [[null, null, null, null], "runtime.sendMessage's last argument is not a function"],
      [[null, null, 1], "runtime.sendMessage's options argument is invalid"],
      [[1, null, null], "runtime.sendMessage's extensionId argument is invalid"],
      [[null, null, null, null, null], "runtime.sendMessage received too many arguments"],

      // Even when the parameters are accepted, we still expect an error
      // because there is no onMessage listener.
      [[null, null, null], "Could not establish connection. Receiving end does not exist."],

      // Structural cloning doesn't work with DOM but we fall back
      // JSON serialization, so we don't expect another error.
      [[null, location, null], "Could not establish connection. Receiving end does not exist."],

      // Structured cloning supports cyclic self-references.
      [[null, [circ, location], null], "cyclic object value"],
      // JSON serialization does not support cyclic references.
      [[null, circ, null], "Could not establish connection. Receiving end does not exist."],
      // (the last two tests shows whether sendMessage is implemented as structured cloning).
    ];

    // Repeat all tests with the undefined value instead of null.
    for (let [args, expectedError] of testCases.slice()) {
      args = args.map(arg => arg === null ? undefined : arg);
      testCases.push([args, expectedError]);
    }

    for (let [args, expectedError] of testCases) {
      let description = `runtime.sendMessage(${args.map(String).join(", ")})`;

      await browser.runtime.sendMessage(...args)
        .then(() => {
          browser.test.fail(`Unexpectedly got no error for ${description}`);
        }, err => {
          browser.test.assertEq(expectedError, err.message, `expected error message for ${description}`);
        });
    }

    browser.test.notifyPass("sendMessage parameter validation");
  }
  let extensionData = {
    background,
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  yield extension.startup();

  yield extension.awaitFinish("sendMessage parameter validation");

  yield extension.unload();
});
