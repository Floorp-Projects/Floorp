// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";
import { resolveMode, createSmokeSteps } from "./smoke_runner.ts";

// --- resolveMode ---

Deno.test("resolveMode returns 'all' when value is undefined", () => {
  assertEquals(resolveMode(undefined), "all");
});

Deno.test("resolveMode returns 'all' for empty string", () => {
  assertEquals(resolveMode(""), "all");
});

Deno.test("resolveMode accepts 'all'", () => {
  assertEquals(resolveMode("all"), "all");
});

Deno.test("resolveMode accepts 'unit'", () => {
  assertEquals(resolveMode("unit"), "unit");
});

Deno.test("resolveMode accepts 'runtime'", () => {
  assertEquals(resolveMode("runtime"), "runtime");
});

Deno.test("resolveMode is case-insensitive", () => {
  assertEquals(resolveMode("ALL"), "all");
  assertEquals(resolveMode("Unit"), "unit");
  assertEquals(resolveMode("RUNTIME"), "runtime");
});

Deno.test("resolveMode maps 'browser' to 'runtime' for backward compatibility", () => {
  assertEquals(resolveMode("browser"), "runtime");
  assertEquals(resolveMode("BROWSER"), "runtime");
});

Deno.test("resolveMode throws on invalid value", () => {
  assertThrows(
    () => resolveMode("invalid"),
    Error,
    "Invalid --mode value",
  );
});

// --- createSmokeSteps ---

Deno.test("createSmokeSteps('unit') returns only unit steps", () => {
  const steps = createSmokeSteps("unit");
  assertEquals(steps.length, 1);
  assertEquals(steps[0].name, "colocated runner unit tests");
});

Deno.test("createSmokeSteps('runtime') returns only runtime steps", () => {
  const steps = createSmokeSteps("runtime");
  for (const step of steps) {
    assertEquals(step.name.startsWith("runtime"), true);
  }
  // Should not include unit steps
  const hasUnit = steps.some((s) => s.name.includes("unit test"));
  assertEquals(hasUnit, false);
});

Deno.test("createSmokeSteps('all') returns both unit and runtime steps", () => {
  const allSteps = createSmokeSteps("all");
  const unitSteps = createSmokeSteps("unit");
  const runtimeSteps = createSmokeSteps("runtime");
  assertEquals(allSteps.length, unitSteps.length + runtimeSteps.length);
});

Deno.test("createSmokeSteps steps have non-empty name and args", () => {
  const steps = createSmokeSteps("all");
  for (const step of steps) {
    assertEquals(step.name.length > 0, true);
    assertEquals(step.args.length > 0, true);
  }
});
