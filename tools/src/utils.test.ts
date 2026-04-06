// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assert } from "@std/assert";
import {
  resolveFromRoot,
  platform,
  arch,
  exists,
  PROJECT_ROOT,
} from "./utils.ts";
import * as path from "@std/path";

Deno.test("resolveFromRoot returns absolute path under PROJECT_ROOT", () => {
  const result = resolveFromRoot("deno.json");
  assert(path.isAbsolute(result), "should be absolute");
  assert(result.endsWith("deno.json"), "should end with deno.json");
});

Deno.test("resolveFromRoot with nested path", () => {
  const result = resolveFromRoot("tools/src/utils.ts");
  assert(result.includes("tools"), "should contain tools");
  assert(result.includes("utils.ts"), "should contain utils.ts");
});

Deno.test("platform returns a valid value", () => {
  const p = platform();
  assert(
    ["windows", "darwin", "linux"].includes(p),
    `platform should be one of windows/darwin/linux, got: ${p}`,
  );
});

Deno.test("arch returns a valid value", () => {
  const a = arch();
  assert(
    ["aarch64", "x86_64"].includes(a),
    `arch should be aarch64 or x86_64, got: ${a}`,
  );
});

Deno.test("exists returns true for existing file", () => {
  assertEquals(
    exists(path.join(PROJECT_ROOT, "deno.json")),
    true,
    "deno.json should exist",
  );
});

Deno.test("exists returns false for non-existing file", () => {
  assertEquals(
    exists(path.join(PROJECT_ROOT, "non-existent-file-12345.txt")),
    false,
    "non-existent file should return false",
  );
});

Deno.test("PROJECT_ROOT is a valid directory", () => {
  assert(exists(PROJECT_ROOT), "PROJECT_ROOT should exist");
});
