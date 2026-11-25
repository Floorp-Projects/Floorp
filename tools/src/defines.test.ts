// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";
import * as path from "@std/path";
import { PROJECT_ROOT, resolveTestProfilePath } from "./defines.ts";

Deno.test("test profile resolver defaults without an override", () => {
  assertEquals(
    resolveTestProfilePath(),
    path.join(PROJECT_ROOT, "_dist", "profile", "test"),
  );
});

Deno.test("test profile resolver accepts and normalizes absolute overrides", () => {
  const profilePath = `${PROJECT_ROOT}/_dist/fdh/profiles/../isolated`;
  assertEquals(
    resolveTestProfilePath(profilePath),
    path.normalize(profilePath),
  );
});

Deno.test("test profile resolver rejects relative overrides", () => {
  assertThrows(
    () => resolveTestProfilePath("relative-profile"),
    Error,
    "must be absolute",
  );
});
