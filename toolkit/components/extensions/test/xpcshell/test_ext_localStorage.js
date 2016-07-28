/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function backgroundScript() {
  let hasRun = localStorage.getItem("has-run");
  let result;
  if (!hasRun) {
    localStorage.setItem("has-run", "yup");
    localStorage.setItem("test-item", "item1");
    result = "item1";
  } else {
    let data = localStorage.getItem("test-item");
    if (data == "item1") {
      localStorage.setItem("test-item", "item2");
      result = "item2";
    } else if (data == "item2") {
      localStorage.removeItem("test-item");
      result = "deleted";
    } else if (!data) {
      localStorage.clear();
      result = "cleared";
    }
  }
  browser.test.sendMessage("result", result);
  browser.test.notifyPass("localStorage");
}

let extensionData = {
  background: backgroundScript,
};

add_task(function* test_localStorage() {
  let id = "test-webextension@mozilla.com";
  const RESULTS = ["item1", "item2", "deleted", "cleared", "item1"];

  for (let expected of RESULTS) {
    let extension = ExtensionTestUtils.loadExtension(extensionData, id);

    yield extension.startup();

    let actual = yield extension.awaitMessage("result");

    yield extension.awaitFinish("localStorage");
    yield extension.unload();

    equal(actual, expected, "got expected localStorage data");
  }
});
