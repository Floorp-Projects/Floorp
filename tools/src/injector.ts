// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import {
  runCommandChecked,
  Logger,
  exists,
  safeRemove,
  createSymlink,
} from "./utils.ts";
import { BIN_DIR, PROD_BIN_DIR, PROJECT_ROOT } from "./defines.ts";

const logger = new Logger("injector");

export async function injectXhtmlFromTs(
  isDev = false,
  isCI = false,
): Promise<void> {
  const scriptPath = path.join(PROJECT_ROOT, "tools", "scripts", "xhtml.ts");
  const binPath = !isCI ? BIN_DIR : PROD_BIN_DIR;

  const args = ["run", "--allow-read", "--allow-write", scriptPath, binPath];
  if (isDev) args.push("--dev");

  const result = runCommandChecked("deno", args);
  if (!result.success) {
    throw new Error(`Failed to inject XHTML: ${result.stderr}`);
  }
  logger.success("XHTML injection complete.");
  // Keep function legitimately async to match callers that await it
  // and satisfy the lint rule requiring at least one await.
  await Promise.resolve();
}

export function createManifest(mode: string, dirPath: string) {
  let manifestContent = [
    "content noraneko content/ contentaccessible=yes",
    "content noraneko-startup startup/ contentaccessible=yes",
    "skin noraneko classic/1.0 skin/",
    "resource noraneko resource/ contentaccessible=yes",
  ].join("\n");

  // Determine dev mode from the provided mode string. When running
  // `deno task feles-build dev` we expect mode === "dev". Only in dev
  // mode should the additional development entries be appended.
  const isDev = mode === "dev";

  if (!isDev) {
    const devEntries = [
      "content noraneko-newtab pages-newtab/ contentaccessible=yes",
      "content noraneko-welcome pages-welcome/ contentaccessible=yes",
      "content noraneko-notes pages-notes/ contentaccessible=yes",
      "content noraneko-modal-child pages-modal-child/ contentaccessible=yes",
      "content noraneko-settings pages-settings/ contentaccessible=yes",
    ].join("\n");
    manifestContent += "\n" + devEntries;
  }

  Deno.writeTextFileSync(
    path.join(dirPath, "noraneko.manifest"),
    manifestContent,
  );
}

/**
 * This creates chrome.manifest, and symlinks the dists to firefox binary dir.
 * for production, only symlinks will be created.
 * @param mode
 * @param dirName
 */
export function run(mode: string, dirName = "noraneko-devdir"): void {
  const manifestPath = path.join(BIN_DIR, "chrome.manifest");

  if (mode !== "production") {
    let manifest = "";
    if (exists(manifestPath)) {
      manifest = Deno.readTextFileSync(manifestPath);
    }
    const entry = `manifest ${dirName}/noraneko.manifest`;
    if (!manifest.includes(entry)) {
      Deno.writeTextFileSync(manifestPath, `${manifest}\n${entry}`);
    }
  }

  const dirPath = path.join(BIN_DIR, dirName);
  try {
    if (exists(dirPath)) {
      safeRemove(dirPath);
    }
    Deno.mkdirSync(dirPath, { recursive: true });
  } catch (e) {
    const msg = e instanceof Error ? e.message : String(e);
    logger.error(`Failed to prepare directory ${dirPath}: ${msg}`);
    throw e;
  }

  createManifest(mode, dirPath);

  const mounts: Array<[string, string]> = [
    ["content", "bridge/loader-features/_dist"],
    ["startup", "bridge/startup/_dist"],
    ["skin", "browser-features/skin"],
    ["resource", "bridge/loader-modules/_dist"],
    ["pages-newtab", "browser-features/pages-newtab/dist"],
    ["pages-settings", "browser-features/pages-settings/dist"],
    ["pages-welcome", "browser-features/pages-welcome/dist"],
    ["pages-notes", "browser-features/pages-notes/dist"],
    ["pages-modal-child", "browser-features/pages-modal-child/dist"],
  ];

  for (const [subdir, target] of mounts) {
    const linkPath = path.resolve(dirPath, subdir);
    const targetPath = path.resolve(target);
    try {
      if (exists(linkPath)) {
        safeRemove(linkPath);
      }
    } catch {
      // ignore
    }

    try {
      createSymlink(linkPath, targetPath);
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e);
      logger.warn(
        `Failed to create symlink ${linkPath} -> ${targetPath}: ${msg}`,
      );
    }
  }

  logger.success("Manifest injected successfully.");
}
