// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";

Deno.test("tab context menu sections match in production and extracted Runtime patches", async () => {
  const sourcePath =
    "browser/components/tabbrowser/content/tab-context-menu.js";
  const production = await Deno.readTextFile(
    ".github/patches/floorp-runtime/common/tab-context-menu-sections.patch",
  );
  const extracted = await Deno.readTextFile(
    "tools/patches/tab-context-menu-sections.patch",
  );
  assertEquals(
    extracted.replaceAll("\r\n", "\n"),
    production.replaceAll("\r\n", "\n").replaceAll(
      sourcePath,
      "browser/chrome/browser/content/browser/tabbrowser/tab-context-menu.js",
    ),
  );
});
