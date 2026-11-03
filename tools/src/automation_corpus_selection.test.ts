// SPDX-License-Identifier: MPL-2.0
import { assertEquals, assertThrows } from "@std/assert";
import {
  automationIds,
  selectAutomationIds,
} from "../chrome-extension-runtime/automation-corpus-selection.mjs";
import type { FrozenCorpus } from "../chrome-extension-runtime/corpus-types.ts";

const corpus: FrozenCorpus = {
  extensions: automationIds.map((id) => ({
    id,
    name: id,
    area: "automation",
    primaryOperation: "host selection boundary only",
    status: "frozen",
  })),
};

Deno.test("automation selection runs only requested IDs in their requested order", () => {
  const selected = [automationIds[3], automationIds[0], automationIds[1]];
  assertEquals(selectAutomationIds(selected.join(","), corpus), selected);
  assertEquals(selected.includes(automationIds[2]), false);
  assertEquals(selectAutomationIds("", corpus), [...automationIds]);
});

Deno.test("automation selection rejects unknown IDs, duplicates and empty members", () => {
  for (
    const selection of [
      "a".repeat(32),
      ",",
      automationIds[0] + ",",
      [automationIds[0], automationIds[0]].join(","),
    ]
  ) {
    assertThrows(() => selectAutomationIds(selection, corpus));
  }
});

Deno.test("automation selection requires an exact frozen record without substituting packages", () => {
  const id = automationIds[0];
  assertThrows(() => selectAutomationIds(id, { extensions: [] }));
  assertThrows(() =>
    selectAutomationIds(id, {
      extensions: [corpus.extensions[0], corpus.extensions[0]],
    })
  );
  assertThrows(() =>
    selectAutomationIds(id, {
      extensions: [{ ...corpus.extensions[0], status: "unavailable" }],
    })
  );
});
