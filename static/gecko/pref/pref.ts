// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { runCommandChecked, Logger, exists } from "../../../tools/src/utils.ts";
import { BIN_DIR } from "../../../tools/src/defines.ts";

const logger = new Logger("pref");

const PREF_OVERRIDE_DIR = "static/gecko/pref";

function binDir(): string {
  return BIN_DIR;
}

export function prefNeeded(): boolean {
  const overrideIniPath = path.join(PREF_OVERRIDE_DIR, "override.ini");

  // If override.ini doesn't exist, no pref override needed
  if (!exists(overrideIniPath)) return false;

  // Always return true if override.ini exists
  return true;
}

export function applyPrefs(): void {
  if (!prefNeeded()) {
    logger.info("No preference override needed to apply.");
    return;
  }

  const firefoxJsPath = path.join(
    binDir(),
    "browser",
    "defaults",
    "preferences",
    "firefox.js",
  );

  // Check if firefox.js exists
  if (!exists(firefoxJsPath)) {
    logger.warn(
      `firefox.js not found at ${firefoxJsPath}, skipping preferences override`,
    );
    return;
  }

  // Determine which script to use based on platform
  const isWindows = Deno.build.os === "windows";
  const scriptName = isWindows ? "override.ps1" : "override.sh";
  const overrideScriptPath = path.join(PREF_OVERRIDE_DIR, scriptName);
  const overrideIniPath = path.join(PREF_OVERRIDE_DIR, "override.ini");

  // Check if override script exists
  if (!exists(overrideScriptPath)) {
    logger.warn(`${scriptName} not found, skipping preferences override`);
    return;
  }

  // Check if override.ini exists
  if (!exists(overrideIniPath)) {
    logger.warn("override.ini not found, skipping preferences override");
    return;
  }

  try {
    let result;

    if (isWindows) {
      // Run PowerShell script on Windows
      result = runCommandChecked(
        "powershell.exe",
        [
          "-ExecutionPolicy",
          "Bypass",
          "-File",
          overrideScriptPath,
          firefoxJsPath,
        ],
        undefined,
      );
    } else {
      // Run bash script on Unix-like systems
      runCommandChecked("chmod", ["+x", overrideScriptPath], undefined);
      result = runCommandChecked(
        overrideScriptPath,
        [firefoxJsPath],
        undefined,
      );
    }

    if (!result.success) {
      logger.error("Failed to apply preferences override");
      logger.error(result.stderr);
      if (result.stdout.trim()) {
        logger.error("Script output:");
        console.error(result.stdout);
      }
      throw new Error("Preference override failed");
    }

    logger.success("Preferences override applied successfully");
  } catch (error) {
    logger.error(`Failed to apply preferences override: ${error}`);
    throw error;
  }
}

export function run(): void {
  applyPrefs();
}
