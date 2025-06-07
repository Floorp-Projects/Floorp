import {
  binDir,
  binPathExe,
  binRootDir,
  binVersion,
  VERSION,
} from "../../build/defines.ts";
import { isExists } from "../../build/utils.ts";
import { decompressBin } from "./decompressBin.ts";

/**
 * Initializes the binary by checking for existing version and binary files.
 */
export async function initBin() {
  const hasVersion = await isExists(binVersion);
  const hasBin = await isExists(binPathExe);

  let isBinInitNeeded = false;

  if (hasBin) {
    if (hasVersion) {
      const version = await Deno.readTextFile(binVersion);
      const mismatch = VERSION !== version.trim();

      if (mismatch) {
        console.log(`[dev] version mismatch ${version.trim()} !== ${VERSION}`);
        console.log(
          "[dev] Removing existing binary directory and decompressing new binary.",
        );
        await Deno.remove(binRootDir, { recursive: true });
        isBinInitNeeded = true;
      } else {
        // if (!mismatch)
        console.log("[dev] Binary version matches.");
      }
    } else {
      // if (!hasVersion)
      console.log(
        `[dev] Binary exists, but version file not found. Writing ${VERSION} to version file.`,
      );
      await Deno.mkdir(binDir, { recursive: true });
      await Deno.writeTextFile(binVersion, VERSION);
      console.log("[dev] Initialization complete.");
    }
  } else {
    // if (!hasBin)
    if (hasVersion) {
      console.error("[dev] Unreachable : there is metadata version of binary");
      throw Error("Unreachable: !hasBin && hasVersion");
    } else {
      // if (!hasVersion)
      console.log("[dev] No binary found. Decompressing binary.");
      isBinInitNeeded = true;
    }
  }

  if (isBinInitNeeded) {
    await Deno.mkdir(binDir, { recursive: true });
    await decompressBin();
    console.log("[dev] Initialization complete.");
  }
}

// Direct execution support
if (import.meta.main) {
  try {
    await initBin();
  } catch (error) {
    console.error("[dev] Failed to initialize binary:", error);
    Deno.exit(1);
  }
}
