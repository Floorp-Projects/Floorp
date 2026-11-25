// SPDX-License-Identifier: MPL-2.0

import { join } from "@std/path";
import { hashDirectoryContents } from "./hub_menu_acceptance.ts";
import type { M3dProfileSnapshot } from "./m3d_package_acquisition_acceptance.types.ts";
import {
  M3D_REQUIRED_CASE_IDS,
  type M3dAcceptanceVerdictInput,
} from "./m3d_package_acquisition_acceptance.types.ts";

const MAX_PROFILE_ENTRIES = 100_000;
const MAX_PROFILE_BYTES = 1024 * 1024 * 1024;
const encoder = new TextEncoder();

export function m3dAcceptanceVerdict(
  input: M3dAcceptanceVerdictInput,
): boolean {
  if (
    !input.complete || input.launchFailure !== null ||
    input.runFailure !== null ||
    !input.launchAttempted || !input.launchSucceeded ||
    !input.browserCleanupProven || !input.sharedProfileBefore ||
    !input.sharedProfileAfter || Object.keys(input.cleanup).length === 0 ||
    !Object.values(input.cleanup).every((value) => value === true)
  ) return false;
  if (
    !m3dProfileSnapshotsEqual(
      input.sharedProfileBefore,
      input.sharedProfileAfter,
    )
  ) return false;
  const required = new Set<string>(M3D_REQUIRED_CASE_IDS);
  const seen = new Set<string>();
  for (const result of input.cases) {
    if (!required.has(result.id) || seen.has(result.id)) return false;
    seen.add(result.id);
    if (result.status !== "passed") return false;
  }
  return seen.size === required.size;
}

export function m3dProfileSnapshotsEqual(
  before: M3dProfileSnapshot | null,
  after: M3dProfileSnapshot | null,
): boolean {
  return before !== null && after !== null &&
    before.exists === after.exists && before.entries === after.entries &&
    before.byteLength === after.byteLength && before.sha256 === after.sha256;
}

async function digest(bytes: Uint8Array): Promise<string> {
  const copy = bytes.slice();
  const hash = new Uint8Array(await crypto.subtle.digest("SHA-256", copy));
  return Array.from(hash, (byte) => byte.toString(16).padStart(2, "0"))
    .join("");
}

export async function snapshotProfileTree(
  profilePath: string,
): Promise<M3dProfileSnapshot> {
  let root: Deno.FileInfo;
  try {
    root = await Deno.lstat(profilePath);
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) {
      return {
        exists: false,
        entries: 0,
        byteLength: 0,
        sha256: await digest(encoder.encode("missing")),
      };
    }
    throw new Error("profile snapshot failed");
  }
  if (!root.isDirectory || root.isSymlink) {
    throw new Error("profile snapshot root is not a directory");
  }

  let entries = 0;
  let byteLength = 0;
  const visit = async (directory: string, relative: string): Promise<void> => {
    const children: Deno.DirEntry[] = [];
    for await (const entry of Deno.readDir(directory)) children.push(entry);
    children.sort((left, right) => left.name.localeCompare(right.name));
    for (const entry of children) {
      entries++;
      if (entries > MAX_PROFILE_ENTRIES) {
        throw new Error("profile snapshot entry limit exceeded");
      }
      const childPath = join(directory, entry.name);
      const childRelative = relative ? `${relative}/${entry.name}` : entry.name;
      if (entry.isSymlink) {
        continue;
      } else if (entry.isDirectory) {
        await visit(childPath, childRelative);
      } else if (entry.isFile) {
        byteLength += (await Deno.lstat(childPath)).size;
        if (byteLength > MAX_PROFILE_BYTES) {
          throw new Error("profile snapshot byte limit exceeded");
        }
      }
    }
  };
  try {
    await visit(profilePath, "");
  } catch {
    throw new Error("profile snapshot failed");
  }
  return {
    exists: true,
    entries,
    byteLength,
    sha256: await hashDirectoryContents(profilePath),
  };
}
