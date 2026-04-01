// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@jsr/std__assert";
import {
  detectLayer,
  escapeRegExp,
  isResultMatchTarget,
  isTestFile,
  normalizeBrowserResultPath,
  parseLayer,
} from "./colocated_test_utils.ts";

Deno.test("isTestFile detects supported test patterns", () => {
  assertEquals(isTestFile("foo/bar.test.ts"), true);
  assertEquals(isTestFile("foo/bar.spec.ts"), true);
  assertEquals(isTestFile("foo/bar.test.mts"), true);
  assertEquals(isTestFile("foo/bar.spec.mjs"), true);
  assertEquals(isTestFile("foo/bar.test.tsx"), true);
  assertEquals(isTestFile("foo/bar.test.jsx"), true);
  assertEquals(isTestFile("foo/test/unit/something.test.ts"), true);
  assertEquals(isTestFile("foo\\bar.test.ts"), true);
});

Deno.test("isTestFile ignores non-test and excluded paths", () => {
  assertEquals(isTestFile("foo/bar.ts"), false);
  assertEquals(isTestFile("foo/test/index.ts"), false);
  assertEquals(isTestFile("foo/test/helper.ts"), false);
  assertEquals(isTestFile("foo/test/setup.ts"), false);
  assertEquals(isTestFile("foo/_dist/bar.test.ts"), false);
  assertEquals(isTestFile("foo/node_modules/bar.test.ts"), false);
  assertEquals(isTestFile("foo/libs/@types/gecko/foo.test.ts"), false);
  assertEquals(isTestFile("foo\\_dist\\bar.test.ts"), false);
});

Deno.test("detectLayer resolves known layer roots", () => {
  assertEquals(
    detectLayer("browser-features/chrome/common/foo.test.ts"),
    "chrome",
  );
  assertEquals(
    detectLayer("browser-features/pages-llm-chat/foo.test.ts"),
    "pages",
  );
  assertEquals(
    detectLayer("browser-features/modules/modules/foo.test.mts"),
    "esm",
  );
  assertEquals(detectLayer("bridge/loader-features/loader/foo.test.ts"), "esm");
  assertEquals(
    detectLayer("browser-features\\chrome\\test\\foo.test.ts"),
    "chrome",
  );
});

Deno.test("detectLayer returns null for unknown paths", () => {
  assertEquals(detectLayer("tools/src/foo.test.ts"), null);
  assertEquals(detectLayer("libs/some-lib/foo.test.ts"), null);
});

Deno.test("parseLayer accepts aliases and rejects invalid values", () => {
  assertEquals(parseLayer(undefined), "all");
  assertEquals(parseLayer("all"), "all");
  assertEquals(parseLayer("chrome"), "chrome");
  assertEquals(parseLayer("Chrome"), "chrome");
  assertEquals(parseLayer("esm"), "esm");
  assertEquals(parseLayer("pages"), "pages");
  assertEquals(parseLayer("page"), "pages");
  assertEquals(parseLayer("built-in-pages"), "pages");
  assertEquals(parseLayer("builtin-pages"), "pages");
  assertEquals(parseLayer("builtin"), "pages");

  assertThrows(() => parseLayer("invalid"), Error, "Invalid --layer value");
});

Deno.test("escapeRegExp escapes regex control characters", () => {
  assertEquals(escapeRegExp("foo.bar"), "foo\\.bar");
  assertEquals(escapeRegExp("file[1]"), "file\\[1\\]");
  assertEquals(escapeRegExp("a+b(c)"), "a\\+b\\(c\\)");
  assertEquals(escapeRegExp("normal"), "normal");
  assertEquals(escapeRegExp("a*b?c"), "a\\*b\\?c");
  assertEquals(escapeRegExp("$100"), "\\$100");
});

Deno.test("normalizeBrowserResultPath maps known aliases and prefixes", () => {
  assertEquals(
    normalizeBrowserResultPath("#features-chrome/common/test/foo.test.ts"),
    "browser-features/chrome/common/test/foo.test.ts",
  );
  assertEquals(
    normalizeBrowserResultPath("#features-modules/modules/foo.test.mts"),
    "browser-features/modules/modules/foo.test.mts",
  );
  assertEquals(
    normalizeBrowserResultPath("/link-features-chrome/common/foo.test.ts"),
    "browser-features/chrome/common/foo.test.ts",
  );
  assertEquals(
    normalizeBrowserResultPath("C:\\repo\\browser-features\\chrome\\x.test.ts"),
    "browser-features/chrome/x.test.ts",
  );
  assertEquals(
    normalizeBrowserResultPath("#/unexpected-alias/path.test.ts"),
    "[unknown-alias] #/unexpected-alias/path.test.ts",
  );
});

Deno.test("isResultMatchTarget matches normalized browser paths", () => {
  assertEquals(
    isResultMatchTarget(
      "#features-chrome/common/foo.test.ts",
      "browser-features/chrome/common/foo.test.ts",
    ),
    true,
  );
  assertEquals(
    isResultMatchTarget(
      "C:\\workspace\\browser-features\\modules\\x.test.mts",
      "browser-features/modules/x.test.mts",
    ),
    true,
  );
  assertEquals(
    isResultMatchTarget(
      "#/unknown/foo.test.ts",
      "browser-features/chrome/common/foo.test.ts",
    ),
    false,
  );
});
