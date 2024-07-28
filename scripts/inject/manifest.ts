import * as fs from "node:fs/promises";
import path from "node:path";

export async function injectManifest(binPath: string) {
  const manifest_chrome = (
    await fs.readFile(`${binPath}/chrome.manifest`)
  ).toString();

  console.log(manifest_chrome);

  if (!manifest_chrome.includes("manifest noraneko/noraneko.manifest")) {
    await fs.writeFile(
      `${binPath}/chrome.manifest`,
      `${manifest_chrome}\nmanifest noraneko/noraneko.manifest`,
    );
  }

  try {
    await fs.access(`${binPath}/noraneko`);
    await fs.rm(`${binPath}/noraneko`);
  } catch {}
  const isWin = process.platform === "win32";

  await fs.symlink(
    path.relative(`${binPath}`, "./apps/main/_dist"),
    `${binPath}/noraneko`,
    isWin ? "junction" : undefined,
  );
}
