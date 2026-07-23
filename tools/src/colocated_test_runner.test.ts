// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertThrows } from "@std/assert";
import {
  detectLayer,
  escapeRegExp,
  isResultMatchTarget,
  isTestFile,
  normalizeBrowserResultPath,
  parseLayer,
} from "./colocated_test_utils.ts";
import {
  clearBrowserTestControlPrefs,
  parseOptions,
  writeBrowserTestControlPrefs,
} from "./colocated_test_runner.ts";

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

Deno.test("browser test control is versioned, expiring, and cleared by owner", async () => {
  const root = await Deno.makeTempDir();
  try {
    const profileDir = `${root}/profile/test`;
    await Deno.mkdir(profileDir, { recursive: true });
    await Deno.writeTextFile(
      `${profileDir}/prefs.js`,
      'user_pref("floorp.unrelated", "keep");\n' +
        'user_pref("nora.tests.filter.9", "stale");\n',
    );

    const runId = "current-run";
    const expiresAtMs = 1_900_000_000_000;
    const filter = ["browser-features/chrome/test/current.test.ts"];
    writeBrowserTestControlPrefs(
      filter,
      runId,
      expiresAtMs,
      profileDir,
    );

    assertEquals(
      JSON.parse(
        await Deno.readTextFile(
          `${profileDir}/nora-tests-control.json`,
        ),
      ),
      { schemaVersion: 1, runId, expiresAtMs, filter },
    );
    const writtenPrefs = await Deno.readTextFile(`${profileDir}/prefs.js`);
    assertEquals(writtenPrefs.includes('user_pref("floorp.unrelated"'), true);
    assertEquals(writtenPrefs.includes(`user_pref("nora.tests.run_id"`), true);
    assertEquals(
      writtenPrefs.includes(`user_pref("nora.tests.filter.0"`),
      true,
    );

    clearBrowserTestControlPrefs("different-run", profileDir);
    assertEquals(
      (await Deno.stat(`${profileDir}/nora-tests-control.json`)).isFile,
      true,
    );

    clearBrowserTestControlPrefs(runId, profileDir);
    const cleanedPrefs = await Deno.readTextFile(`${profileDir}/prefs.js`);
    assertEquals(cleanedPrefs.includes('user_pref("floorp.unrelated"'), true);
    assertEquals(cleanedPrefs.includes("nora.tests."), false);
    await assertRejects(
      () => Deno.stat(`${profileDir}/nora-tests-control.json`),
      Deno.errors.NotFound,
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("host cleanup removes its control file even when prefs cleanup fails", async () => {
  const root = await Deno.makeTempDir();
  try {
    const profileDir = `${root}/profile/test`;
    await Deno.mkdir(`${profileDir}/prefs.js`, { recursive: true });
    await Deno.writeTextFile(
      `${profileDir}/nora-tests-control.json`,
      JSON.stringify({
        schemaVersion: 1,
        runId: "owned-run",
        expiresAtMs: 1_900_000_000_000,
        filter: [],
      }),
    );

    assertThrows(
      () => clearBrowserTestControlPrefs("owned-run", profileDir),
      Error,
      "cleanup was incomplete",
    );
    await assertRejects(
      () => Deno.stat(`${profileDir}/nora-tests-control.json`),
      Deno.errors.NotFound,
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});
