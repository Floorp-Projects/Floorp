// SPDX-License-Identifier: MPL-2.0

import {
  assert,
  assertEquals,
  assertRejects,
  assertStringIncludes,
  assertThrows,
} from "@std/assert";
import {
  MAX_SOURCE_BYTES,
  readSourceBuffer,
  sourceDigest,
} from "../../libs/test-inventory/source-files.ts";
import {
  parseTestSourceManifest,
  TEST_SOURCE_EXPORT,
  testSourceManifestFilename,
} from "../../libs/test-inventory/source-manifest.ts";
import { testSourceBindingPlugin } from "../../libs/test-inventory/source-vite-plugin.ts";
import type {
  SourceFileOperations,
  SourceFileStat,
} from "../../libs/test-inventory/source-types.ts";
import {
  buildBrowserInventoryPlan,
  reconcileBrowserInventoryResults,
} from "./browser_test_inventory.ts";
import { writeTestSourceManifest } from "./browser_test_sources.ts";
import { writeBrowserTestControlPrefs } from "./colocated_test_runner.ts";
import { viteServerArguments } from "./dev_server.ts";

const id = "browser-features/chrome/test/input.test.ts";
const original = 'export function runAllTests() { return "original"; }\n';

async function fixture(
  sourceBytes = new TextEncoder().encode(original),
  additionalSources: ReadonlyMap<string, string> = new Map(),
) {
  const root = await Deno.makeTempDir();
  const filename = root + "/" + id;
  const driver = root + "/bridge/loader-features/loader/test/index.ts";
  const profile = root + "/_dist/profile/test";
  await Deno.mkdir(root + "/browser-features/chrome/test", { recursive: true });
  await Deno.mkdir(root + "/bridge/loader-features/loader/test", {
    recursive: true,
  });
  await Deno.writeFile(filename, sourceBytes);
  await Deno.writeTextFile(driver, "export const driverFixture = true;\n");
  const hashes = new Map([[id, sourceDigest(sourceBytes)]]);
  for (const [extraId, code] of additionalSources) {
    const extraFile = root + "/" + extraId;
    await Deno.mkdir(extraFile.slice(0, extraFile.lastIndexOf("/")), {
      recursive: true,
    });
    await Deno.writeTextFile(extraFile, code);
    hashes.set(extraId, sourceDigest(new TextEncoder().encode(code)));
  }
  const metadata = {
    inventoryVersion: 1,
    origin: "floorp",
    executionKind: "development-browser",
    packagedStatus: "not-onboarded",
  } as const;
  const inventory = buildBrowserInventoryPlan(
    [...hashes.keys()],
    [...hashes.keys()],
    hashes,
    "a".repeat(64),
    new Map([...hashes.keys()].map((entryId) => [entryId, metadata])),
  );
  const runId = "source-host-fixture";
  const expiry = Date.now() + 60_000;
  const context = await writeTestSourceManifest(
    inventory,
    runId,
    expiry,
    profile,
    root + "/inventory.json",
  );
  writeBrowserTestControlPrefs(
    [id],
    runId,
    expiry,
    profile,
    context.manifestSha256,
  );
  return {
    root,
    filename,
    driver,
    profile,
    inventory,
    context,
    expiry,
    close: () => Deno.remove(root, { recursive: true }),
  };
}

function faultReader(openedPatch: Partial<SourceFileStat> = {}) {
  const bytes = new Uint8Array(16).fill(42);
  const pathStat = {
    isFile: () => true,
    isSymbolicLink: () => false,
    dev: 1,
    ino: 2,
    size: bytes.length,
    mtimeMs: 1,
    ctimeMs: 1,
  };
  const openedStat = { ...pathStat, ...openedPatch };
  const activity = { opened: 0, reads: 0, closed: 0, returnedBytes: 0 };
  const failures = {
    read: false,
    close: false,
    readError: undefined as unknown,
    closeError: undefined as unknown,
  };
  let offset = 0;
  const operations: SourceFileOperations = {
    lstat: () => Promise.resolve(pathStat),
    open: () => {
      activity.opened++;
      return Promise.resolve({
        stat: () => Promise.resolve(openedStat),
        read: (buffer) => {
          activity.reads++;
          if (failures.read) return Promise.reject(failures.readError);
          const length = Math.min(buffer.length, bytes.length - offset);
          buffer.set(bytes.subarray(offset, offset + length));
          offset += length;
          activity.returnedBytes += length;
          return Promise.resolve({ bytesRead: length });
        },
        close: () => {
          activity.closed++;
          return failures.close
            ? Promise.reject(failures.closeError)
            : Promise.resolve();
        },
      });
    },
  };
  return { pathStat, openedStat, activity, failures, operations };
}

Deno.test("source FD mismatch and nonregular descriptors fail before the first read and always close", async () => {
  for (const patch of [{ ino: 3 }, { dev: 3 }, { isFile: () => false }]) {
    const f = faultReader(patch);
    await assertRejects(() => readSourceBuffer("fixture", 32, f.operations));
    assertEquals(f.activity, {
      opened: 1,
      reads: 0,
      closed: 1,
      returnedBytes: 0,
    });
  }
});

Deno.test("missing or unsafe path and descriptor identities cannot authorize any bytes", async () => {
  for (const value of [0, -1, Number.NaN, Number.MAX_SAFE_INTEGER + 1]) {
    for (const field of ["dev", "ino"] as const) {
      const path = faultReader();
      path.pathStat[field] = value;
      await assertRejects(
        () => readSourceBuffer("fixture", 32, path.operations),
        Error,
        "safe file identity",
      );
      assertEquals(path.activity.opened, 0);
      const fd = faultReader({ [field]: value });
      await assertRejects(
        () => readSourceBuffer("fixture", 32, fd.operations),
        Error,
        "safe file identity",
      );
      assertEquals(fd.activity.reads, 0);
      assertEquals(fd.activity.closed, 1);
    }
  }
});

Deno.test("actual source growth is capped independently of reported size and close failures preserve the first failure", async () => {
  const growth = faultReader({ size: 1 });
  growth.pathStat.size = 1;
  growth.failures.close = true;
  growth.failures.closeError = new Error("secondary close failure");
  await assertRejects(
    () => readSourceBuffer("fixture", 8, growth.operations),
    Error,
    "read bound exceeded",
  );
  assertEquals(growth.activity.returnedBytes, 9);
  assertEquals(growth.activity.closed, 1);
  const read = faultReader();
  read.failures.read = true;
  read.failures.close = true;
  read.failures.closeError = new Error("secondary close failure");
  let rejected = false;
  try {
    await readSourceBuffer("fixture", 32, read.operations);
  } catch (error) {
    rejected = true;
    assertEquals(error, undefined);
  }
  assert(rejected, "Even an undefined first rejection must not be masked");
  assertEquals(read.activity.closed, 1);
  const close = faultReader();
  close.failures.close = true;
  close.failures.closeError = new Error("standalone close failure");
  await assertRejects(
    () => readSourceBuffer("fixture", 32, close.operations),
    Error,
    "standalone close failure",
  );
  assertEquals(close.activity.closed, 1);
});

async function load(
  plugin: ReturnType<typeof testSourceBindingPlugin>,
  filename: string,
) {
  const hook = typeof plugin.load === "function"
    ? plugin.load
    : plugin.load?.handler;
  assert(hook);
  const result = await hook.call({} as never, filename, { ssr: true });
  assert(result && typeof result === "object" && "code" in result);
  return result.code;
}

Deno.test("source buffer is bounded, closed, and hashes the exact returned bytes", async () => {
  const f = await fixture();
  try {
    const snapshot = await readSourceBuffer(f.filename);
    await Deno.writeTextFile(f.filename, "changed after read");
    assertEquals(new TextDecoder().decode(snapshot.bytes), original);
    assertEquals(
      snapshot.sha256,
      sourceDigest(new TextEncoder().encode(original)),
    );
    await Deno.remove(f.filename);
  } finally {
    await f.close();
  }
});

Deno.test("source reader refuses oversized, directory and redirected final components", async () => {
  const f = await fixture();
  try {
    await Deno.truncate(f.filename, MAX_SOURCE_BYTES + 1);
    await assertRejects(() => readSourceBuffer(f.filename), Error, "oversized");
    await assertRejects(
      () => readSourceBuffer(f.profile),
      Error,
      "ordinary file",
    );
    if (Deno.build.os !== "windows") {
      await Deno.symlink(f.filename, f.root + "/redirect.test.ts");
      await assertRejects(
        () => readSourceBuffer(f.root + "/redirect.test.ts"),
        Error,
        "ordinary file",
      );
    }
    await assertRejects(() => readSourceBuffer(f.filename, 0), Error, "bound");
  } finally {
    await f.close();
  }
});

Deno.test("source plan filenames and manifests reject traversal, expiry, duplicate and missing entries", async () => {
  const f = await fixture();
  try {
    const raw = JSON.parse(
      await Deno.readTextFile(f.root + "/inventory.json.source.json"),
    );
    assertEquals(parseTestSourceManifest(raw).runId, f.context.runId);
    for (
      const bad of [
        null,
        {},
        { ...raw, runId: "../other" },
        { ...raw, expiresAtMs: Date.now() - 1 },
        { ...raw, expiresAtMs: Date.now() + 1_900_000 },
        { ...raw, sources: [] },
        { ...raw, sources: [...raw.sources, raw.sources[0]] },
        { ...raw, sources: [{ id: "../secret", sha256: "a".repeat(64) }] },
        { ...raw, sources: [{ id, sha256: "" }] },
      ]
    ) {
      assertThrows(() => parseTestSourceManifest(bad));
    }
    assertThrows(() => testSourceManifestFilename("../other"));
    assertThrows(() => testSourceManifestFilename("a/b"));
  } finally {
    await f.close();
  }
});

Deno.test("source plans are append-only and preserve identical original evidence bytes", async () => {
  const f = await fixture();
  try {
    const originalPlan = await Deno.readFile(
      f.root + "/inventory.json.source.json",
    );
    assertEquals(sourceDigest(originalPlan), f.context.manifestSha256);
    assertEquals(
      await Deno.readFile(
        f.profile + "/" + testSourceManifestFilename(f.context.runId),
      ),
      originalPlan,
    );
    await assertRejects(
      () =>
        writeTestSourceManifest(
          f.inventory,
          f.context.runId,
          f.expiry,
          f.profile,
        ),
      Deno.errors.AlreadyExists,
    );
    assertEquals(
      await Deno.readFile(f.root + "/inventory.json.source.json"),
      originalPlan,
    );
  } finally {
    await f.close();
  }
});

Deno.test("collector rejects absent, foreign, stale and changed source proof", async () => {
  const f = await fixture();
  try {
    const entry = f.inventory.entries[0];
    const proof = { ...f.context, id, sourceSha256: entry.sourceSha256 };
    const result = {
      file: id,
      inventoryVersion: 1,
      origin: entry.origin,
      executionKind: entry.executionKind,
      sourceProof: proof,
    };
    assertEquals(
      reconcileBrowserInventoryResults(f.inventory, [result], f.context),
      [],
    );
    for (
      const patch of [
        undefined,
        { ...proof, id: "foreign" },
        { ...proof, runId: "other" },
        { ...proof, schemaVersion: 2 },
        { ...proof, sourceSha256: "b".repeat(64) },
        { ...proof, manifestSha256: "b".repeat(64) },
      ]
    ) {
      assert(
        reconcileBrowserInventoryResults(f.inventory, [{
          ...result,
          sourceProof: patch as typeof proof,
        }], f.context).length > 0,
      );
    }
  } finally {
    await f.close();
  }
});

Deno.test("real source load returns its hash-bound buffer and retains the first run proof", async () => {
  const f = await fixture();
  try {
    const plugin = testSourceBindingPlugin(f.root);
    const driver = await load(plugin, f.driver);
    assertStringIncludes(driver, f.context.manifestSha256);
    const code = await load(plugin, f.filename);
    assert(code.startsWith(original));
    assertStringIncludes(code, f.inventory.entries[0].sourceSha256);
    assertStringIncludes(code, TEST_SOURCE_EXPORT);
    await Deno.writeTextFile(
      f.profile + "/nora-tests-control.json",
      JSON.stringify({
        runId: "another",
        sourceManifestSha256: "0".repeat(64),
      }),
    );
    assertEquals(await load(plugin, f.filename), code);
    assertStringIncludes(await load(plugin, f.driver), f.context.runId);
  } finally {
    await f.close();
  }
});

Deno.test("source mutation between plan and load is refused before code is returned", async () => {
  const f = await fixture();
  try {
    const plugin = testSourceBindingPlugin(f.root);
    await load(plugin, f.driver);
    await Deno.writeTextFile(
      f.filename,
      "globalThis.unplannedSource = true;\n",
    );
    await assertRejects(
      () => load(plugin, f.filename),
      Error,
      "digest differs",
    );
  } finally {
    await f.close();
  }
});

Deno.test("source manifest corruption cannot produce a driver marker", async () => {
  const f = await fixture();
  try {
    await Deno.writeTextFile(
      f.profile + "/" + testSourceManifestFilename(f.context.runId),
      "{}\n",
    );
    await assertRejects(
      () => load(testSourceBindingPlugin(f.root), f.driver),
      Error,
      "digest differs",
    );
  } finally {
    await f.close();
  }
});

Deno.test("unlisted eager sources, external aliases and unknown queries never fall through", async () => {
  const f = await fixture();
  const outside = await Deno.makeTempDir();
  try {
    const plugin = testSourceBindingPlugin(f.root);
    await load(plugin, f.driver);
    const unlisted = f.root + "/browser-features/chrome/test/unlisted.test.ts";
    await Deno.writeTextFile(unlisted, original);
    await assertRejects(() => load(plugin, unlisted), Error, "digest differs");
    await assertRejects(
      () => load(plugin, f.filename + "?raw"),
      Error,
      "Unsupported query",
    );
    await assertRejects(
      () => load(plugin, f.filename + "?t=1?raw"),
      Error,
      "Unsupported query",
    );
    const external = outside + "/" + id;
    await Deno.mkdir(outside + "/browser-features/chrome/test", {
      recursive: true,
    });
    await Deno.writeTextFile(external, original);
    await assertRejects(() => load(plugin, external), Error, "outside");
    if (Deno.build.os !== "windows") {
      const link = f.root + "/bridge/loader-features/link-features-chrome";
      await Deno.symlink(f.root + "/browser-features/chrome", link);
      assertEquals(
        await load(plugin, link + "/test/input.test.ts"),
        await load(plugin, f.filename),
      );
    }
  } finally {
    await f.close();
    await Deno.remove(outside, { recursive: true });
  }
});

Deno.test("bootstrap server remains unqualified and cannot be retagged into a QA source server", async () => {
  const f = await fixture();
  try {
    const control = { purpose: "restart-bootstrap-isolation" };
    await Deno.writeTextFile(
      f.profile + "/nora-tests-control.json",
      JSON.stringify(control),
    );
    const plugin = testSourceBindingPlugin(f.root);
    assertEquals(
      await load(plugin, f.driver),
      "export const driverFixture = true;\n",
    );
    writeBrowserTestControlPrefs(
      [id],
      f.context.runId,
      f.expiry,
      f.profile,
      f.context.manifestSha256,
    );
    assertEquals(
      await load(plugin, f.driver),
      "export const driverFixture = true;\n",
    );
    await assertRejects(
      () => load(plugin, f.filename),
      Error,
      "bootstrap server",
    );
  } finally {
    await f.close();
  }
});

Deno.test({
  name:
    "logical entry A cannot redirect to another authorized entry B through a leaf or ancestor symlink",
  ignore: Deno.build.os === "windows",
  fn: async () => {
    const otherId = "browser-features/chrome/other/test/input.test.ts";
    const f = await fixture(
      undefined,
      new Map([[otherId, "export const other = true;\n"]]),
    );
    try {
      const plugin = testSourceBindingPlugin(f.root);
      await load(plugin, f.driver);
      // B is valid and independently in the same original manifest.
      assertStringIncludes(await load(plugin, f.root + "/" + otherId), otherId);
      await Deno.remove(f.filename);
      await Deno.symlink(f.root + "/" + otherId, f.filename);
      await assertRejects(
        () => load(plugin, f.filename),
        Error,
        "logical path is redirected",
      );
      await Deno.remove(f.filename);
      await Deno.remove(f.root + "/browser-features/chrome/test");
      await Deno.symlink(
        f.root + "/browser-features/chrome/other/test",
        f.root + "/browser-features/chrome/test",
      );
      await assertRejects(
        () => load(plugin, f.filename),
        Error,
        "logical path is redirected",
      );
    } finally {
      await f.close();
    }
  },
});

Deno.test("expired source run cannot supply another module", async () => {
  const f = await fixture();
  const now = Date.now;
  try {
    const plugin = testSourceBindingPlugin(f.root);
    await load(plugin, f.driver);
    Date.now = () => f.expiry;
    await assertRejects(() => load(plugin, f.filename), Error, "expired");
  } finally {
    Date.now = now;
    await f.close();
  }
});

Deno.test("test source mode is explicit on main 5181 and absent from ordinary/page server arguments", () => {
  assertEquals(
    viteServerArguments("main", { testSourceBinding: true }).slice(-2),
    ["--mode", "test"],
  );
  assert(!viteServerArguments("main").includes("--mode"));
  assert(
    !viteServerArguments("settings", { testSourceBinding: true }).includes(
      "--mode",
    ),
  );
});

Deno.test("source input binds UTF-8 BOM bytes and refuses invalid UTF-8", async () => {
  const bytes = new TextEncoder().encode(original);
  const bom = new Uint8Array(bytes.length + 3);
  bom.set([0xef, 0xbb, 0xbf]);
  bom.set(bytes, 3);
  const valid = await fixture(bom);
  try {
    const plugin = testSourceBindingPlugin(valid.root);
    await load(plugin, valid.driver);
    const code = await load(plugin, valid.filename);
    assert(code.startsWith(original));
    assertStringIncludes(code, sourceDigest(bom));
  } finally {
    await valid.close();
  }
  const invalid = await fixture(new Uint8Array([0xff, 0xfe, 0xff]));
  try {
    const plugin = testSourceBindingPlugin(invalid.root);
    await load(plugin, invalid.driver);
    await assertRejects(() => load(plugin, invalid.filename), TypeError);
  } finally {
    await invalid.close();
  }
});

Deno.test("expiry between pre-read and post-read checks prevents code delivery", async () => {
  const f = await fixture();
  const now = Date.now;
  try {
    const plugin = testSourceBindingPlugin(f.root);
    await load(plugin, f.driver);
    let checks = 0;
    Date.now = () => ++checks === 1 ? f.expiry - 1 : f.expiry;
    await assertRejects(() => load(plugin, f.filename), Error, "expired");
    assert(checks >= 2);
  } finally {
    Date.now = now;
    await f.close();
  }
});
