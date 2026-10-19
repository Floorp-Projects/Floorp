// SPDX-License-Identifier: MPL-2.0
import {
  assert,
  assertEquals,
} from "../../browser-features/chrome/test/utils/test_harness.ts";
import type { NativePortErrorResult } from "./native-port-errors-test-types.ts";
export function assertNativePortErrors(
  result: NativePortErrorResult,
  ordinary: boolean,
): void {
  assertEquals(
    result.before,
    null,
    "No lastError leaks into the next operation",
  );
  assertEquals(
    result.after,
    null,
    "No lastError leaks after the disconnect task",
  );
  assertEquals(
    result.records.length,
    2,
    "Both actual listeners run, including after an intentional throw",
  );
  assertEquals(
    result.microtasks.length,
    2,
    "Each actual listener's microtask executes",
  );
  assertEquals(
    result.turns.length,
    1,
    "An actual later event-loop turn observes cleanup",
  );
  assertEquals(
    result.throwReached,
    result.throwFirst,
    "The intentional throw path actually ran when requested",
  );
  for (
    const value of [...result.records, ...result.microtasks, ...result.turns]
  ) {
    assert(
      value.samePort,
      "The public callback receives its exact original Port object",
    );
    const expectedError = !ordinary && !result.normal &&
      (value.phase === "callback" ||
        (value.phase === "microtask" && !result.isWorker));
    if (expectedError) {
      assert(
        value.lastError && value.lastError.message.length > 0,
        "Chrome callback error is available in the measured dispatch scope",
      );
      assertEquals(
        JSON.stringify(value.lastError.keys),
        '["message"]',
        "Chrome lastError has only its plain message field",
      );
      assertEquals(
        value.lastError.isError,
        false,
        "Chrome lastError is a plain object",
      );
      assertEquals(
        value.lastError.nameType,
        "undefined",
        "Chrome lastError has no Error name",
      );
    } else {
      assertEquals(
        value.lastError,
        null,
        "lastError is absent outside the measured error scope",
      );
    }
    if (ordinary && !result.normal) {
      assert(
        value.portError && value.portError.message.length > 0,
        "Ordinary Firefox retains its public Port.error behavior",
      );
    } else {
      assertEquals(
        value.portError,
        null,
        "Converted Chrome ports do not expose a Firefox error value",
      );
    }
  }
}
