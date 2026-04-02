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
import { parseOptions } from "./colocated_test_runner.ts";

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

Deno.test("parseOptions applies default timeout values", () => {
  const options = parseOptions([]);

  assertEquals(options.near, undefined);
  assertEquals(options.listOnly, false);
  assertEquals(options.layer, "all");
  assertEquals(options.autoStart, true);
  assertEquals(options.timeoutMs, 1_800_000);
  assertEquals(options.startupTimeoutMs, 1_800_000);
  assertEquals(options.help, false);
});

Deno.test("parseOptions accepts explicit timeout and startup values", () => {
  const options = parseOptions([
    "browser-features/chrome",
    "--layer",
    "chrome",
    "--timeout-ms",
    "450000",
    "--startup-timeout-ms=360000",
  ]);

  assertEquals(options.near, "browser-features/chrome");
  assertEquals(options.layer, "chrome");
  assertEquals(options.timeoutMs, 450_000);
  assertEquals(options.startupTimeoutMs, 360_000);
});

Deno.test("parseOptions rejects unknown options", () => {
  assertThrows(
    () => parseOptions(["--timeout", "100"]),
    Error,
    "Unknown option: --timeout",
  );
});

Deno.test("parseOptions rejects invalid timeout values", () => {
  assertThrows(
    () => parseOptions(["--timeout-ms", "abc"]),
    Error,
    "Invalid --timeout-ms value",
  );

  assertThrows(
    () => parseOptions(["--startup-timeout-ms", "0"]),
    Error,
    "Invalid --startup-timeout-ms value",
  );

  assertThrows(
    () => parseOptions(["--timeout-ms", "1800001"]),
    Error,
    "Invalid --timeout-ms value",
  );
});

Deno.test("parseOptions rejects --near with positional path", () => {
  assertThrows(
    () => parseOptions(["foo", "--near", "bar"]),
    Error,
    "Use either --near or a positional path",
  );
});

Deno.test("parseOptions supports help mode", () => {
  const options = parseOptions(["--help"]);
  assertEquals(options.help, true);
  assertEquals(options.timeoutMs, 1_800_000);
  assertEquals(options.startupTimeoutMs, 1_800_000);
});
