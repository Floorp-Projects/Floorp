// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertStringIncludes } from "@std/assert";
import * as path from "@std/path";
import { triageFirefoxBrowserTests } from "./triage_firefox_browser_tests.ts";
import { createTestRuntimeLock } from "./test_runtime_lock.ts";

type FixtureFile = {
  path: string;
  source: string;
};

async function writeJson(filePath: string, value: unknown): Promise<void> {
  await Deno.mkdir(path.dirname(filePath), { recursive: true });
  await Deno.writeTextFile(filePath, `${JSON.stringify(value, null, 2)}\n`);
}

async function writeFixtureCollection(
  dir: string,
  files: FixtureFile[],
): Promise<string> {
  const collectionDir = path.join(dir, "collection");
  const collectionFiles = [];
  const candidates = [];

  for (const file of files) {
    const outputPath = `files/${file.path}`;
    const outputFilePath = path.join(collectionDir, ...outputPath.split("/"));
    await Deno.mkdir(path.dirname(outputFilePath), { recursive: true });
    await Deno.writeTextFile(outputFilePath, file.source);
    collectionFiles.push({
      path: file.path,
      outputPath,
      size: file.source.length,
      sha256: `${file.path}-sha`,
      harnesses: ["browser-chrome"],
      roles: ["candidate"],
    });
    candidates.push({
      path: file.path,
      directory: path.dirname(file.path).replaceAll("\\", "/"),
      nearestManifest: `${
        path.dirname(file.path).replaceAll("\\", "/")
      }/browser.toml`,
      hasHeadJs: true,
      supportFileCount: 2,
      size: file.source.length,
      sha256: `${file.path}-sha`,
    });
  }

  await writeJson(path.join(collectionDir, "manifest.json"), {
    schemaVersion: 1,
    mode: "candidate",
    source: {
      repository: "Floorp-Projects/Floorp-Runtime",
      ref: "fixture-ref",
      commit: "fixture-commit",
      tree: "fixture-tree",
    },
    files: collectionFiles,
  });
  await writeJson(
    path.join(collectionDir, "browser-chrome-candidates.json"),
    candidates,
  );
  return collectionDir;
}

function testByPath(
  tests: Awaited<ReturnType<typeof triageFirefoxBrowserTests>>["tests"],
  upstreamPath: string,
) {
  const entry = tests.find((test) => test.path === upstreamPath);
  if (!entry) {
    throw new Error(`Missing triage entry: ${upstreamPath}`);
  }
  return entry;
}

Deno.test("triageFirefoxBrowserTests classifies locked, quarantined, direct, runner-shim, and unsupported candidates", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const allowedPath = "browser/base/content/test/general/browser_allowed.js";
    const quarantinedPath =
      "browser/base/content/test/general/browser_bug484315.js";
    const directPath = "browser/base/content/test/general/browser_direct.js";
    const runnerShimPath =
      "browser/base/content/test/general/browser_runner_shim.js";
    const unsupportedPath =
      "browser/base/content/test/general/browser_unsupported.js";
    const collectionDir = await writeFixtureCollection(dir, [
      {
        path: allowedPath,
        source: "add_task(function allowed() { gURLBar.focus(); });\n",
      },
      {
        path: quarantinedPath,
        source: "add_task(function popup() { OpenBrowserWindow({}); });\n",
      },
      {
        path: directPath,
        source:
          "add_task(function direct() { ok(gBrowser.tabs.length >= 0); });\n",
      },
      {
        path: runnerShimPath,
        source:
          "add_task(async function runner() { await BrowserTestUtils.withNewTab('about:blank', async () => {}); EventUtils.synthesizeKey('x', {}); });\n",
      },
      {
        path: unsupportedPath,
        source:
          "add_task(async function unsupported() { await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {}); await BrowserTestUtils.waitForContentEvent(gBrowser.selectedBrowser, 'click'); });\n",
      },
    ]);
    const quarantinePath = path.join(dir, "quarantine.json");
    const outputDir = path.join(dir, "triage");
    await writeJson(quarantinePath, [
      {
        path: quarantinedPath,
        classification: "quarantined",
        reason: "popup window semantics are blocked in Floorp test profiles",
        requiredApis: ["OpenBrowserWindow"],
        sourceRef: "fixture-ref",
        lastObserved: "2026-06-30",
      },
    ]);

    const manifest = await triageFirefoxBrowserTests({
      collectionDir,
      quarantinePath,
      outputDir,
      runtimeLock: createTestRuntimeLock({
        tests: [{
          path: allowedPath,
          manifest: "browser/base/content/test/general/browser.toml",
          expectedTasks: 1,
          headPolicy: "harness-replaced",
          supportPolicy: "locked-not-loaded",
        }],
      }),
    });

    assertEquals(manifest.counts.candidates, 5);
    assertEquals(manifest.counts.classifications["already-allowed"], 1);
    assertEquals(manifest.counts.classifications.quarantined, 1);
    assertEquals(manifest.counts.classifications.direct, 1);
    assertEquals(manifest.counts.classifications["needs-runner-shim"], 1);
    assertEquals(manifest.counts.classifications.unsupported, 1);

    assertEquals(
      testByPath(manifest.tests, allowedPath).classification,
      "already-allowed",
    );
    assertEquals(
      testByPath(manifest.tests, quarantinedPath).classification,
      "quarantined",
    );
    assertEquals(
      testByPath(manifest.tests, directPath).classification,
      "direct",
    );
    assertEquals(
      testByPath(manifest.tests, runnerShimPath).classification,
      "needs-runner-shim",
    );
    assertEquals(
      testByPath(manifest.tests, unsupportedPath).classification,
      "unsupported",
    );

    const unsupported = testByPath(manifest.tests, unsupportedPath);
    assertEquals(
      unsupported.requiredApis.includes("SpecialPowers"),
      true,
    );
    assertEquals(
      unsupported.requiredApis.includes(
        "BrowserTestUtils.waitForContentEvent",
      ),
      true,
    );

    const triageJson = await Deno.readTextFile(
      path.join(outputDir, "triage.json"),
    );
    assertStringIncludes(triageJson, "fixture-commit");
    assertEquals(Object.hasOwn(JSON.parse(triageJson), "generatedAt"), false);
    const report = await Deno.readTextFile(path.join(outputDir, "TRIAGE.md"));
    assertStringIncludes(report, "Firefox Browser Test Triage");
    assertStringIncludes(report, unsupportedPath);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("triageFirefoxBrowserTests classifies an unknown BrowserTestUtils method as unsupported", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const upstreamPath =
      "browser/base/content/test/general/browser_unknown_browser_test_utils.js";
    const collectionDir = await writeFixtureCollection(dir, [{
      path: upstreamPath,
      source:
        "add_task(async function unknown() { await BrowserTestUtils.futureMethod(); });\n",
    }]);
    const quarantinePath = path.join(dir, "quarantine.json");
    await writeJson(quarantinePath, []);

    const manifest = await triageFirefoxBrowserTests({
      collectionDir,
      quarantinePath,
      outputDir: path.join(dir, "triage"),
      runtimeLock: createTestRuntimeLock(),
    });
    const unknown = testByPath(manifest.tests, upstreamPath);

    assertEquals(unknown.classification, "unsupported");
    assertEquals(unknown.detectedApis, ["BrowserTestUtils.futureMethod"]);
    assertEquals(unknown.requiredApis, ["BrowserTestUtils.futureMethod"]);
    assertEquals(unknown.reasons, [
      "uses an unsupported BrowserTestUtils method",
    ]);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("triageFirefoxBrowserTests rejects quarantine entries with missing required fields", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const upstreamPath = "browser/base/content/test/general/browser_popup.js";
    const collectionDir = await writeFixtureCollection(dir, [
      {
        path: upstreamPath,
        source: "add_task(function popup() { OpenBrowserWindow({}); });\n",
      },
    ]);
    const quarantinePath = path.join(dir, "quarantine.json");
    await writeJson(quarantinePath, [
      {
        path: upstreamPath,
        classification: "quarantined",
        requiredApis: ["OpenBrowserWindow"],
        sourceRef: "fixture-ref",
        lastObserved: "2026-06-30",
      },
    ]);

    await assertRejects(
      () =>
        triageFirefoxBrowserTests({
          collectionDir,
          quarantinePath,
          outputDir: path.join(dir, "triage"),
          runtimeLock: createTestRuntimeLock(),
        }),
      Error,
      "reason is required",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("triageFirefoxBrowserTests ignores quarantine paths absent from a scoped candidate collection", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const collectionDir = await writeFixtureCollection(dir, [
      {
        path: "browser/base/content/test/general/browser_direct.js",
        source: "add_task(function direct() { ok(true); });\n",
      },
    ]);
    const quarantinePath = path.join(dir, "quarantine.json");
    await writeJson(quarantinePath, [
      {
        path: "browser/base/content/test/general/browser_missing.js",
        classification: "quarantined",
        reason: "not in candidate manifest",
        requiredApis: ["OpenBrowserWindow"],
        sourceRef: "fixture-ref",
        lastObserved: "2026-06-30",
      },
    ]);

    const manifest = await triageFirefoxBrowserTests({
      collectionDir,
      quarantinePath,
      outputDir: path.join(dir, "triage"),
      runtimeLock: createTestRuntimeLock(),
    });
    assertEquals(manifest.counts.candidates, 1);
    assertEquals(manifest.counts.classifications.quarantined, 0);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("triageFirefoxBrowserTests rejects duplicate quarantine paths", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const upstreamPath = "browser/base/content/test/general/browser_popup.js";
    const collectionDir = await writeFixtureCollection(dir, [
      {
        path: upstreamPath,
        source: "add_task(function popup() { OpenBrowserWindow({}); });\n",
      },
    ]);
    const quarantinePath = path.join(dir, "quarantine.json");
    await writeJson(quarantinePath, [
      {
        path: upstreamPath,
        classification: "quarantined",
        reason: "first",
        requiredApis: ["OpenBrowserWindow"],
        sourceRef: "fixture-ref",
        lastObserved: "2026-06-30",
      },
      {
        path: upstreamPath,
        classification: "quarantined",
        reason: "second",
        requiredApis: ["OpenBrowserWindow"],
        sourceRef: "fixture-ref",
        lastObserved: "2026-06-30",
      },
    ]);

    await assertRejects(
      () =>
        triageFirefoxBrowserTests({
          collectionDir,
          quarantinePath,
          outputDir: path.join(dir, "triage"),
          runtimeLock: createTestRuntimeLock(),
        }),
      Error,
      "Duplicate quarantined Firefox test path",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});
