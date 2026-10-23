// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";

function packagedPath(path: string): string {
  if (path === "browser/base/content/browser-places.js") {
    return "browser/chrome/browser/content/browser/browser-places.js";
  }
  if (path.startsWith("browser/components/places/content/")) {
    return `browser/chrome/browser/content/browser/places/${
      path.split("/").at(-1)
    }`;
  }
  return `moz-src/${path}`;
}

Deno.test("workspace bookmark source and unpacked Runtime patches deliver identical JavaScript", async () => {
  const source = await Deno.readTextFile(
    ".github/patches/floorp-runtime/common/workspace-bookmarks.patch",
  );
  const local = await Deno.readTextFile(
    "tools/patches/workspace-bookmarks.patch",
  );
  const expected = source.split("\n").map((line) => {
    if (line.startsWith("diff --git ")) {
      const [, , before, after] = line.split(" ");
      return `diff --git a/${packagedPath(before.slice(2))} b/${
        packagedPath(after.slice(2))
      }`;
    }
    if (line.startsWith("--- a/") || line.startsWith("+++ b/")) {
      return line.slice(0, 6) + packagedPath(line.slice(6));
    }
    return line;
  }).join("\n");
  assertEquals(local, expected);
});
