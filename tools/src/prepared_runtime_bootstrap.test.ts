// SPDX-License-Identifier: MPL-2.0
import {
  assert,
  assertEquals,
  assertRejects,
  assertStringIncludes,
} from "@std/assert";
import * as path from "@std/path";
import { prepareSyntheticRuntimeBootstrap as prepareRuntimeBootstrap } from "./prepared_runtime_bootstrap_test_support.ts";

const relative = "moz-src/browser/components/BrowserGlue.sys.mjs";
const raw = [
  'import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";',
  'import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";',
  "",
  "const lazy = {};",
  "",
  "ChromeUtils.defineESModuleGetters(lazy, {",
  "});",
  "",
].join(String.fromCharCode(10));
async function fixture(
  run: (root: string, target: string) => Promise<void>,
): Promise<void> {
  const root = Deno.makeTempDirSync({ prefix: "fdh-bootstrap-contract-" });
  const target = path.join(root, relative);
  Deno.mkdirSync(path.dirname(target), { recursive: true });
  Deno.writeTextFileSync(target, raw);
  try {
    await run(root, target);
  } finally {
    Deno.removeSync(root, { recursive: true });
  }
}
Deno.test("bootstrap applies only the trusted hook and is byte-idempotent", () =>
  fixture(async (root, target) => {
    const first = await prepareRuntimeBootstrap(root);
    assertEquals(first.status, "applied");
    assertEquals(first.targetRelativePath, relative);
    assert(/^[0-9a-f]{64}$/.test(first.patchSha256));
    assert(/^[0-9a-f]{64}$/.test(first.targetSha256));
    const patched = Deno.readTextFileSync(target);
    assertStringIncludes(patched, "/*@ NORA INJECT START @*/");
    assertEquals(
      patched.split("resource://noraneko/modules/NoranekoStartup.sys.mjs")
        .length,
      2,
    );
    const second = await prepareRuntimeBootstrap(root);
    assertEquals(second.status, "already");
    assertEquals(second.patchSha256, first.patchSha256);
    assertEquals(second.targetSha256, first.targetSha256);
    assertEquals(Deno.readTextFileSync(target), patched);
  }));
Deno.test("bootstrap rejects incomplete forged and duplicate hooks without rewriting", () =>
  fixture(async (root, target) => {
    for (
      const marker of [
        "/*@ NORA INJECT START @*/",
        "/*@ NORA INJECT END @*/",
        "resource://noraneko/modules/NoranekoStartup.sys.mjs",
      ]
    ) {
      const original = raw + marker;
      Deno.writeTextFileSync(target, original);
      await assertRejects(
        () => prepareRuntimeBootstrap(root),
        Error,
        "conflicting or incomplete",
      );
      assertEquals(Deno.readTextFileSync(target), original);
    }
    Deno.writeTextFileSync(target, raw);
    await prepareRuntimeBootstrap(root);
    const duplicate = Deno.readTextFileSync(target).repeat(2);
    Deno.writeTextFileSync(target, duplicate);
    await assertRejects(
      () => prepareRuntimeBootstrap(root),
      Error,
      "conflicting or incomplete",
    );
    assertEquals(Deno.readTextFileSync(target), duplicate);
  }));
Deno.test("bootstrap rejects incompatible engine context before modifying it", () =>
  fixture(async (root, target) => {
    const incompatible = "export const unrelatedEngine = true;";
    Deno.writeTextFileSync(target, incompatible);
    await assertRejects(
      () => prepareRuntimeBootstrap(root),
      Error,
      "does not apply cleanly",
    );
    assertEquals(Deno.readTextFileSync(target), incompatible);
  }));
Deno.test("bootstrap rejects oversized and invalid UTF8 input before patching", () =>
  fixture(async (root, target) => {
    const oversized = new Uint8Array(2 * 1024 * 1024 + 1).fill(65);
    Deno.writeFileSync(target, oversized);
    await assertRejects(
      () => prepareRuntimeBootstrap(root),
      Error,
      "bounded regular",
    );
    assertEquals(Deno.statSync(target).size, oversized.length);
    Deno.writeFileSync(target, new Uint8Array([255, 254]));
    await assertRejects(() => prepareRuntimeBootstrap(root));
    assertEquals([...Deno.readFileSync(target)], [255, 254]);
  }));
Deno.test("bootstrap refuses a parent directory link without touching its target", () =>
  fixture(async (root, target) => {
    const aliased = path.join(root, "alias");
    Deno.mkdirSync(aliased);
    Deno.symlinkSync(
      path.join(root, "moz-src"),
      path.join(aliased, "moz-src"),
      { type: Deno.build.os === "windows" ? "junction" : "dir" },
    );
    await assertRejects(
      () => prepareRuntimeBootstrap(aliased),
      Error,
      "aliased paths",
    );
    assertEquals(Deno.readTextFileSync(target), raw);
  }));
Deno.test("bootstrap does not manufacture a missing engine file", () =>
  fixture(async (root, target) => {
    Deno.removeSync(target);
    await assertRejects(
      () => prepareRuntimeBootstrap(root),
      Deno.errors.NotFound,
    );
    assertEquals([...Deno.readDirSync(path.dirname(target))].length, 0);
  }));
Deno.test("bootstrap digest describes actual UTF8 bytes including an existing BOM", () =>
  fixture(async (root, target) => {
    await prepareRuntimeBootstrap(root);
    const bytes = new Uint8Array([239, 187, 191, ...Deno.readFileSync(target)]);
    Deno.writeFileSync(target, bytes);
    const result = await prepareRuntimeBootstrap(root);
    const expected = Array.from(
      new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)),
      (byte) => byte.toString(16).padStart(2, "0"),
    ).join("");
    assertEquals(result.status, "already");
    assertEquals(result.targetSha256, expected);
    assertEquals([...Deno.readFileSync(target)], [...bytes]);
  }));
