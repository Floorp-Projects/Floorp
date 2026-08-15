// SPDX-License-Identifier: MPL-2.0

import { assertStringIncludes } from "@std/assert";

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
    ]
  ) {
    assertStringIncludes(combined, `data-testid=\"${hook}\"`);
  }
});
