// SPDX-License-Identifier: MPL-2.0

import { assertRejects } from "@std/assert";
import { sleep } from "./async_utils.ts";

Deno.test("sleep clears timers and abort listeners on success, cancellation and already-aborted signals", async () => {
  await sleep(1);
  const controller = new AbortController();
  await sleep(1, controller.signal);
  const pending = sleep(10000, controller.signal);
  controller.abort(new Error("cancelled sleep"));
  await assertRejects(() => pending, Error, "cancelled sleep");
  await assertRejects(
    () => sleep(10000, controller.signal),
    Error,
    "cancelled sleep",
  );
});
