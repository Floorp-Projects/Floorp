import * as fs from "node:fs/promises";

export async function injectManifest() {
  const manifest_chrome = (await fs.readFile("dist/bin/chrome.manifest")).toString();

  if (!manifest_chrome.includes("manifest noraneko/noraneko.manifest")) {
    await fs.writeFile("dist/bin/chrome.manifest", manifest_chrome + "\nmanifest noraneko/noraneko.manifest");
  }

  try {
    await fs.access("dist/bin/noraneko");
    await fs.rm("dist/bin/noraneko");
  } catch {}
  await fs.symlink("../noraneko", "dist/bin/noraneko", "junction");
}
