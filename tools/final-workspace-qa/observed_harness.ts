// SPDX-License-Identifier: MPL-2.0

import {
  assert as originalAssert,
  assertEquals as originalAssertEquals,
} from "../../browser-features/chrome/test/utils/test_harness.ts";
import type { AssertionEvent } from "./types.ts";

export type { TestCase } from "../../browser-features/chrome/test/utils/test_harness.ts";

let active: AssertionEvent[] | null = null;
let originalException: string | null = null;

export function beginAssertions(): AssertionEvent[] {
  if (active !== null) throw new Error("Overlapping assertion observation");
  active = [];
  originalException = null;
  return active;
}

export function endAssertions(): void {
  active = null;
}

export function recordCaseException(error: unknown): void {
  originalException ??= String(error).slice(0, 1024);
}

export function firstCaseException(): string | null {
  return originalException;
}

function record(passed: boolean, message: string): void {
  if (active === null || active.length >= 128) {
    throw new Error(
      "Assertion outside an owned case or above its bounded limit",
    );
  }
  active.push({ passed, message: message.slice(0, 512) });
}

export function assert(condition: unknown, message: string): asserts condition {
  record(Boolean(condition), message);
  originalAssert(condition, message);
}

export function assertEquals<T>(actual: T, expected: T, message: string): void {
  record(actual === expected, message);
  originalAssertEquals(actual, expected, message);
}
