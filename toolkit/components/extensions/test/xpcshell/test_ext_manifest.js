/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_manifest() {
  let badpaths = ["", " ", "\t", "http://foo.com/icon.png"];
  for (let path of badpaths) {
    let normalized = await ExtensionTestUtils.normalizeManifest({
      "browser_action": {
        "default_icon": path,
      },
    });

    ok(/Error processing browser_action.default_icon/.test(normalized.error),
       `Should have an error for ${JSON.stringify(path)}`);

    normalized = await ExtensionTestUtils.normalizeManifest({
      "browser_action": {
        "default_icon": {
          "16": path,
        },
      },
    });

    ok(/Error processing browser_action.default_icon/.test(normalized.error),
       `Should have an error for ${JSON.stringify(path)}`);
  }

  let paths = ["icon.png", "/icon.png", "./icon.png", "path to an icon.png", " icon.png"];
  for (let path of paths) {
    let normalized = await ExtensionTestUtils.normalizeManifest({
      "browser_action": {
        "default_icon": {
          "16": path,
        },
      },
    });

    ok(!normalized.error, `Should not have an error ${JSON.stringify(path)}`);

    normalized = await ExtensionTestUtils.normalizeManifest({
      "browser_action": {
        "default_icon": path,
      },
    });

    ok(!normalized.error, `Should not have an error ${JSON.stringify(path)}`);
  }
});
