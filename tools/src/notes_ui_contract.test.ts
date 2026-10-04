// SPDX-License-Identifier: MPL-2.0

import { assert, assertFalse, assertStringIncludes } from "@std/assert";

Deno.test("Notes production UI exposes stable automation hooks", async () => {
  const root = new URL(
    "../../browser-features/pages-notes/src/",
    import.meta.url,
  );
  const sources = await Promise.all([
    Deno.readTextFile(new URL("App.tsx", root)),
    Deno.readTextFile(new URL("components/notes/NoteItem.tsx", root)),
    Deno.readTextFile(new URL("components/notes/NoteSearch.tsx", root)),
    Deno.readTextFile(new URL("components/editor/RichTextEditor.tsx", root)),
    Deno.readTextFile(new URL("components/common/ConfirmModal.tsx", root)),
  ]);
  const combined = sources.join("\n");
  for (
    const hook of [
      "notes-add",
      "notes-search",
      "notes-title",
      "notes-body",
      "notes-row",
      "notes-delete",
      "notes-delete-confirm",
    ]
  ) {
    assertStringIncludes(combined, `data-testid=\"${hook}\"`);
  }

  const appSource = sources[0];
  const localSaveStart = appSource.indexOf("const saveNotesToStorage");
  const localSaveEnd = appSource.indexOf("const debouncedSave", localSaveStart);
  assert(localSaveStart >= 0 && localSaveEnd > localSaveStart);
  const localSaveSection = appSource.slice(localSaveStart, localSaveEnd);
  assertFalse(
    localSaveSection.includes("saveSyncState"),
    "A local pref write must not advance the server-confirmed merge base",
  );
  assertStringIncludes(
    appSource,
    "saveSyncState(syncStateFromNotes(remoteNotes))",
  );
  assertStringIncludes(
    appSource,
    "let syncMergeQueue: Promise<void> = Promise.resolve()",
  );
  assertStringIncludes(
    appSource,
    "syncMergeQueue = Promise.all([",
  );
  assertStringIncludes(
    appSource,
    "notesRef.current = result.merged",
  );
});
