// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";
import { claimTestRunOwnership } from "../../bridge/startup/src/test_run_owner.ts";

Deno.test("separate window bootstraps claim a shared browser process only once", async () => {
  const sharedData = new Map<string, unknown>();
  let suiteStarts = 0;
  let featureStarts = 0;
  const bootstrapWindow = async () => {
    const owner = claimTestRunOwnership(sharedData);
    await Promise.resolve(); // Feature loading can let another window start.
    featureStarts++;
    if (owner) suiteStarts++;
  };
  await Promise.all([bootstrapWindow(), bootstrapWindow(), bootstrapWindow()]);
  assertEquals(featureStarts, 3);
  assertEquals(suiteStarts, 1);
  // Opening a window after completion cannot overwrite the collected result.
  await bootstrapWindow();
  assertEquals(suiteStarts, 1);
});

Deno.test("ownership survives a failed owner and resets with the browser process", () => {
  const firstProcess = new Map<string, unknown>();
  assertEquals(claimTestRunOwnership(firstProcess), true);
  // No release occurs on failure or window closure: the host needs that result.
  assertEquals(claimTestRunOwnership(firstProcess), false);
  const restartedProcess = new Map<string, unknown>();
  assertEquals(claimTestRunOwnership(restartedProcess), true);
});
