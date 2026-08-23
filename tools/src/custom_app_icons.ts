// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { BIN_DIR, PATHS, PLATFORM } from "./defines.ts";
import { exists, Logger } from "./utils.ts";

const logger = new Logger("custom-app-icons");

const SOURCE_DIR = path.join(PATHS.root, "static", "gecko", "custom-app-icons");
const RUNTIME_ICON_DIR = path.join(
  BIN_DIR,
  "browser",
  "chrome",
  "browser",
  "content",
  "browser",
  "icons",
);

/**
 * Copy Floorp's browser-icon picker assets into the unpacked Runtime bundle.
 *
 * The Runtime artifact already contains the Firefox icon-picker code and the
 * chrome directory is intentionally writable in the local artifact workflow.
 * Keeping this synchronization in Floorp means the Runtime repository does
 * not need to carry Floorp-specific artwork.
 */
export function run(): void {
  if (PLATFORM !== "windows") {
    return;
  }

  if (!exists(SOURCE_DIR)) {
    logger.warn(`Icon source directory does not exist: ${SOURCE_DIR}`);
    return;
  }

  Deno.mkdirSync(RUNTIME_ICON_DIR, { recursive: true });

  let copied = 0;
  for (const entry of Deno.readDirSync(SOURCE_DIR)) {
    if (!entry.isFile || !entry.name.endsWith(".png")) {
      continue;
    }

    Deno.copyFileSync(
      path.join(SOURCE_DIR, entry.name),
      path.join(RUNTIME_ICON_DIR, entry.name),
    );
    copied += 1;
  }

  logger.info(`Synchronized ${copied} Floorp browser icon asset(s).`);
}
