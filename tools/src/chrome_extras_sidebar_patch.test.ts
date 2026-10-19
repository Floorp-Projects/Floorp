// SPDX-License-Identifier: MPL-2.0

Deno.test("Chrome Extras: development and packaging header patches agree", async () => {
  const source = await Deno.readTextFile(
    new URL(
      "../../.github/patches/floorp-runtime/common/chrome-extras-sidebar-header.patch",
      import.meta.url,
    ),
  );
  const development = await Deno.readTextFile(
    new URL("../patches/chrome-extras-sidebar-header.patch", import.meta.url),
  );
  const expected = source.replaceAll(
    "browser/components/sidebar/sidebar-panel-header.css",
    "browser/chrome/browser/content/browser/sidebar/sidebar-panel-header.css",
  );
  if (development !== expected) {
    throw new Error("Development and packaging header patches must agree");
  }
});
