// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import {
  parseTestSourceManifest,
  testSourceManifestFilename,
} from "../../libs/test-inventory/source-manifest.ts";
import { sourceDigest } from "../../libs/test-inventory/source-files.ts";
import type { TestSourceContext } from "../../libs/test-inventory/source-types.ts";
import type { BrowserInventoryPlan } from "../../libs/test-inventory/types.ts";

/** Append-only evidence. A new run never replaces or retags an earlier plan. */
export async function writeTestSourceManifest(
  inventory: BrowserInventoryPlan,
  runId: string,
  expiresAtMs: number,
  profileDir: string,
  inventoryPath?: string,
): Promise<TestSourceContext> {
  const manifest = parseTestSourceManifest({
    schemaVersion: 1,
    scope: "vite-test-source-input",
    runId,
    expiresAtMs,
    sources: inventory.entries.filter((entry) => entry.browserDiscoverable)
      .map((entry) => ({ id: entry.id, sha256: entry.sourceSha256 })),
  });
  const bytes = new TextEncoder().encode(JSON.stringify(manifest) + "\n");
  if (bytes.length > 2 * 1024 * 1024) {
    throw new Error("Test source manifest bound exceeded");
  }
  await Deno.mkdir(profileDir, { recursive: true });
  await Deno.writeFile(
    path.join(profileDir, testSourceManifestFilename(runId)),
    bytes,
    {
      createNew: true,
      mode: 0o600,
    },
  );
  if (inventoryPath) {
    await Deno.writeFile(path.resolve(inventoryPath) + ".source.json", bytes, {
      createNew: true,
      mode: 0o600,
    });
  }
  return { schemaVersion: 1, runId, manifestSha256: sourceDigest(bytes) };
}
