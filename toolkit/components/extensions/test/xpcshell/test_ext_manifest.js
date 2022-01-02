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

add_task(async function test_manifest_warnings_on_unexpected_props() {
  let extension = await ExtensionTestUtils.loadExtension({
    manifest: {
      background: {
        scripts: ["bg.js"],
        wrong_prop: true,
      },
    },
    files: {
      "bg.js": "",
    },
  });

  ExtensionTestUtils.failOnSchemaWarnings(false);
  await extension.startup();
  ExtensionTestUtils.failOnSchemaWarnings(true);

  // Retrieve the warning message collected by the Extension class
  // packagingWarning method.
  const { warnings } = extension.extension;
  equal(warnings.length, 1, "Got the expected number of manifest warnings");

  const expectedMessage =
    "Reading manifest: Warning processing background.wrong_prop";
  ok(
    warnings[0].startsWith(expectedMessage),
    "Got the expected warning message format"
  );

  await extension.unload();
});
