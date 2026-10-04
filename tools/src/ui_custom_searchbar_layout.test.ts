// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals } from "@std/assert";

const SEARCHBAR_LAYOUT_CSS = new URL(
  "../../browser-features/chrome/common/ui-custom/styles/css/options/move_page_inside_searchbar.css",
  import.meta.url,
);

async function getBrowserContainerRule(): Promise<string> {
  const css = await Deno.readTextFile(SEARCHBAR_LAYOUT_CSS);
  const rule = /\.browserContainer\s*\{([\s\S]*?)\}/.exec(css)?.[1];

  assert(rule, "the searchbar layout should target .browserContainer");
  return rule;
}

function getDeclaration(rule: string, property: string): string {
  const escapedProperty = property.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const value = new RegExp(`${escapedProperty}\\s*:\\s*([\\s\\S]*?);`).exec(
    rule,
  )?.[1];

  assert(value, `${property} should be declared`);
  return value;
}

Deno.test("top findbar preserves the Firefox 156 browser grid", async () => {
  const rule = await getBrowserContainerRule();
  const areas = getDeclaration(rule, "grid-template-areas");
  const rows = Array.from(
    areas.matchAll(/"([^"]+)"/g),
    (match) => match[1].trim().split(/\s+/),
  );

  assertEquals(
    rows.map((row) => row[2]),
    [
      "findbar",
      "notificationbox",
      "rdm-toolbar",
      "browserstack",
      "devtools-bottom-splitter",
      "devtools-bottom",
    ],
  );
  assert(
    rows.slice(0, 4).every((row) =>
      row.length === 5 &&
      row[0] === "devtools-side-start" &&
      row[1] === "devtools-side-start-splitter" &&
      row[3] === "devtools-side-end-splitter" &&
      row[4] === "devtools-side-end"
    ),
    "side-docked DevTools should span the central content rows",
  );
  assert(
    rows[4].every((area) => area === "devtools-bottom-splitter") &&
      rows[5].every((area) => area === "devtools-bottom"),
    "bottom-docked DevTools should continue to span the full grid",
  );

  const rowSizes = getDeclaration(rule, "grid-template-rows");
  assert(rowSizes.includes("var(--rdm-toolbar-height, 0)"));
  assert(rowSizes.includes("minmax(var(--content-area-min-size), 1fr)"));
});
