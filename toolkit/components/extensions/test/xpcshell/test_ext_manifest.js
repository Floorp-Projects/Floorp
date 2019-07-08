/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function testIconPaths(icon, manifest, expectedError) {
  let normalized = await ExtensionTestUtils.normalizeManifest(manifest);

  if (expectedError) {
    ok(
      expectedError.test(normalized.error),
      `Should have an error for ${JSON.stringify(icon)}`
    );
  } else {
    ok(!normalized.error, `Should not have an error ${JSON.stringify(icon)}`);
  }
}

add_task(async function test_manifest() {
  let badpaths = ["", " ", "\t", "http://foo.com/icon.png"];
  for (let path of badpaths) {
    await testIconPaths(
      path,
      {
        icons: path,
      },
      /Error processing icons/
    );

    await testIconPaths(
      path,
      {
        icons: {
          "16": path,
        },
      },
      /Error processing icons/
    );
  }

  let paths = [
    "icon.png",
    "/icon.png",
    "./icon.png",
    "path to an icon.png",
    " icon.png",
  ];
  for (let path of paths) {
    // manifest.icons is an object
    await testIconPaths(
      path,
      {
        icons: path,
      },
      /Error processing icons/
    );

    await testIconPaths(path, {
      icons: {
        "16": path,
      },
    });
  }
});
