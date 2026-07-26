// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertStringIncludes } from "@std/assert";
import * as path from "@std/path";
import type { RuntimeMaterial } from "../src/runtime_lock.ts";
import { prepareFirefoxBrowserTests } from "./prepare_firefox_browser_tests.ts";
import { createTestRuntimeLock } from "./test_runtime_lock.ts";

const MANIFEST_PATH = "browser/example/test/browser.toml";
const TEST_PATH = "browser/example/test/browser_locked.js";
const HEAD_PATH = "browser/example/test/head.js";

async function shaHex(
  algorithm: "SHA-1" | "SHA-256",
  bytes: Uint8Array,
): Promise<string> {
  const input = new Uint8Array(bytes.byteLength);
  input.set(bytes);
  const digest = await crypto.subtle.digest(algorithm, input);
  return [...new Uint8Array(digest)]
    .map((byte) => byte.toString(16).padStart(2, "0"))
    .join("");
}

async function material(
  upstreamPath: string,
  role: RuntimeMaterial["role"],
  source: string,
): Promise<{ material: RuntimeMaterial; bytes: Uint8Array }> {
  const bytes = new TextEncoder().encode(source);
  const prefix = new TextEncoder().encode(`blob ${bytes.byteLength}\0`);
  const gitInput = new Uint8Array(prefix.byteLength + bytes.byteLength);
  gitInput.set(prefix);
  gitInput.set(bytes, prefix.byteLength);
  return {
    material: {
      path: upstreamPath,
      role,
      bytes: bytes.byteLength,
      mode: "100644",
      gitBlob: await shaHex("SHA-1", gitInput),
      sha256: await shaHex("SHA-256", bytes),
    },
    bytes,
  };
}

async function createFixture(mode: "candidate" | "locked" = "locked") {
  const dir = await Deno.makeTempDir();
  const collectionDir = path.join(dir, "collection");
  const outputDir = path.join(dir, "generated");
  const sources = [
    await material(MANIFEST_PATH, "manifest", "[DEFAULT]\n"),
    await material(
      TEST_PATH,
      "test",
      "add_task(() => {});\nadd_task(() => {});\n",
    ),
    await material(HEAD_PATH, "head-support", "const fixtureHead = true;\n"),
  ].sort((left, right) =>
    left.material.path.localeCompare(right.material.path)
  );
  const lock = createTestRuntimeLock({
    commit: "a".repeat(40),
    tree: "b".repeat(40),
    materials: sources.map((entry) => entry.material),
    tests: [{
      path: TEST_PATH,
      manifest: MANIFEST_PATH,
      expectedTasks: 2,
      headPolicy: "harness-replaced",
      supportPolicy: "locked-not-loaded",
    }],
    manifests: [{
      path: MANIFEST_PATH,
      preferences: [
        { name: "fixture.bool", type: "boolean", value: true },
        { name: "fixture.int", type: "integer", value: 7 },
        { name: "fixture.string", type: "string", value: "value" },
      ],
      supportPaths: [HEAD_PATH],
    }],
  });
  const files = [];
  for (const entry of sources) {
    const outputPath = `files/${entry.material.path}`;
    const filePath = path.join(collectionDir, ...outputPath.split("/"));
    await Deno.mkdir(path.dirname(filePath), { recursive: true });
    await Deno.writeFile(filePath, entry.bytes);
    files.push({
      path: entry.material.path,
      outputPath,
      size: entry.material.bytes,
      sha256: entry.material.sha256,
      mode: entry.material.mode,
      gitBlob: entry.material.gitBlob,
      roles: entry.material.role === "test"
        ? ["candidate", "test"]
        : [entry.material.role],
      harnesses: ["browser-chrome"],
    });
  }
  await Deno.writeTextFile(
    path.join(collectionDir, "manifest.json"),
    `${
      JSON.stringify(
        {
          schemaVersion: 1,
          mode,
          source: {
            repository: lock.source.repository,
            ref: mode === "locked" ? lock.source.ref : lock.source.trackingRef,
            commit: lock.source.commit,
            tree: lock.source.tree,
          },
          files,
        },
        null,
        2,
      )
    }\n`,
  );
  return { dir, collectionDir, outputDir, lock };
}

Deno.test("prepareFirefoxBrowserTests generates a lazy marker from the Runtime lock", async () => {
  const fixture = await createFixture();
  try {
    const manifest = await prepareFirefoxBrowserTests({
      collectionDir: fixture.collectionDir,
      outputDir: fixture.outputDir,
      runtimeLock: fixture.lock,
    });

    assertEquals(manifest.tests.length, 1);
    assertEquals(manifest.tests[0]?.upstreamPath, TEST_PATH);
    assertEquals(manifest.tests[0]?.expectedTaskCount, 2);
    assertEquals(manifest.tests[0]?.prefs, [
      { name: "fixture.bool", kind: "bool", value: true },
      { name: "fixture.int", kind: "int", value: 7 },
      { name: "fixture.string", kind: "string", value: "value" },
    ]);

    const wrapper = await Deno.readTextFile(
      path.join(fixture.outputDir, manifest.tests[0].wrapperPath),
    );
    assertStringIncludes(wrapper, "@colocated-env browser");
    assertStringIncludes(
      wrapper,
      "export const __NORA_DOWNLOADED_FIREFOX_TEST__ = {",
    );
    assertStringIncludes(wrapper, `upstreamPath: ${JSON.stringify(TEST_PATH)}`);
    assertStringIncludes(
      wrapper,
      `manifestPath: ${JSON.stringify(MANIFEST_PATH)}`,
    );
    assertStringIncludes(wrapper, "expectedTaskCount: 2");
    assertStringIncludes(wrapper, 'kind":"bool"');
    assertStringIncludes(wrapper, 'kind":"int"');
    assertStringIncludes(wrapper, 'kind":"string"');
    assertStringIncludes(wrapper, 'headPolicy: "harness-replaced"');
    assertStringIncludes(wrapper, 'supportPolicy: "locked-not-loaded"');
    assertStringIncludes(
      wrapper,
      `load: () => import(${JSON.stringify(`#firefox-tests/${TEST_PATH}`)})`,
    );
    assertEquals(/^import\s/m.test(wrapper), false);

    const generatedManifest = JSON.parse(
      await Deno.readTextFile(path.join(fixture.outputDir, "manifest.json")),
    );
    assertEquals(Object.hasOwn(generatedManifest, "generatedAt"), false);
    assertEquals(Object.hasOwn(generatedManifest, "allowlistPath"), false);
  } finally {
    await Deno.remove(fixture.dir, { recursive: true });
  }
});

Deno.test("prepareFirefoxBrowserTests rejects candidate collections", async () => {
  const fixture = await createFixture("candidate");
  try {
    await assertRejects(
      () =>
        prepareFirefoxBrowserTests({
          collectionDir: fixture.collectionDir,
          outputDir: fixture.outputDir,
          runtimeLock: fixture.lock,
        }),
      Error,
      "require a locked collection",
    );
  } finally {
    await Deno.remove(fixture.dir, { recursive: true });
  }
});

Deno.test("prepareFirefoxBrowserTests rejects extra and modified material", async () => {
  const extraFixture = await createFixture();
  try {
    const manifestPath = path.join(extraFixture.collectionDir, "manifest.json");
    const manifest = JSON.parse(await Deno.readTextFile(manifestPath));
    manifest.files.push({
      ...manifest.files[0],
      path: "browser/example/test/extra.js",
      outputPath: "files/browser/example/test/extra.js",
    });
    await Deno.writeTextFile(manifestPath, `${JSON.stringify(manifest)}\n`);
    await assertRejects(
      () =>
        prepareFirefoxBrowserTests({
          collectionDir: extraFixture.collectionDir,
          outputDir: extraFixture.outputDir,
          runtimeLock: extraFixture.lock,
        }),
      Error,
      "extra: browser/example/test/extra.js",
    );
  } finally {
    await Deno.remove(extraFixture.dir, { recursive: true });
  }

  const modifiedFixture = await createFixture();
  try {
    await Deno.writeTextFile(
      path.join(
        modifiedFixture.collectionDir,
        "files",
        ...TEST_PATH.split("/"),
      ),
      "tampered\n",
    );
    await assertRejects(
      () =>
        prepareFirefoxBrowserTests({
          collectionDir: modifiedFixture.collectionDir,
          outputDir: modifiedFixture.outputDir,
          runtimeLock: modifiedFixture.lock,
        }),
      Error,
      "content mismatch",
    );
  } finally {
    await Deno.remove(modifiedFixture.dir, { recursive: true });
  }
});
