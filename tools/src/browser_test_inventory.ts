// SPDX-License-Identifier: MPL-2.0

import { walkSync } from "@std/fs";
import * as path from "@std/path";
import upstreamQuarantine from "../../browser-features/chrome/test/firefox-downloaded/quarantine.json" with {
  type: "json",
};
import {
  BROWSER_CATALOG,
  browserDiscoverable,
  canonicalTestId,
  testInventoryMetadata,
} from "../../libs/test-inventory/contract.ts";
import type {
  BrowserInventoryPlan,
  BrowserInventorySelection,
  InventoryBrowserResult,
  TestInventoryEntry,
  TestInventoryMetadata,
} from "../../libs/test-inventory/types.ts";
import { isTestFile } from "./colocated_test_utils.ts";

const WALK_SKIP_PATTERNS = [
  /(?:^|[\\/])(?:_dist|\.git|node_modules|\.venv|\.vscode|logs)(?:[\\/]|$)/,
  /(?:^|[\\/])libs[\\/]@types[\\/]gecko(?:[\\/]|$)/,
];

export function discoverColocatedTests(projectRoot: string): string[] {
  const files: string[] = [];
  for (
    const entry of walkSync(projectRoot, {
      includeDirs: false,
      followSymlinks: false,
      skip: WALK_SKIP_PATTERNS,
    })
  ) {
    if (entry.isSymlink || !entry.isFile) continue;
    const id = path.relative(projectRoot, entry.path).replaceAll("\\", "/");
    if (isTestFile(id)) files.push(canonicalTestId(id));
    if (files.length > 4096) {
      throw new Error("Test inventory file bound exceeded");
    }
  }
  return files.sort();
}

export async function sha256(bytes: Uint8Array): Promise<string> {
  return Array.from(
    new Uint8Array(
      await crypto.subtle.digest("SHA-256", new Uint8Array(bytes)),
    ),
  )
    .map((value) => value.toString(16).padStart(2, "0")).join("");
}

export async function inventorySourceHashes(
  projectRoot: string,
  files: string[],
): Promise<ReadonlyMap<string, string>> {
  const hashes = new Map<string, string>();
  let total = 0;
  for (const id of files) {
    if (canonicalTestId(id) !== id) throw new Error("Noncanonical source ID");
    const filename = path.join(projectRoot, id);
    const stat = await Deno.lstat(filename);
    if (!stat.isFile || stat.isSymlink || stat.size > 2 * 1024 * 1024) {
      throw new Error("Invalid or oversized test inventory source");
    }
    total += stat.size;
    if (total > 32 * 1024 * 1024) {
      throw new Error("Inventory source bound exceeded");
    }
    hashes.set(id, await sha256(await Deno.readFile(filename)));
  }
  return hashes;
}

export function buildBrowserInventoryPlan(
  discovered: string[],
  selected: string[],
  hashes: ReadonlyMap<string, string>,
  catalogSha256: string,
  catalog: ReadonlyMap<string, TestInventoryMetadata> = BROWSER_CATALOG,
  selection: BrowserInventorySelection = {
    scope: "full-development-suite",
    requiredCatalogIds: [...catalog.keys()],
  },
): BrowserInventoryPlan {
  if (!/^[0-9a-f]{64}$/.test(catalogSha256)) {
    throw new Error("Missing inventory catalog digest");
  }
  const all = new Set(discovered);
  const wanted = new Set(selected);
  const required = new Set(selection.requiredCatalogIds);
  if (
    required.size !== selection.requiredCatalogIds.length ||
    [...required].some((id) => !catalog.has(id)) ||
    !["full-development-suite", "explicit-development-selection"].includes(
      selection.scope,
    ) ||
    (selection.scope === "full-development-suite" &&
      required.size !== catalog.size)
  ) throw new Error("Invalid required inventory selection");
  if (all.size !== discovered.length || wanted.size !== selected.length) {
    throw new Error("Duplicate test inventory identity");
  }
  for (const id of [...discovered, ...selected]) {
    if (canonicalTestId(id) !== id) {
      throw new Error("Noncanonical inventory ID");
    }
  }
  const issues: string[] = [];
  const unselectedMissingCatalogSources: string[] = [];
  const unselectedUnknownBrowserSources: string[] = [];
  for (const id of catalog.keys()) {
    if (!all.has(id)) {
      if (required.has(id)) {
        issues.push("Required catalog source is missing: " + id);
      } else unselectedMissingCatalogSources.push(id);
    }
  }
  for (const id of required) {
    if (all.has(id) && !wanted.has(id)) {
      issues.push("Required catalog source is not selected: " + id);
    }
  }
  for (const id of wanted) {
    if (!all.has(id) || !browserDiscoverable(id)) {
      issues.push("Selected browser source is missing or unsupported: " + id);
    }
  }
  const entries: TestInventoryEntry[] = [...all].sort().map((id) => {
    const metadata = testInventoryMetadata(id, catalog);
    const sourceSha256 = hashes.get(id);
    if (!sourceSha256 || !/^[0-9a-f]{64}$/.test(sourceSha256)) {
      throw new Error("Missing test inventory source digest: " + id);
    }
    if (browserDiscoverable(id) && metadata.origin === "unknown") {
      if (wanted.has(id) || selection.scope === "full-development-suite") {
        issues.push("Browser source is not classified: " + id);
      } else unselectedUnknownBrowserSources.push(id);
    }
    return {
      id,
      sourceSha256,
      ...metadata,
      browserDiscoverable: browserDiscoverable(id),
      selected: wanted.has(id),
      requiredInSelection: required.has(id),
    };
  });
  return {
    schemaVersion: 1,
    scope: "colocated-source-files",
    selection: { ...selection, requiredCatalogIds: [...required].sort() },
    catalogSha256,
    entries,
    issues: issues.sort(),
    unselectedMissingCatalogSources: unselectedMissingCatalogSources.sort(),
    unselectedUnknownBrowserSources: unselectedUnknownBrowserSources.sort(),
    upstreamQuarantine: structuredClone(upstreamQuarantine),
    counts: {
      discovered: entries.length,
      browserDiscoverable:
        entries.filter((entry) => entry.browserDiscoverable).length,
      catalogBrowserFiles: catalog.size,
      missingCatalogSources:
        [...catalog.keys()].filter((id) => !all.has(id)).length,
      selected: wanted.size,
      requiredCatalogFiles: required.size,
      packagedEligible: 0,
      notOnboarded:
        entries.filter((entry) => entry.packagedStatus === "not-onboarded")
          .length,
      unsupported:
        entries.filter((entry) => entry.packagedStatus === "unsupported")
          .length,
      unknown: entries.filter((entry) => entry.origin === "unknown").length,
      quarantinedColocated: 0,
      quarantinedUpstreamCandidates: upstreamQuarantine.length,
      executed: 0,
    },
    productVerdict: "UNVERIFIED",
    publicationAuthorized: false,
  };
}

export function requireExecutableBrowserPlan(plan: BrowserInventoryPlan): void {
  if (plan.counts.selected < 1) {
    throw new Error("Browser inventory selected no tests; execution refused");
  }
  if (plan.issues.length > 0) {
    throw new Error(
      "Browser inventory planning failed: " + plan.issues.join(" | "),
    );
  }
}

/** Exact file identity and classification; substring/suffix matches are forbidden. */
export function reconcileBrowserInventoryResults(
  plan: BrowserInventoryPlan,
  results: InventoryBrowserResult[],
): string[] {
  const expected = new Map(
    plan.entries.filter((entry) => entry.selected).map((
      entry,
    ) => [entry.id, entry]),
  );
  const seen = new Set<string>();
  const issues: string[] = [];
  for (const result of results) {
    let id: string;
    try {
      id = canonicalTestId(result.file);
    } catch {
      issues.push("Result has an invalid inventory identity");
      continue;
    }
    const entry = expected.get(id);
    if (!entry || seen.has(id)) {
      issues.push("Unexpected or duplicate inventory result: " + id);
      continue;
    }
    seen.add(id);
    if (
      result.inventoryVersion !== 1 || result.origin !== entry.origin ||
      result.executionKind !== entry.executionKind ||
      (entry.origin === "mozilla-converted") !==
        (result.source === "downloaded-firefox")
    ) {
      issues.push("Result inventory classification mismatch: " + id);
    }
  }
  return issues;
}
