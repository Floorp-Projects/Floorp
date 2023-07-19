/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function queryBuildID() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("result", { buildID: navigator.buildID });
    },
  });
  await extension.startup();
  let { buildID } = await extension.awaitMessage("result");
  await extension.unload();
  return buildID;
}

const BUILDID_OVERRIDE = "Overridden buildID";

add_task(
  {
    pref_set: [["general.buildID.override", BUILDID_OVERRIDE]],
  },
  async function test_buildID_normal() {
    let buildID = await queryBuildID();
    Assert.equal(buildID, BUILDID_OVERRIDE);
  }
);

add_task(
  {
    pref_set: [
      ["general.buildID.override", BUILDID_OVERRIDE],
      ["privacy.resistFingerprinting", true],
    ],
  },
  async function test_buildID_resistFingerprinting() {
    let buildID = await queryBuildID();
    Assert.equal(buildID, BUILDID_OVERRIDE);
  }
);
