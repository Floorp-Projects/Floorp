// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assert } from "@std/assert";
import { packageVersion } from "./builder.ts";

Deno.test("packageVersion returns a non-empty semver-like string", () => {
  const version = packageVersion();
  assert(typeof version === "string", "version should be a string");
  assert(version.length > 0, "version should not be empty");
  // Should look like a version (contains at least one dot)
  assert(version.includes("."), "version should contain a dot separator");
});

Deno.test("packageVersion returns consistent results on repeated calls", () => {
  const v1 = packageVersion();
  const v2 = packageVersion();
  assertEquals(v1, v2);
});
