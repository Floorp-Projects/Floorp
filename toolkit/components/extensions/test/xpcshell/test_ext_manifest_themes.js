/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function test_theme_property(property) {
  let normalized = await ExtensionTestUtils.normalizeManifest({
    "theme": {
      [property]: {
        "unrecognized_key": "unrecognized_value",
      },
    },
  }, "manifest.ThemeManifest");

  let expectedWarning;
  if (property === "unrecognized_key") {
    expectedWarning = `Error processing theme.${property}`;
  } else {
    expectedWarning = `Error processing theme.${property}.unrecognized_key`;
  }
  equal(normalized.error, undefined, "Should not have an error");
  equal(normalized.errors.length, 1, "Should have a warning");
  ok(normalized.errors[0].includes(expectedWarning),
     `The manifest warning ${JSON.stringify(normalized.errors[0])} must contain ${JSON.stringify(expectedWarning)}`);
}

add_task(async function test_manifest_themes() {
  await test_theme_property("images");
  await test_theme_property("colors");
  await test_theme_property("icons");
  await test_theme_property("unrecognized_key");
});
