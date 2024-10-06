import * as fs from "node:fs/promises";
import path from "node:path";

export async function injectManifest(binPath: string, isDev: boolean) {
  if (isDev) {
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
  }

  try {
    await fs.access(`${binPath}/noraneko`);
    await fs.rm(`${binPath}/noraneko`, { recursive: true });
  } catch {}
  const isWin = process.platform === "win32";

  await fs.mkdir(`${binPath}/noraneko`, { recursive: true });

  await fs.writeFile(
    `${binPath}/noraneko/noraneko.manifest`,
    `content noraneko content/ contentaccessible=yes
content noraneko-startup startup/ contentaccessible=yes
skin noraneko classic/1.0 skin/
resource noraneko resource/ contentaccessible=yes`,
  );

  await fs.symlink(
    path.relative(`${binPath}/noraneko`, "./src/apps/main/_dist"),
    `${binPath}/noraneko/content`,
    isWin ? "junction" : undefined,
  );

  await fs.symlink(
    path.relative(`${binPath}/noraneko`, "./src/apps/startup/_dist"),
    `${binPath}/noraneko/startup`,
    isWin ? "junction" : undefined,
  );

  await fs.symlink(
    path.relative(`${binPath}/noraneko`, "./src/apps/designs/_dist"),
    `${binPath}/noraneko/skin`,
    isWin ? "junction" : undefined,
  );

  await fs.symlink(
    path.relative(`${binPath}/noraneko`, "./src/apps/modules/_dist"),
    `${binPath}/noraneko/resource`,
    isWin ? "junction" : undefined,
  );
}
