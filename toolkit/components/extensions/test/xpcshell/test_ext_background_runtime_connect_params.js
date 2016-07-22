/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function backgroundScript() {
  let detected_invalid_connect_params = 0;
  let received_ports_number = 0;

  const invalid_connect_params = [
    // too many params
    ["fake-extensions-id", {name: "fake-conn-name"}, "unexpected third params"],
    // invalid params format
    [{}, {}],
    ["fake-extensions-id", "invalid-connect-info-format"],
  ];

  const expected_detected_invalid_connect_params = invalid_connect_params.length;
  const expected_received_ports_number = 1;

  function assertInvalidConnectParamsException(params) {
    try {
      browser.runtime.connect(...params);
    } catch (e) {
      detected_invalid_connect_params++;
      browser.test.assertTrue(e.toString().indexOf("Incorrect argument types for runtime.connect.") >= 0, "exception message is correct");
    }
  }

  function countReceivedPorts(port) {
    received_ports_number++;

    if (port.name == "check-results") {
      browser.runtime.onConnect.removeListener(countReceivedPorts);

      browser.test.assertEq(expected_detected_invalid_connect_params, detected_invalid_connect_params, "all invalid runtime.connect params detected");
      browser.test.assertEq(expected_received_ports_number, received_ports_number, "invalid connect should not create a port");

      browser.test.notifyPass("runtime.connect invalid params");
    }
  }

  browser.runtime.onConnect.addListener(countReceivedPorts);

  for (let params of invalid_connect_params) {
    assertInvalidConnectParamsException(params);
  }

  browser.runtime.connect(browser.runtime.id, {name: "check-results"});
}

let extensionData = {
  background: backgroundScript,
  manifest: {},
  files: {},
};

add_task(function* test_backgroundRuntimeConnectParams() {
  let extension = ExtensionTestUtils.loadExtension(extensionData);
  yield extension.startup();

  yield extension.awaitFinish("runtime.connect invalid params");

  yield extension.unload();
});
