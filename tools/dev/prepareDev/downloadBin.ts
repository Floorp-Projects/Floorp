import { getBinArchive } from "../../build/defines.ts";

// Binary archive filename
const binArchive = getBinArchive();

/**
 * Gets the git remote origin URL using Deno's subprocess API
 */
async function getGitOriginUrl(): Promise<string> {
  const command = new Deno.Command("git", {
    args: ["remote", "get-url", "origin"],
    stdout: "piped",
    stderr: "piped",
  });

  const { code, stdout, stderr } = await command.output();

  if (code !== 0) {
    const errorText = new TextDecoder().decode(stderr);
    throw new Error(`Git command failed: ${errorText}`);
  }

  return new TextDecoder().decode(stdout).trim();
}

/**
 * Downloads a file from URL using Deno's fetch API
 */
async function downloadFile(url: string, outputPath: string): Promise<void> {
  console.log(`[dev] Fetching ${url}...`);

  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`HTTP error! status: ${response.status}`);
  }

  const fileData = await response.arrayBuffer();
  await Deno.writeFile(outputPath, new Uint8Array(fileData));
}

/**
 * Downloads the binary archive from GitHub releases.
 */
export async function downloadBinArchive() {
  const fileName = getBinArchive();
  let originUrl = await getGitOriginUrl();
  if (originUrl.endsWith("/")) {
    originUrl = originUrl.slice(0, -1);
  }
  const originDownloadUrl =
    `${originUrl}-runtime/releases/latest/download/${fileName}`;

  console.log(`[dev] Downloading from origin: ${originDownloadUrl}`);
  try {
    await downloadFile(originDownloadUrl, binArchive);
    console.log("[dev] Download complete from origin!");
  } catch (error: unknown) {
    console.error(
      "[dev] Origin download failed, falling back to upstream:",
      error instanceof Error ? error.message : String(error),
    );

    const upstreamUrl =
      `https://github.com/nyanrus/noraneko-runtime/releases/latest/download/${fileName}`;
    console.log(`[dev] Downloading from upstream: ${upstreamUrl}`);

    try {
      await downloadFile(upstreamUrl, binArchive);
      console.log("[dev] Download complete from upstream!");
    } catch (error2: unknown) {
      console.error(
        "[dev] Upstream download failed:",
        error2 instanceof Error ? error2.message : String(error2),
      );
      throw error2;
    }
  }
}
