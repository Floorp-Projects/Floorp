/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function queryAppName() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("result", { appName: navigator.appName });
    },
  });
  await extension.startup();
  let result = await extension.awaitMessage("result");
  await extension.unload();
  return result.appName;
}

const APPNAME_OVERRIDE = "MyTestAppName";

add_task(
  {
    pref_set: [["general.appname.override", APPNAME_OVERRIDE]],
  },
  async function test_appName_normal() {
    let appName = await queryAppName();
    Assert.equal(appName, APPNAME_OVERRIDE);
  }
);

add_task(
  {
    pref_set: [
      ["general.appname.override", APPNAME_OVERRIDE],
      ["privacy.resistFingerprinting", true],
    ],
  },
  async function test_appName_resistFingerprinting() {
    let appName = await queryAppName();
    Assert.equal(appName, APPNAME_OVERRIDE);
  }
);
