// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert";
import * as path from "@std/path";
import expandedGroups from "../workspace-expanded-development/groups.json" with {
  type: "json",
};
import {
  BROWSER_CATALOG,
  browserDiscoverable,
  canonicalTestId,
  makeBrowserCatalog,
  testInventoryMetadata,
} from "../../libs/test-inventory/contract.ts";
import type {
  BrowserInventoryCatalog,
  BrowserInventorySelection,
} from "../../libs/test-inventory/types.ts";
import {
  buildBrowserInventoryPlan,
  discoverColocatedTests,
  inventorySourceHashes,
  reconcileBrowserInventoryResults,
  requireExecutableBrowserPlan,
} from "./browser_test_inventory.ts";
import {
  parseOptions,
  resolveDevelopmentTargets,
  runColocatedTests,
} from "./colocated_test_runner.ts";

const floorp = "browser-features/chrome/test/owned.test.ts";
const mozilla =
  "browser-features/chrome/test/firefox-downloaded/generated/locked.test.js";
const adapted = "browser-features/chrome/test/firefox-imported/old.test.js";
const compat = "browser-features/chrome/test/mochitest-compat/context.test.js";
const catalogValue: BrowserInventoryCatalog = {
  schemaVersion: 1,
  groups: [
    { origin: "floorp", executionKind: "development-browser", files: [floorp] },
    {
      origin: "mozilla-converted",
      executionKind: "development-mozilla-wrapper",
      files: [mozilla],
    },
    {
      origin: "mozilla-adapted",
      executionKind: "development-mozilla-adaptation",
      files: [adapted],
    },
    {
      origin: "floorp",
      executionKind: "development-harness-compat",
      files: [compat],
    },
  ],
};
const catalog = makeBrowserCatalog(catalogValue);
const browserFiles = [floorp, mozilla, adapted, compat];
const digest = "a".repeat(64);
function plan(
  files = browserFiles,
  selected = browserFiles,
  selection?: BrowserInventorySelection,
) {
  return buildBrowserInventoryPlan(
    files,
    selected,
    new Map(files.map((id) => [id, digest])),
    digest,
    catalog,
    selection,
  );
}

Deno.test("inventory distinguishes authored, converted, adapted, compatibility and tooling", () => {
  assertEquals(testInventoryMetadata(floorp, catalog).origin, "floorp");
  assertEquals(
    testInventoryMetadata(mozilla, catalog).origin,
    "mozilla-converted",
  );
  assertEquals(
    testInventoryMetadata(adapted, catalog).origin,
    "mozilla-adapted",
  );
  assertEquals(
    testInventoryMetadata(compat, catalog).executionKind,
    "development-harness-compat",
  );
  assertEquals(
    testInventoryMetadata("tools/src/fixture.test.ts", catalog).executionKind,
    "host-tooling",
  );
  assertEquals(
    testInventoryMetadata("browser-features/chrome/test/new.test.ts", catalog)
      .origin,
    "unknown",
  );
  for (const id of browserFiles) {
    assertEquals(
      testInventoryMetadata(id, catalog).packagedStatus,
      "not-onboarded",
    );
  }
});

Deno.test("inventory catalog rejects duplicate, changed-origin and unsafe entries", () => {
  const duplicate = structuredClone(catalogValue);
  duplicate.groups[0].files.push(floorp);
  assertThrows(() => makeBrowserCatalog(duplicate), Error, "Duplicate");
  const changed = structuredClone(catalogValue);
  changed.groups[1] = {
    origin: "floorp",
    executionKind: "development-browser",
    files: [mozilla],
  };
  assertThrows(() => makeBrowserCatalog(changed), Error, "provenance");
  const unsupported = structuredClone(catalogValue);
  unsupported.groups[0].files = ["tools/src/fixture.test.ts"];
  assertThrows(() => makeBrowserCatalog(unsupported), Error, "noncanonical");
  for (
    const id of [
      "../escape.test.ts",
      "/absolute.test.ts",
      "a//b.test.ts",
      "#unknown/test.ts",
      "a/../b.test.ts",
    ]
  ) {
    assertThrows(() => canonicalTestId(id), Error, "identity");
  }
});

Deno.test("inventory retains the complete denominator when selection is reduced", () => {
  const inventory = plan([...browserFiles, "tools/src/fixture.test.ts"], [
    floorp,
  ], { scope: "explicit-development-selection", requiredCatalogIds: [floorp] });
  assertEquals(inventory.counts.discovered, 5);
  assertEquals(inventory.counts.browserDiscoverable, 4);
  assertEquals(inventory.counts.catalogBrowserFiles, 4);
  assertEquals(inventory.counts.selected, 1);
  assertEquals(inventory.counts.notOnboarded, 4);
  assertEquals(inventory.counts.unsupported, 1);
  assertEquals(inventory.counts.packagedEligible, 0);
  assertEquals(inventory.counts.quarantinedColocated, 0);
  assertEquals(inventory.counts.quarantinedUpstreamCandidates, 1);
  assertEquals(inventory.upstreamQuarantine[0].classification, "quarantined");
  assertEquals(inventory.counts.executed, 0);
  assertEquals(inventory.productVerdict, "UNVERIFIED");
  assertEquals(inventory.publicationAuthorized, false);
  requireExecutableBrowserPlan(inventory);
  assertEquals(
    JSON.stringify(inventory),
    JSON.stringify(
      plan([...browserFiles, "tools/src/fixture.test.ts"].reverse(), [floorp], {
        scope: "explicit-development-selection",
        requiredCatalogIds: [floorp],
      }),
    ),
  );
});

Deno.test("scoped development selection observes unselected missing wrappers without requiring them", () => {
  const inventory = plan([floorp], [floorp], {
    scope: "explicit-development-selection",
    requiredCatalogIds: [floorp],
  });
  requireExecutableBrowserPlan(inventory);
  assertEquals(inventory.counts.catalogBrowserFiles, 4);
  assertEquals(inventory.counts.missingCatalogSources, 3);
  assertEquals(inventory.counts.requiredCatalogFiles, 1);
  assertEquals(
    inventory.unselectedMissingCatalogSources,
    [adapted, mozilla, compat].sort(),
  );
  assertThrows(
    () => requireExecutableBrowserPlan(plan([floorp], [floorp])),
    Error,
    "missing",
  );
  const missingSelected = plan([floorp], [floorp], {
    scope: "explicit-development-selection",
    requiredCatalogIds: [floorp, mozilla],
  });
  assertThrows(
    () => requireExecutableBrowserPlan(missingSelected),
    Error,
    "missing",
  );
  assertThrows(
    () => requireExecutableBrowserPlan(plan(browserFiles, [floorp])),
    Error,
    "not selected",
  );
});

Deno.test("actual scoped selector retains required workspace files when unrelated wrappers are absent", () => {
  const discovered = [...BROWSER_CATALOG.keys()].filter((id) =>
    !id.includes("/firefox-downloaded/generated/")
  );
  for (
    const target of [
      "browser-features/chrome/common/workspaces/test/nativeLastTabWorkspacesEnabled.test.ts",
      ...Object.values(expandedGroups).map((group) => group.target),
    ]
  ) {
    const options = parseOptions(["--layer", "chrome", "--near", target]);
    const resolved = resolveDevelopmentTargets(options, discovered);
    assertEquals(resolved.targetRels, [target]);
    assertEquals(resolved.selection.requiredCatalogIds, [target]);
    const hashes = new Map(discovered.map((id) => [id, digest]));
    const inventory = buildBrowserInventoryPlan(
      discovered,
      resolved.targetRels,
      hashes,
      digest,
      BROWSER_CATALOG,
      resolved.selection,
    );
    requireExecutableBrowserPlan(inventory);
    assertEquals(inventory.counts.missingCatalogSources, 8);
    assertEquals(inventory.unselectedMissingCatalogSources.length, 8);
    const missing = discovered.filter((id) => id !== target);
    const reduced = resolveDevelopmentTargets(options, missing);
    assertEquals(reduced.selection.requiredCatalogIds, [target]);
    assert(reduced.targetRels.length === 0);
    assertThrows(
      () =>
        requireExecutableBrowserPlan(
          buildBrowserInventoryPlan(
            missing,
            reduced.targetRels,
            hashes,
            digest,
            BROWSER_CATALOG,
            reduced.selection,
          ),
        ),
      Error,
    );
  }
});

Deno.test("inventory refuses empty, missing, duplicate and unclassified required plans", () => {
  assertThrows(
    () => requireExecutableBrowserPlan(plan(browserFiles, [])),
    Error,
    "no tests",
  );
  const missing = plan(browserFiles.slice(1), browserFiles.slice(1));
  assertEquals(missing.counts.missingCatalogSources, 1);
  assertThrows(() => requireExecutableBrowserPlan(missing), Error, "missing");
  const newId = "browser-features/chrome/test/unreviewed.test.ts";
  assertThrows(
    () => requireExecutableBrowserPlan(plan([...browserFiles, newId], [newId])),
    Error,
    "not classified",
  );
  assertThrows(
    () =>
      requireExecutableBrowserPlan(
        plan(browserFiles, ["tools/src/absent.test.ts"]),
      ),
    Error,
    "unsupported",
  );
  assertThrows(() => plan([...browserFiles, floorp]), Error, "Duplicate");
  assertThrows(() => plan(browserFiles, [floorp, floorp]), Error, "Duplicate");
  assertThrows(
    () =>
      buildBrowserInventoryPlan(
        browserFiles,
        browserFiles,
        new Map(),
        digest,
        catalog,
      ),
    Error,
    "digest",
  );
});

Deno.test("inventory reconciles exact result IDs and rejects origin, kind, marker and duplicate drift", () => {
  const inventory = plan();
  const results = browserFiles.map((file) => ({
    file,
    ...testInventoryMetadata(file, catalog),
    source: file === mozilla ? "downloaded-firefox" : undefined,
  }));
  assertEquals(reconcileBrowserInventoryResults(inventory, results), []);
  for (
    const extra of [
      { origin: "mozilla-adapted" as const },
      { executionKind: "host-tooling" as const },
      { inventoryVersion: 0 },
      { source: "downloaded-firefox" },
    ]
  ) {
    assert(
      reconcileBrowserInventoryResults(inventory, [{ ...results[0], ...extra }])
        .some((issue) => issue.includes("mismatch")),
    );
  }
  assertEquals(
    reconcileBrowserInventoryResults(inventory, [...results, results[0]])
      .length,
    1,
  );
  for (
    const file of ["prefix-" + floorp, floorp.toUpperCase(), "#unknown/test.ts"]
  ) {
    assertEquals(
      reconcileBrowserInventoryResults(inventory, [{ ...results[0], file }])
        .length,
      1,
    );
  }
  assertEquals(
    reconcileBrowserInventoryResults(inventory, [{
      ...results[0],
      file: "#features-chrome/test/owned.test.ts",
    }]),
    [],
  );
});

Deno.test("actual inventory discovery covers each root and excludes build, dependency and log output", async () => {
  const root = await Deno.makeTempDir();
  try {
    const files = [
      ...browserFiles,
      "browser-features/modules/one.test.mts",
      "browser-features/pages-settings/one.test.tsx",
      "tools/src/one.test.ts",
      "bridge/loader/one.test.ts",
      "libs/one.test.ts",
    ];
    for (
      const id of [
        ...files,
        "_dist/skip.test.ts",
        "node_modules/skip.test.ts",
        "logs/skip.test.ts",
        "libs/@types/gecko/skip.test.ts",
        "tools/src/not-a-test.ts",
      ]
    ) {
      const filename = path.join(root, id);
      await Deno.mkdir(path.dirname(filename), { recursive: true });
      await Deno.writeTextFile(filename, "// host discovery fixture\n");
    }
    assertEquals(discoverColocatedTests(root), [...files].sort());
    const hashes = await inventorySourceHashes(root, files);
    assertEquals(hashes.size, files.length);
    assertEquals(new Set(hashes.values()).size, 1);
    for (const id of files.filter((id) => id.startsWith("browser-features/"))) {
      assert(browserDiscoverable(id));
    }
    await Deno.remove(path.join(root, floorp));
    await assertRejects(
      () => inventorySourceHashes(root, [floorp]),
      Deno.errors.NotFound,
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("inventory CLI options constrain development origin and create-only JSON output", () => {
  const options = parseOptions([
    "--origin",
    "floorp",
    "--inventory",
    "new.json",
    "--list",
  ]);
  assertEquals(options.origin, "floorp");
  assertEquals(options.inventoryPath, "new.json");
  assertEquals(parseOptions([]).origin, "all");
  for (const origin of ["product", "tooling", "unknown"]) {
    assertThrows(() => parseOptions(["--origin", origin]), Error, "origin");
  }
  assertThrows(() => parseOptions(["--inventory", ""]), Error, "Empty");
  assertEquals(BROWSER_CATALOG.size, 262);
});

Deno.test("real runner rejects an empty browser scope before reaching any network or process permission", async () => {
  const saved = console.error;
  const savedLog = console.log;
  const notices: string[] = [];
  console.error = console.log = (...values: unknown[]) =>
    notices.push(values.join(" "));
  try {
    assertEquals(
      await runColocatedTests(["--near", "tools/src", "--no-autostart"]),
      1,
    );
    assert(
      !notices.some((line) => line.includes("=== Browser Connection ===")),
    );
    assert(
      notices.some((line) =>
        line.includes("planning failed") || line.includes("selected no tests")
      ),
    );
  } finally {
    console.error = saved;
    console.log = savedLog;
  }
});

Deno.test("real runner emits the complete informational inventory and refuses overwriting it", async () => {
  const directory = await Deno.makeTempDir();
  const filename = path.join(directory, "inventory.json");
  const saved = console.error;
  const savedLog = console.log;
  console.error = console.log = () => {};
  try {
    const args = ["--near", "tools/src", "--list", "--inventory", filename];
    assertEquals(await runColocatedTests(args), 0);
    const bytes = await Deno.readTextFile(filename);
    const inventory = JSON.parse(bytes);
    assertEquals(inventory.scope, "colocated-source-files");
    assertEquals(inventory.counts.catalogBrowserFiles, 262);
    assertEquals(inventory.counts.selected, 0);
    assertEquals(inventory.counts.executed, 0);
    assertEquals(inventory.productVerdict, "UNVERIFIED");
    assertEquals(inventory.publicationAuthorized, false);
    assertEquals(await runColocatedTests(args), 1);
    assertEquals(await Deno.readTextFile(filename), bytes);
  } finally {
    console.error = saved;
    console.log = savedLog;
    await Deno.remove(directory, { recursive: true });
  }
});
