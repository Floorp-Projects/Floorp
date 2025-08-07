import {
  binDir,
  binPathExe,
  binRootDir,
  binVersion,
  VERSION,
} from "../../utils/defines.ts";
import { ensureDir, isExists, safeRemove } from "../../utils/utils.ts";
import { decompressBin } from "./decompressBin.ts";

/**
 * Binary initialization process. Checks version and existence, extracts if needed.
 */
export async function initBin() {
  const hasVersion = await isExists(binVersion);
  const hasBin = await isExists(binPathExe);
  let needInit = false;

  if (hasBin && hasVersion) {
    const version = (await Deno.readTextFile(binVersion)).trim();
    if (VERSION !== version) {
      console.log(
        `[dev] Version mismatch: ${version} !== ${VERSION}. Re-extracting.`,
      );
      await safeRemove(binRootDir);
      needInit = true;
    } else {
      console.log("[dev] Binary version matches. No initialization needed.");
    }
  } else if (hasBin && !hasVersion) {
    console.log(
      `[dev] Binary exists but version file is missing. Writing ${VERSION}.`,
    );
    await ensureDir(binDir);
    await Deno.writeTextFile(binVersion, VERSION);
    console.log("[dev] Initialization complete.");
  } else if (!hasBin && hasVersion) {
    console.error(
      "[dev] Version file exists but binary is missing. Abnormal termination.",
    );
    throw new Error("Unreachable: !hasBin && hasVersion");
  } else {
    console.log("[dev] Binary not found. Extracting.");
    needInit = true;
  }

  if (needInit) {
    await ensureDir(binDir);
    await decompressBin();
    console.log("[dev] Initialization complete.");
  }
}

// Entry point when run directly
if (import.meta.main) {
  initBin().catch((error) => {
    console.error("[dev] Binary initialization failed:", error);
    Deno.exit(1);
  });
}
