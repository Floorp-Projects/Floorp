// TODO: Replace with official import for getBinArchive
const getBinArchive = () => "noraneko-bin.tar.xz";
const binArchive = getBinArchive();

/**
 * Get git remote origin URL
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
 * Download file from specified URL
 */
async function downloadFile(url: string, outputPath: string): Promise<void> {
  console.log(`[dev] Downloading: ${url}`);
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`HTTP error: status ${response.status}`);
  }
  const fileData = await response.arrayBuffer();
  await Deno.writeFile(outputPath, new Uint8Array(fileData));
}

/**
 * Download binary archive from GitHub release
 */
export async function downloadBinArchive() {
  const fileName = binArchive;
  let originUrl = await getGitOriginUrl();
  if (originUrl.endsWith("/")) originUrl = originUrl.slice(0, -1);
  const originDownloadUrl =
    `${originUrl}-runtime/releases/latest/download/${fileName}`;
  console.log(`[dev] Downloading from origin: ${originDownloadUrl}`);
  try {
    await downloadFile(originDownloadUrl, binArchive);
    console.log("[dev] Download from origin complete!");
  } catch (error) {
    console.error(
      "[dev] Download from origin failed. Falling back to upstream:",
      error instanceof Error ? error.message : String(error),
    );
    const upstreamUrl =
      `https://github.com/nyanrus/noraneko-runtime/releases/latest/download/${fileName}`;
    console.log(`[dev] Downloading from upstream: ${upstreamUrl}`);
    try {
      await downloadFile(upstreamUrl, binArchive);
      console.log("[dev] Download from upstream complete!");
    } catch (error2) {
      console.error(
        "[dev] Download from upstream failed:",
        error2 instanceof Error ? error2.message : String(error2),
      );
      throw error2;
    }
  }
}
