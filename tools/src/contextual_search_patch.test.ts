// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";

Deno.test("contextual search initialization matches source and extracted builds", async () => {
  const sourcePath = "toolkit/components/search/SearchEngineSelector.sys.mjs";
  const production = await Deno.readTextFile(
    ".github/patches/floorp-runtime/common/contextual-search-initialization.patch",
  );
  const local = await Deno.readTextFile(
    "tools/patches/moz-src-toolkit-components-search-SearchEngineSelector.sys.patch",
  );
  assertEquals(
    local.replaceAll("\r\n", "\n"),
    production.replaceAll("\r\n", "\n").replaceAll(
      sourcePath,
      `moz-src/${sourcePath}`,
    ),
  );
});
