// SPDX-License-Identifier: MPL-2.0

import { assertStringIncludes } from "@std/assert";

Deno.test("Clips production UI exposes stable automation hooks", async () => {
  const root = new URL(
    "../../browser-features/pages-clips/src/",
    import.meta.url,
  );
  const sources = await Promise.all([
    Deno.readTextFile(new URL("App.tsx", root)),
    Deno.readTextFile(new URL("components/clips/ClipItem.tsx", root)),
    Deno.readTextFile(new URL("components/clips/ClipComposer.tsx", root)),
    Deno.readTextFile(new URL("components/common/ConfirmModal.tsx", root)),
  ]);
  const combined = sources.join("\n");
  for (
    const hook of [
      "clips-search",
      "clips-pinned-filter",
      "clips-row",
      "clips-pin",
      "clips-delete",
      "clips-delete-confirm",
      "clips-open-file",
      "clips-input",
      "clips-suggest",
    ]
  ) {
    assertStringIncludes(combined, `data-testid=\"${hook}\"`);
  }
});
