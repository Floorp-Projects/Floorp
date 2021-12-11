/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function backgroundScript() {
  let received_ports_number = 0;

  const expected_received_ports_number = 1;

  function countReceivedPorts(port) {
    received_ports_number++;

    if (port.name == "check-results") {
      browser.runtime.onConnect.removeListener(countReceivedPorts);

      browser.test.assertEq(
        expected_received_ports_number,
        received_ports_number,
        "invalid connect should not create a port"
      );

      browser.test.notifyPass("runtime.connect invalid params");
    }
  }

  browser.runtime.onConnect.addListener(countReceivedPorts);

  let childFrame = document.createElement("iframe");
  childFrame.src = "extensionpage.html";
  document.body.appendChild(childFrame);
}

function senderScript() {
  let detected_invalid_connect_params = 0;

  const invalid_connect_params = [
    // too many params
    [
      "fake-extensions-id",
      { name: "fake-conn-name" },
      "unexpected third params",
    ],
    // invalid params format
    [{}, {}],
    ["fake-extensions-id", "invalid-connect-info-format"],
  ];
  const expected_detected_invalid_connect_params =
    invalid_connect_params.length;

  function assertInvalidConnectParamsException(params) {
    try {
      browser.runtime.connect(...params);
    } catch (e) {
      detected_invalid_connect_params++;
      browser.test.assertTrue(
        e.toString().includes("Incorrect argument types for runtime.connect."),
        "exception message is correct"
      );
    }
  }
  for (let params of invalid_connect_params) {
    assertInvalidConnectParamsException(params);
  }
  browser.test.assertEq(
    expected_detected_invalid_connect_params,
    detected_invalid_connect_params,
    "all invalid runtime.connect params detected"
  );

  browser.runtime.connect(browser.runtime.id, { name: "check-results" });
}

let extensionData = {
  background: backgroundScript,
  files: {
    "senderScript.js": senderScript,
    "extensionpage.html": `<!DOCTYPE html><meta charset="utf-8"><script src="senderScript.js"></script>`,
  },
};

add_task(async function test_backgroundRuntimeConnectParams() {
  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  await extension.awaitFinish("runtime.connect invalid params");

  await extension.unload();
});
