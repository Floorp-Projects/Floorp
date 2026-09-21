// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";

Deno.test("workspace last-tab protection matches in production and local Runtime patches", async () => {
  const sourcePath = "browser/components/tabbrowser/Tabbrowser.sys.mjs";
  const source = await Deno.readTextFile(
    ".github/patches/floorp-runtime/common/workspace-last-tab.patch",
  );
  const local = await Deno.readTextFile(
    "tools/patches/workspace-last-tab.patch",
  );
  assertEquals(
    local.replaceAll("\r\n", "\n"),
    source.replaceAll("\r\n", "\n").replaceAll(
      sourcePath,
      `moz-src/${sourcePath}`,
    ),
  );
});
