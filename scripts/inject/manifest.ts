import * as fs from "node:fs/promises";

export async function injectManifest() {
  const manifest_chrome = (
    await fs.readFile("_dist/bin/chrome.manifest")
  ).toString();

  console.log(manifest_chrome);

  if (!manifest_chrome.includes("manifest noraneko/noraneko.manifest")) {
    await fs.writeFile(
      "_dist/bin/chrome.manifest",
      `${manifest_chrome}\nmanifest noraneko/noraneko.manifest`,
    );
  }

  try {
    await fs.access("_dist/bin/noraneko");
    await fs.rm("_dist/bin/noraneko");
  } catch {}
  const isWin = process.platform === "win32";

  await fs.symlink(
    path.relative(`${binPath}`, "./apps/main/_dist"),
    `${binPath}/noraneko`,
    isWin ? "junction" : undefined,
  );
}
