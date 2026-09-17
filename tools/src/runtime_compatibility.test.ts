// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";

// Production patches source modules before packaging; local browser tests patch
// extracted modules. Keep the actual changes identical across both build paths.
Deno.test("Firefox 156 session and split patches match in production and local builds", async () => {
  const source = await Deno.readTextFile(
    ".github/patches/floorp-runtime/common/tab-state-and-split-view.patch",
  );
  const patches = source.replaceAll("\r\n", "\n").split("diff --git ").slice(1);
  const mappings = [
    [
      "browser/components/tabbrowser/Tabbrowser.sys.mjs",
      "moz-src/browser/components/tabbrowser/Tabbrowser.sys.mjs",
      "browser-chrome-browser-content-browser-tabbrowser-tabbrowser.patch",
    ],
    [
      "browser/components/sessionstore/SessionStore.sys.mjs",
      "moz-src/browser/components/sessionstore/SessionStore.sys.mjs",
      "browser-modules-sessionstore-SessionStore.sys.patch",
    ],
    [
      "browser/components/sessionstore/TabState.sys.mjs",
      "moz-src/browser/components/sessionstore/TabState.sys.mjs",
      "browser-modules-sessionstore-TabState.sys.patch",
    ],
    [
      "browser/themes/shared/tabbrowser/content-area.css",
      "browser/chrome/browser/skin/classic/browser/tabbrowser/content-area.css",
      "browser-chrome-browser-skin-classic-browser-tabbrowser-content-area.patch",
    ],
  ];
  assertEquals(patches.length, mappings.length);
  for (const [sourcePath, packedPath, filename] of mappings) {
    const matching = patches.filter((part) =>
      part.startsWith(`a/${sourcePath} b/${sourcePath}\n`)
    );
    assertEquals(matching.length, 1, sourcePath);
    const expected = ("diff --git " + matching[0]).replaceAll(
      sourcePath,
      packedPath,
    );
    const actual = await Deno.readTextFile(`tools/patches/${filename}`);
    assertEquals(actual.replaceAll("\r\n", "\n"), expected, filename);
  }
});
