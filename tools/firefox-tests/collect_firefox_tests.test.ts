// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertStringIncludes } from "@std/assert";
import * as path from "@std/path";
import { collectFirefoxTests, main } from "./collect_firefox_tests.ts";
import { createTestRuntimeLock } from "./test_runtime_lock.ts";
import type { RuntimeLock, RuntimeMaterial } from "../src/runtime_lock.ts";

async function runCommand(
  cwd: string,
  command: string,
  args: string[],
): Promise<void> {
  const result = await new Deno.Command(command, {
    args,
    cwd,
    stdout: "piped",
    stderr: "piped",
  }).output();
  if (!result.success) {
    const stderr = new TextDecoder().decode(result.stderr);
    throw new Error(`${command} ${args.join(" ")} failed: ${stderr}`);
  }
}

async function runCommandBytes(
  cwd: string,
  command: string,
  args: string[],
): Promise<Uint8Array> {
  const result = await new Deno.Command(command, {
    args,
    cwd,
    stdout: "piped",
    stderr: "piped",
  }).output();
  if (!result.success) {
    const stderr = new TextDecoder().decode(result.stderr);
    throw new Error(`${command} ${args.join(" ")} failed: ${stderr}`);
  }
  return result.stdout;
}

async function runCommandText(
  cwd: string,
  command: string,
  args: string[],
): Promise<string> {
  return new TextDecoder().decode(await runCommandBytes(cwd, command, args))
    .trim();
}

async function sha256Hex(bytes: Uint8Array): Promise<string> {
  const input = new Uint8Array(bytes.byteLength);
  input.set(bytes);
  const digest = await crypto.subtle.digest("SHA-256", input);
  return [...new Uint8Array(digest)]
    .map((byte) => byte.toString(16).padStart(2, "0"))
    .join("");
}

async function writeFixtureFile(
  root: string,
  relativePath: string,
  text: string,
): Promise<void> {
  const filePath = path.join(root, ...relativePath.split("/"));
  await Deno.mkdir(path.dirname(filePath), { recursive: true });
  await Deno.writeTextFile(filePath, text);
}

async function writeFixtureSymlink(
  root: string,
  relativePath: string,
  target: string,
): Promise<void> {
  const filePath = path.join(root, ...relativePath.split("/"));
  await Deno.mkdir(path.dirname(filePath), { recursive: true });
  await Deno.symlink(target, filePath, { type: "file" });
}

async function createRuntimeFixture(): Promise<string> {
  const dir = await Deno.makeTempDir();
  await runCommand(dir, "git", ["init", "-q"]);
  await writeFixtureFile(
    dir,
    "browser/base/content/test/general/browser.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/test/general/browser_bug537474.js",
    "add_task(async function test_bug() {});\n",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/test/general/head.js",
    "const HEAD = true;\n",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/test/general/support.html",
    "<!doctype html>\n",
  );
  await writeFixtureSymlink(
    dir,
    "browser/base/content/test/general/external-link.txt",
    "/etc/passwd",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/test/chrome/chrome.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/test/chrome/chrome_window.js",
    "add_task(async function test_chrome() {});\n",
  );
  await writeFixtureFile(
    dir,
    "browser/components/customizableui/test/browser-common.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "browser/components/customizableui/test/browser_toolbar.js",
    "add_task(async function test_toolbar() {});\n",
  );
  await writeFixtureFile(
    dir,
    "dom/base/crashtests/crashtests.list",
    "load 123.html\n",
  );
  await writeFixtureFile(
    dir,
    "dom/base/crashtests/123.html",
    "<!doctype html>\n",
  );
  await writeFixtureFile(
    dir,
    "dom/base/test/mochitest.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "dom/base/test/test_parent.html",
    "<!doctype html>\n",
  );
  await writeFixtureFile(
    dir,
    "dom/base/test/reftest/reftest.list",
    "== reftest_child.html reftest_child.html\n",
  );
  await writeFixtureFile(
    dir,
    "dom/base/test/reftest/reftest_child.html",
    "<!doctype html>\n",
  );
  await writeFixtureFile(
    dir,
    "testing/web-platform/tests/url/url-origin.any.js",
    "test(() => {}, 'origin');\n",
  );
  await writeFixtureFile(
    dir,
    "testing/web-platform/meta/url/url-origin.any.js.ini",
    "[url-origin.any.html]\n",
  );
  await writeFixtureFile(
    dir,
    "browser/components/search/test/marionette/manifest.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "browser/components/search/test/marionette/test_search.py",
    "def test_search():\n    pass\n",
  );
  await writeFixtureFile(
    dir,
    "testing/firefox-ui/tests/functional/manifest.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "testing/firefox-ui/tests/functional/test_security.py",
    "def test_security():\n    pass\n",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/test/perftest.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/test/browser_startup.js",
    "add_task(async function test_startup() {});\n",
  );
  await writeFixtureFile(
    dir,
    "toolkit/components/ml/tests/browser_eval/eval.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "toolkit/components/ml/tests/browser_eval/browser_eval.js",
    "add_task(async function test_eval() {});\n",
  );
  await writeFixtureFile(
    dir,
    "testing/mozbase/mozlog/tests/manifest.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "testing/mozbase/mozlog/tests/test_logger.py",
    "def test_logger():\n    pass\n",
  );
  await writeFixtureFile(
    dir,
    "python/mozbuild/mozbuild/test/python.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "python/mozbuild/mozbuild/test/test_backend.py",
    "def test_backend():\n    pass\n",
  );
  await writeFixtureFile(
    dir,
    "js/src/tests/jstests.list",
    "script non262/test.js\n",
  );
  await writeFixtureFile(
    dir,
    "js/src/tests/non262/test.js",
    "assertEq(1, 1);\n",
  );
  await writeFixtureFile(
    dir,
    "testing/web-platform/wptrunner.ini",
    "[wptrunner]\n",
  );
  await writeFixtureFile(
    dir,
    "testing/web-platform/docs/index.rst",
    "not a collected test root\n",
  );
  await writeFixtureFile(
    dir,
    "toolkit/components/example/tests/xpcshell/xpcshell.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "toolkit/components/example/tests/xpcshell/test_example.js",
    "function run_test() {}\n",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/not-a-test/browser_outside.js",
    "not collected\n",
  );
  await runCommand(dir, "git", ["add", "."]);
  await runCommand(dir, "git", [
    "-c",
    "user.name=Floorp Test",
    "-c",
    "user.email=floorp-test@example.invalid",
    "commit",
    "-q",
    "-m",
    "fixture",
  ]);
  await runCommand(dir, "git", ["branch", "fixture-ref"]);
  return dir;
}

async function createLockedRuntimeFixture(): Promise<{
  runtimeDir: string;
  lock: RuntimeLock;
}> {
  const runtimeDir = await createRuntimeFixture();
  const manifestPath = "browser/base/content/test/general/browser.toml";
  const testPath = "browser/base/content/test/general/browser_bug537474.js";
  const headPath = "browser/base/content/test/general/head.js";
  const selectedPaths = [manifestPath, testPath, headPath].sort();
  const treeLines = (await runCommandText(runtimeDir, "git", [
    "ls-tree",
    "-r",
    "-l",
    "--full-tree",
    "HEAD",
  ])).split("\n");
  const treeByPath = new Map<string, {
    mode: "100644";
    gitBlob: string;
    bytes: number;
  }>();
  for (const line of treeLines) {
    const match = line.match(
      /^(\d+)\s+blob\s+([0-9a-f]+)\s+(\d+)\t(.+)$/,
    );
    if (match && selectedPaths.includes(match[4])) {
      treeByPath.set(match[4], {
        mode: match[1] as "100644",
        gitBlob: match[2],
        bytes: Number(match[3]),
      });
    }
  }
  const materials: RuntimeMaterial[] = [];
  for (const upstreamPath of selectedPaths) {
    const treeEntry = treeByPath.get(upstreamPath);
    if (!treeEntry) {
      throw new Error(`Missing fixture tree entry: ${upstreamPath}`);
    }
    const bytes = await runCommandBytes(runtimeDir, "git", [
      "show",
      `HEAD:${upstreamPath}`,
    ]);
    materials.push({
      path: upstreamPath,
      role: upstreamPath === testPath
        ? "test"
        : upstreamPath === manifestPath
        ? "manifest"
        : "head-support",
      bytes: treeEntry.bytes,
      mode: treeEntry.mode,
      gitBlob: treeEntry.gitBlob,
      sha256: await sha256Hex(bytes),
    });
  }
  const commit = await runCommandText(runtimeDir, "git", ["rev-parse", "HEAD"]);
  const tree = await runCommandText(runtimeDir, "git", [
    "rev-parse",
    "HEAD^{tree}",
  ]);
  return {
    runtimeDir,
    lock: createTestRuntimeLock({
      commit,
      tree,
      materials,
      tests: [{
        path: testPath,
        manifest: manifestPath,
        expectedTasks: 1,
        headPolicy: "harness-replaced",
        supportPolicy: "locked-not-loaded",
      }],
      manifests: [{
        path: manifestPath,
        preferences: [],
        supportPaths: [headPath],
      }],
    }),
  };
}

Deno.test("candidate collection is bounded to the locked browser-chrome projection", async () => {
  const { runtimeDir, lock } = await createLockedRuntimeFixture();
  const outputDir = await Deno.makeTempDir();
  try {
    const manifest = await collectFirefoxTests({
      runtimeDir,
      outputDir,
      sourceRepo: lock.source.repository,
      sourceRef: lock.source.trackingRef,
      candidate: true,
      runtimeLock: lock,
    });

    assertEquals(manifest.scope, "locked-closure");
    assertEquals(manifest.source.ref, "fixture-ref");
    assertEquals(manifest.counts.roots, 1);
    assertEquals(manifest.counts.files, 3);
    assertEquals(manifest.counts.candidates, 1);
    assertEquals(
      manifest.files.map((file) => file.path).sort((left, right) =>
        left.localeCompare(right)
      ),
      lock.source.materials.entries.map((entry) => entry.path).sort((
        left,
        right,
      ) => left.localeCompare(right)),
    );
    assertEquals(
      manifest.files.some((file) =>
        file.path ===
          "browser/base/content/test/general/browser_bug537474.js" &&
        file.roles.includes("candidate")
      ),
      true,
    );
    assertEquals(
      manifest.files.some((file) =>
        file.path === "browser/base/content/test/chrome/chrome_window.js"
      ),
      false,
    );
    assertEquals(
      manifest.files.some((file) =>
        file.path ===
          "toolkit/components/example/tests/xpcshell/test_example.js"
      ),
      false,
    );

    const copied = await Deno.readTextFile(
      path.join(
        outputDir,
        "files",
        "browser",
        "base",
        "content",
        "test",
        "general",
        "browser_bug537474.js",
      ),
    );
    assertStringIncludes(copied, "add_task");

    const candidates = await Deno.readTextFile(
      path.join(outputDir, "browser-chrome-candidates.json"),
    );
    assertStringIncludes(candidates, "browser_bug537474.js");

    const summary = await Deno.readTextFile(path.join(outputDir, "SUMMARY.md"));
    assertStringIncludes(summary, "Browser chrome candidates: 1");
  } finally {
    await Deno.remove(runtimeDir, { recursive: true });
    await Deno.remove(outputDir, { recursive: true });
  }
});

Deno.test("collectFirefoxTests path prefix limits roots and copied files", async () => {
  const { runtimeDir, lock } = await createLockedRuntimeFixture();
  const outputDir = await Deno.makeTempDir();
  try {
    const manifest = await collectFirefoxTests({
      runtimeDir,
      outputDir,
      sourceRepo: lock.source.repository,
      sourceRef: lock.source.trackingRef,
      pathPrefix: "browser/base/content/test/general/",
      candidate: true,
      runtimeLock: lock,
    });

    assertEquals(
      manifest.filters.pathPrefix,
      "browser/base/content/test/general",
    );
    assertEquals(manifest.counts.roots, 1);
    assertEquals(manifest.counts.candidates, 1);
    assertEquals(
      manifest.files.every((file) =>
        file.path.startsWith("browser/base/content/test/general/")
      ),
      true,
    );
    assertEquals(
      manifest.files.some((file) =>
        file.path === "browser/base/content/test/chrome/chrome_window.js"
      ),
      false,
    );
  } finally {
    await Deno.remove(runtimeDir, { recursive: true });
    await Deno.remove(outputDir, { recursive: true });
  }
});

Deno.test("collectFirefoxTests materializes only an exact locked closure", async () => {
  const { runtimeDir, lock } = await createLockedRuntimeFixture();
  const outputDir = await Deno.makeTempDir();
  try {
    const manifest = await collectFirefoxTests({
      runtimeDir,
      outputDir,
      sourceRepo: lock.source.repository,
      sourceRef: lock.source.ref,
      runtimeLock: lock,
    });

    assertEquals(manifest.mode, "locked");
    assertEquals(manifest.source.commit, lock.source.commit);
    assertEquals(manifest.source.tree, lock.source.tree);
    assertEquals(manifest.files.length, 3);
    assertEquals(manifest.counts.candidates, 1);
    assertEquals(
      manifest.files.map((file) => file.path),
      lock.source.materials.entries.map((entry) => entry.path),
    );
    assertEquals(
      Object.hasOwn(
        JSON.parse(
          await Deno.readTextFile(path.join(outputDir, "manifest.json")),
        ),
        "generatedAt",
      ),
      false,
    );
  } finally {
    await Deno.remove(runtimeDir, { recursive: true });
    await Deno.remove(outputDir, { recursive: true });
  }
});

Deno.test("collectFirefoxTests rejects locked material identity drift", async () => {
  const { runtimeDir, lock } = await createLockedRuntimeFixture();
  const outputDir = await Deno.makeTempDir();
  try {
    const mutations: Array<{
      label: string;
      expected: string;
      mutate: (candidate: RuntimeLock) => void;
    }> = [
      {
        label: "mode",
        expected: "mode mismatch",
        mutate(candidate) {
          candidate.source.materials.entries[0].mode = "100755" as "100644";
        },
      },
      {
        label: "Git blob",
        expected: "Git blob mismatch",
        mutate(candidate) {
          candidate.source.materials.entries[0].gitBlob = "0".repeat(40);
        },
      },
      {
        label: "size",
        expected: "size mismatch",
        mutate(candidate) {
          candidate.source.materials.entries[0].bytes += 1;
        },
      },
      {
        label: "SHA-256",
        expected: "content mismatch",
        mutate(candidate) {
          candidate.source.materials.entries[0].sha256 = "0".repeat(64);
        },
      },
      {
        label: "missing",
        expected: "is missing",
        mutate(candidate) {
          candidate.source.materials.entries[0].path =
            "browser/base/content/test/general/missing.toml";
        },
      },
    ];

    for (const mutation of mutations) {
      const candidate = structuredClone(lock);
      mutation.mutate(candidate);
      await assertRejects(
        () =>
          collectFirefoxTests({
            runtimeDir,
            outputDir,
            sourceRepo: candidate.source.repository,
            sourceRef: candidate.source.ref,
            runtimeLock: candidate,
          }),
        Error,
        mutation.expected,
        mutation.label,
      );
    }
  } finally {
    await Deno.remove(runtimeDir, { recursive: true });
    await Deno.remove(outputDir, { recursive: true });
  }
});

Deno.test("collectFirefoxTests requires an explicit tracking ref in candidate mode", async () => {
  const runtimeDir = await createRuntimeFixture();
  const outputDir = await Deno.makeTempDir();
  try {
    await assertRejects(
      () =>
        collectFirefoxTests({
          runtimeDir,
          outputDir,
          sourceRepo: "Floorp-Projects/Floorp-Runtime",
          candidate: true,
          runtimeLock: createTestRuntimeLock(),
        }),
      Error,
      "Candidate collection requires --source-ref fixture-ref",
    );
    await assertRejects(
      () =>
        collectFirefoxTests({
          runtimeDir,
          outputDir,
          sourceRepo: "Floorp-Projects/Floorp-Runtime",
          sourceRef: "wrong-ref",
          candidate: true,
          runtimeLock: createTestRuntimeLock(),
        }),
      Error,
      "Candidate collection must use tracking ref fixture-ref",
    );
  } finally {
    await Deno.remove(runtimeDir, { recursive: true });
    await Deno.remove(outputDir, { recursive: true });
  }
});

Deno.test("collector CLI rejects the removed --scope option", async () => {
  await assertRejects(
    () => main(["--scope", "browser-chrome"]),
    Error,
    "Unknown option: --scope",
  );
});

Deno.test("candidate collection verifies HEAD against the tracking ref", async () => {
  const runtimeDir = await createRuntimeFixture();
  const outputDir = await Deno.makeTempDir();
  try {
    await writeFixtureFile(runtimeDir, "candidate-drift.txt", "new commit\n");
    await runCommand(runtimeDir, "git", ["add", "candidate-drift.txt"]);
    await runCommand(runtimeDir, "git", [
      "-c",
      "user.name=Floorp Test",
      "-c",
      "user.email=floorp-test@example.invalid",
      "commit",
      "-q",
      "-m",
      "candidate drift",
    ]);

    await assertRejects(
      () =>
        collectFirefoxTests({
          runtimeDir,
          outputDir,
          sourceRepo: "Floorp-Projects/Floorp-Runtime",
          sourceRef: "fixture-ref",
          candidate: true,
          runtimeLock: createTestRuntimeLock(),
        }),
      Error,
      "Candidate Runtime checkout HEAD mismatch for fixture-ref",
    );
  } finally {
    await Deno.remove(runtimeDir, { recursive: true });
    await Deno.remove(outputDir, { recursive: true });
  }
});
