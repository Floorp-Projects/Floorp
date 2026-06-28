// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";
import {
  browserFilterTargetsForRun,
  replaceStringPref,
} from "./colocated_test_runner.ts";

Deno.test("replaceStringPref appends pref on a fresh line", () => {
  assertEquals(
    replaceStringPref(
      'user_pref("existing.pref", "value");',
      "nora.tests.filter",
      "[]",
    ),
    'user_pref("existing.pref", "value");\nuser_pref("nora.tests.filter", "[]");\n',
  );
});

Deno.test("replaceStringPref replaces an existing pref", () => {
  assertEquals(
    replaceStringPref(
      'user_pref("nora.tests.filter", "old");\nuser_pref("other.pref", "value");\n',
      "nora.tests.filter",
      '["browser-features/chrome/test/example.test.js"]',
    ),
    'user_pref("other.pref", "value");\nuser_pref("nora.tests.filter", "[\\"browser-features/chrome/test/example.test.js\\"]");\n',
  );
});

Deno.test("browserFilterTargetsForRun keeps full all-runs unfiltered", () => {
  const targets = [
    "browser-features/chrome/test/example.test.ts",
    "browser-features/pages-settings/test/lib/utils.test.ts",
  ];

  assertEquals(browserFilterTargetsForRun(targets, false), []);
  assertEquals(browserFilterTargetsForRun(targets, true), targets);
});
