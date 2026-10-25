// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertThrows } from "@std/assert";
import {
  decodeNotes,
  decodeNotesBase,
  encodeNotes,
  notesBase,
} from "../../browser-features/modules/common/notes-record.ts";
import { mergeNotesThreeWay } from "../../browser-features/pages-notes/src/lib/merge.ts";

Deno.test("Notes remote ordering includes reserved IDs in the acknowledged base", () => {
  const local = [
    { id: "ordinary", title: "A", content: "A", createdAt: 1, updatedAt: 1 },
    { id: "__proto__", title: "B", content: "B", createdAt: 2, updatedAt: 2 },
  ];
  const base = decodeNotesBase(JSON.stringify({
    lastSyncedSnapshots: notesBase(local),
  }));
  const result = mergeNotesThreeWay(local, local.toReversed(), base);
  assertEquals(result.merged.map((note) => note.id), ["__proto__", "ordinary"]);
  assertEquals(result.hadConflicts, false);
});

const notes = [
  {
    id: "note-a",
    title: "日本語の題名",
    content: "<p>Body with <strong>formatting</strong></p>",
    createdAt: 0,
    updatedAt: 42,
  },
  {
    id: "note-b",
    title: "",
    content: "",
    createdAt: 10,
    updatedAt: Number.MAX_SAFE_INTEGER,
  },
];

function wire(): Record<string, unknown> {
  return JSON.parse(encodeNotes(notes));
}

Deno.test("Notes wire format preserves identities, rich text, and timestamps", () => {
  const encoded = encodeNotes(notes);
  assertEquals(decodeNotes(encoded), notes);
  assertEquals(encodeNotes(decodeNotes(encoded)), encoded);
});

Deno.test("Only an explicit valid empty Notes record represents deletion", () => {
  assertEquals(decodeNotes(encodeNotes([])), []);
  for (const value of ["null", "{}", "[]", '""', "", "{"]) {
    assertThrows(() => decodeNotes(value), Error);
  }
  for (const key of ["titles", "contents", "ids", "createdAts", "updatedAts"]) {
    const missing = JSON.parse(encodeNotes([]));
    delete missing[key];
    assertThrows(() => decodeNotes(JSON.stringify(missing)), TypeError);
  }
});

Deno.test("Remote Notes rejects absent, non-array, and unequal parallel arrays", () => {
  for (const key of ["titles", "contents", "ids", "createdAts", "updatedAts"]) {
    for (const invalid of [undefined, null, {}, "not-an-array", [], ["one"]]) {
      const record = wire();
      if (invalid === undefined) delete record[key];
      else record[key] = invalid;
      assertThrows(() => decodeNotes(JSON.stringify(record)), TypeError);
    }
  }
});

Deno.test("Notes rejects null or incorrectly typed entries even during migration", () => {
  for (const allowLegacy of [false, true]) {
    for (
      const key of ["titles", "contents", "ids", "createdAts", "updatedAts"]
    ) {
      for (const invalid of [null, {}, [], true]) {
        const record = wire();
        const values = record[key] as unknown[];
        values[0] = invalid;
        assertThrows(
          () => decodeNotes(JSON.stringify(record), allowLegacy),
          TypeError,
        );
      }
    }
    for (const key of ["titles", "contents", "ids"]) {
      const record = wire();
      (record[key] as unknown[])[0] = 123;
      assertThrows(
        () => decodeNotes(JSON.stringify(record), allowLegacy),
        TypeError,
      );
    }
  }
});

Deno.test("Notes rejects duplicate and blank identities rather than regenerating them", () => {
  for (const allowLegacy of [false, true]) {
    for (
      const ids of [["note-a", "note-a"], ["", "note-b"], [" \t", "note-b"]]
    ) {
      const record = { ...wire(), ids };
      let generated = 0;
      assertThrows(
        () =>
          decodeNotes(JSON.stringify(record), allowLegacy, () => {
            generated++;
            return "replacement";
          }),
        TypeError,
      );
      assertEquals(generated, 0);
    }
  }
});

Deno.test("Notes rejects negative, fractional, unsafe, and non-numeric timestamps", () => {
  for (const allowLegacy of [false, true]) {
    for (const key of ["createdAts", "updatedAts"]) {
      for (
        const invalid of [
          -1,
          0.5,
          Number.MAX_SAFE_INTEGER + 1,
          NaN,
          Infinity,
          "42",
        ]
      ) {
        const record = wire();
        (record[key] as unknown[])[0] = invalid;
        assertThrows(
          () => decodeNotes(JSON.stringify(record), allowLegacy),
          TypeError,
        );
      }
    }
  }
});

Deno.test("Legacy migration fills only metadata arrays that are entirely missing", () => {
  for (
    const missing of [["ids"], ["createdAts"], ["updatedAts"], [
      "ids",
      "createdAts",
      "updatedAts",
    ]]
  ) {
    const record = wire();
    for (const key of missing) delete record[key];
    let generated = 0;
    const before = Date.now();
    const migrated = decodeNotes(
      JSON.stringify(record),
      true,
      () => `migrated-${++generated}`,
    );
    const after = Date.now();
    assertEquals(generated, missing.includes("ids") ? notes.length : 0);
    for (const [index, note] of migrated.entries()) {
      assertEquals(note.title, notes[index].title);
      assertEquals(note.content, notes[index].content);
      assertEquals(
        note.id,
        missing.includes("ids") ? `migrated-${index + 1}` : notes[index].id,
      );
      if (missing.includes("createdAts")) {
        assert(note.createdAt >= before && note.createdAt <= after);
      } else {
        assertEquals(note.createdAt, notes[index].createdAt);
      }
      assertEquals(
        note.updatedAt,
        missing.includes("updatedAts")
          ? note.createdAt
          : notes[index].updatedAt,
      );
    }
    const persisted = encodeNotes(migrated);
    assertEquals(decodeNotes(persisted), migrated);
    assertEquals(
      encodeNotes(decodeNotes(persisted, true, () => {
        throw new Error(
          "A persisted migration must not generate another identity",
        );
      })),
      persisted,
    );
  }
});

Deno.test("Legacy migration still requires valid title and body arrays", () => {
  for (const key of ["titles", "contents"]) {
    for (const invalid of [undefined, null, {}, [], ["one"]]) {
      const record = wire();
      delete record.ids;
      delete record.createdAts;
      delete record.updatedAts;
      if (invalid === undefined) delete record[key];
      else record[key] = invalid;
      assertThrows(() => decodeNotes(JSON.stringify(record), true), TypeError);
    }
  }
});

Deno.test("Legacy migration rejects present but malformed metadata arrays", () => {
  for (const key of ["ids", "createdAts", "updatedAts"]) {
    for (const invalid of [null, {}, [], [null], [null, null]]) {
      const record = wire();
      record[key] = invalid;
      assertThrows(() => decodeNotes(JSON.stringify(record), true), TypeError);
    }
  }
});

Deno.test("Notes base retains only the acknowledged user content and timestamp", () => {
  const base = notesBase(notes);
  assertEquals(base, {
    "note-a": {
      id: notes[0].id,
      title: notes[0].title,
      content: notes[0].content,
      updatedAt: notes[0].updatedAt,
    },
    "note-b": {
      id: notes[1].id,
      title: notes[1].title,
      content: notes[1].content,
      updatedAt: notes[1].updatedAt,
    },
  });
  assertEquals(
    decodeNotesBase(JSON.stringify({ lastSyncedSnapshots: base })),
    base,
  );
});

Deno.test("Notes base preserves reserved property names as ordinary own identities", () => {
  const reservedNotes = ["__proto__", "constructor", "toString"].map(
    (id, index) => ({
      id,
      title: `Reserved identity ${index}`,
      content: `<p>${id}</p>`,
      createdAt: index,
      updatedAt: index + 10,
    }),
  );
  const base = notesBase(reservedNotes);
  const encoded = JSON.stringify({ lastSyncedSnapshots: base });
  const decoded = decodeNotesBase(encoded);
  assert(Object.getPrototypeOf(decoded) === Object.prototype);
  assertEquals(Object.keys(decoded), reservedNotes.map((note) => note.id));
  for (const note of reservedNotes) {
    assert(Object.hasOwn(decoded, note.id));
    assertEquals(decoded[note.id], base[note.id]);
  }
  assertEquals(JSON.stringify(decoded), JSON.stringify(base));
  const roundTripped = decodeNotesBase(
    JSON.stringify({ lastSyncedSnapshots: decoded }),
  );
  assert(Object.getPrototypeOf(roundTripped) === Object.prototype);
  assertEquals(Object.keys(roundTripped), Object.keys(decoded));
  assertEquals(roundTripped, decoded);
  assertEquals(
    mergeNotesThreeWay(reservedNotes, [], roundTripped).merged,
    [],
    "An acknowledged unchanged note with a reserved ID must obey remote deletion",
  );
});

Deno.test("Empty or rejected Notes bases retain a safe ordinary object prototype", () => {
  for (const value of [null, "", "{}", "{", '{"lastSyncedSnapshots":{}}']) {
    const decoded = decodeNotesBase(value);
    assert(Object.getPrototypeOf(decoded) === Object.prototype);
    assertEquals(Object.keys(decoded), []);
    assertEquals(JSON.stringify(decoded), "{}");
  }
});

Deno.test("Malformed or absent Notes bases never authorize a remote deletion", () => {
  const snapshot = notesBase(notes)[notes[0].id];
  const invalidBases: unknown[] = [
    null,
    [],
    {},
    { lastSyncedSnapshots: null },
    { lastSyncedSnapshots: [] },
    { lastSyncedSnapshots: "invalid" },
    { lastSyncedSnapshots: { "note-a": null } },
    { lastSyncedSnapshots: { "note-a": { ...snapshot, id: "another-id" } } },
    { lastSyncedSnapshots: { " ": { ...snapshot, id: " " } } },
  ];
  for (const key of ["id", "title", "content", "updatedAt"]) {
    const missing: Record<string, unknown> = { ...snapshot };
    delete missing[key];
    invalidBases.push({ lastSyncedSnapshots: { "note-a": missing } });
    for (const invalid of [null, {}, true]) {
      invalidBases.push({
        lastSyncedSnapshots: { "note-a": { ...snapshot, [key]: invalid } },
      });
    }
  }
  for (const invalid of [-1, 0.5, Number.MAX_SAFE_INTEGER + 1, "42"]) {
    invalidBases.push({
      lastSyncedSnapshots: { "note-a": { ...snapshot, updatedAt: invalid } },
    });
  }
  // Even a partly valid map must be discarded atomically: its valid entry
  // would otherwise allow an unrelated local note to be deleted.
  invalidBases.push({
    lastSyncedSnapshots: { "note-a": snapshot, "broken-note": null },
  });
  for (
    const value of [
      null,
      "",
      "{",
      ...invalidBases.map((value) => JSON.stringify(value)),
    ]
  ) {
    const base = decodeNotesBase(value);
    assertEquals(base, {});
    assertEquals(mergeNotesThreeWay([notes[0]], [], base).merged, [notes[0]]);
  }
  const acknowledged = decodeNotesBase(
    JSON.stringify({ lastSyncedSnapshots: { "note-a": snapshot } }),
  );
  assertEquals(mergeNotesThreeWay([notes[0]], [], acknowledged).merged, []);
});
