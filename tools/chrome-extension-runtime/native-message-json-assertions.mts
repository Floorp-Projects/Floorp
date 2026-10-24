// SPDX-License-Identifier: MPL-2.0
import {
  assert,
  assertEquals,
} from "../../browser-features/chrome/test/utils/test_harness.ts";
import type { MessageRecord } from "./native-message-json-test-types.ts";
export function record(
  value: unknown,
  label = "object observation",
): MessageRecord {
  assert(
    value !== null && typeof value === "object" && !Array.isArray(value),
    label,
  );
  return value as MessageRecord;
}
export function records(value: unknown): MessageRecord[] {
  assert(Array.isArray(value), "Observation list exists");
  return value.map((item) => record(item));
}
export function same(actual: unknown, expected: unknown, label: string): void {
  const canonical = (value: unknown): unknown => {
    if (Array.isArray(value)) return value.map(canonical);
    if (value !== null && typeof value === "object") {
      return Object.fromEntries(
        Object.entries(value).filter(([, item]) => item !== undefined).sort((
          [left],
          [right],
        ) => left.localeCompare(right)).map((
          [key, item],
        ) => [key, canonical(item)]),
      );
    }
    return value;
  };
  assertEquals(
    JSON.stringify(canonical(actual)),
    JSON.stringify(canonical(expected)),
    label,
  );
}
export function replaceStrings(
  value: unknown,
  replacements: ReadonlyMap<string, string>,
): unknown {
  if (typeof value === "string") return replacements.get(value) ?? value;
  if (Array.isArray(value)) {
    return value.map((item) => replaceStrings(item, replacements));
  }
  if (value !== null && typeof value === "object") {
    return Object.fromEntries(
      Object.entries(value).map((
        [key, item],
      ) => [key, replaceStrings(item, replacements)]),
    );
  }
  return value;
}
export function decodeDescription(value: unknown): unknown {
  if (value === undefined || value === null) return { absent: true };
  const item = record(value);
  if (["string", "number", "boolean"].includes(String(item.type))) {
    return item.value;
  }
  if (item.tag === "[object Null]") return null;
  if (item.type === "object") {
    assert(Array.isArray(item.entries), "Described object has entries");
    const entries = Object.fromEntries(item.entries.map((pair) => {
      assert(
        Array.isArray(pair) && pair.length === 2,
        "Described entry is a pair",
      );
      return [String(pair[0]), decodeDescription(pair[1])];
    }));
    if (item.tag === "[object Array]") {
      return Array.from(
        { length: Number(item.length) },
        (_, index) =>
          entries[String(index)] ??
            (Object.hasOwn(entries, String(index)) ? null : { hole: true }),
      );
    }
    return entries;
  }
  return { type: item.type, text: item.text };
}
