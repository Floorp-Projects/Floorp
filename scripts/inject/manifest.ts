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
    await fs.rm(`${binPath}/noraneko`, { recursive: true });
  } catch {}
  const isWin = process.platform === "win32";

  await fs.mkdir(`${binPath}/noraneko`);

  await fs.writeFile(
    `${binPath}/noraneko/noraneko.manifest`,
    `content noraneko content/
content noraneko-startup startup/ contentaccessible=yes
skin noraneko classic/1.0 skin/
resource noraneko resource/ contentaccessible=yes`,
  );

  await fs.symlink(
    path.relative(`${binPath}/noraneko`, "./apps/main/_dist"),
    `${binPath}/noraneko/content`,
    isWin ? "junction" : undefined,
  );

  await fs.symlink(
    path.relative(`${binPath}/noraneko`, "./apps/startup/_dist"),
    `${binPath}/noraneko/startup`,
    isWin ? "junction" : undefined,
  );

  await fs.symlink(
    path.relative(`${binPath}/noraneko`, "./apps/designs/_dist"),
    `${binPath}/noraneko/skin`,
    isWin ? "junction" : undefined,
  );
}
