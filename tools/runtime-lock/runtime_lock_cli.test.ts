// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertThrows } from "@std/assert";
import { parseRuntimeLockCliArgs } from "./runtime_lock_cli.ts";

Deno.test("Runtime lock CLI parses validate-lock with the canonical default", () => {
  const options = parseRuntimeLockCliArgs(["validate-lock"]);
  assertEquals(options.command, "validate-lock");
  assert(options.lockPath instanceof URL);
  assertEquals(options.out, undefined);
});

Deno.test("Runtime lock CLI requires an output for native validation", () => {
  assertThrows(
    () => parseRuntimeLockCliArgs(["validate-native"]),
    Error,
    "requires --out",
  );
  const options = parseRuntimeLockCliArgs([
    "validate-native",
    "--out",
    "_dist/runtime-validation",
  ]);
  assertEquals(options.command, "validate-native");
  assert(options.out?.endsWith("runtime-validation"));
});

Deno.test("Runtime lock CLI rejects unknown and misplaced options", () => {
  assertThrows(() => parseRuntimeLockCliArgs(["unknown"]), Error, "Unknown");
  assertThrows(
    () =>
      parseRuntimeLockCliArgs([
        "install-native",
        "--out",
        "_dist/runtime-validation",
      ]),
    Error,
    "does not accept --out",
  );
  assertThrows(
    () =>
      parseRuntimeLockCliArgs([
        "validate-native",
        "--out",
        "outside-runtime-validation",
      ]),
    Error,
    "child of _dist",
  );
});

Deno.test("Runtime lock CLI rejects missing and option-shaped values", () => {
  for (
    const args of [
      ["validate-lock", "--lock"],
      ["validate-native", "--out"],
      ["validate-lock", "--lock", "--out"],
      ["validate-native", "--out", "--lock"],
    ]
  ) {
    assertThrows(
      () => parseRuntimeLockCliArgs(args),
      Error,
      "requires a value",
    );
  }
});
