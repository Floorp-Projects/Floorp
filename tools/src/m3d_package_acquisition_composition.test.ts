// SPDX-License-Identifier: MPL-2.0
/// <reference lib="deno.ns" />

import { assertEquals } from "@std/assert";
import { m3dCompositionNativeScriptsForTests } from "./m3d_package_acquisition_composition.ts";

Deno.test("generated composition scripts compile as async browser functions", () => {
  const AsyncFunction = Object.getPrototypeOf(async function () {})
    .constructor as new (...parameters: string[]) => unknown;
  const scripts = m3dCompositionNativeScriptsForTests();
  const entries = Object.entries(scripts);
  assertEquals(entries.length, 6);
  for (const [name, source] of entries) {
    try {
      new AsyncFunction("input", source);
    } catch {
      throw new Error(`invalid generated browser script: ${name}`);
    }
  }
});
