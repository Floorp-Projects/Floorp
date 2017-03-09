/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function* test_theme_property(property) {
  let normalized = yield ExtensionTestUtils.normalizeManifest({
    "theme": {
      [property]: {
        "unrecognized_key": "unrecognized_value",
      },
    },
  });

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

add_task(function* test_manifest_themes() {
  yield test_theme_property("images");
  yield test_theme_property("colors");
  yield test_theme_property("icons");
  yield test_theme_property("unrecognized_key");
});
