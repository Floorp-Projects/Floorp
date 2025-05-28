import { $ } from "npm:zx@^8.5.3/core";
import { getBinArchive } from "../defines.ts";

// Binary archive filename
const binArchive = getBinArchive();

/**
 * Downloads the binary archive from GitHub releases.
 */
export async function downloadBinArchive() {
  const fileName = getBinArchive();
  let originUrl = (await $`git remote get-url origin`).stdout.trim();
  if (originUrl.endsWith("/")) {
    originUrl = originUrl.slice(0, -1);
  }
  const originDownloadUrl =
    `${originUrl}-runtime/releases/latest/download/${fileName}`;

  console.log(`[dev] Downloading from origin: ${originDownloadUrl}`);
  try {
    await $`curl -L --fail --progress-bar -o ${binArchive} ${originDownloadUrl}`;
    console.log("[dev] Download complete from origin!");
  } catch (error: any) {
    console.error(
      "[dev] Origin download failed, falling back to upstream:",
      error.stderr,
    );

    const upstreamUrl =
      `https://github.com/nyanrus/noraneko-runtime/releases/latest/download/${fileName}`;
    console.log(`[dev] Downloading from upstream: ${upstreamUrl}`);

    try {
      await $`curl -L --fail --progress-bar -o ${binArchive} ${upstreamUrl}`;
      console.log("[dev] Download complete from upstream!");
    } catch (error2: any) {
      console.error("[dev] Upstream download failed:", error2.stderr);
      throw error2.stderr;
    }
  }
}
