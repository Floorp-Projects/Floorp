// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals } from "@std/assert";

Deno.test("Windows custom icon patches use the packaged taskbar module", async () => {
  const files = [
    ".github/patches/floorp-runtime/custom-app-icons-manager.patch",
    "tools/patches/custom-app-icons-manager.windows.patch",
    ".github/patches/floorp-runtime/custom-app-icons-tests.patch",
  ];
  const packaged =
    "moz-src:///browser/components/taskbartabs/TaskbarTabsUtils.sys.mjs";
  const missing = "resource:///modules/taskbartabs/TaskbarTabsUtils.sys.mjs";
  for (const path of files) {
    const text = await Deno.readTextFile(path);
    assert(text.includes(packaged), `${path} must use the packaged module`);
    assertEquals(
      text.includes(missing),
      false,
      `${path} must not use the missing alias`,
    );
  }
});
