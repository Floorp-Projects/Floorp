import * as fs from "node:fs/promises";
import path from "node:path";

export async function injectManifest(
  binPath: string,
  isDev: boolean,
  dirName = "noraneko",
) {
  if (isDev) {
    const manifest_chrome = (
      await fs.readFile(`${binPath}/chrome.manifest`)
    ).toString();

    console.log(manifest_chrome);

    if (!manifest_chrome.includes(`manifest ${dirName}/noraneko.manifest`)) {
      await fs.writeFile(
        `${binPath}/chrome.manifest`,
        `${manifest_chrome}\nmanifest ${dirName}/noraneko.manifest`,
      );
    }
  }

  try {
    await fs.access(`${binPath}/${dirName}`);
    await fs.rm(`${binPath}/${dirName}`, { recursive: true });
  } catch {}
  const isWin = process.platform === "win32";

  await fs.mkdir(`${binPath}/${dirName}`, { recursive: true });

  await fs.writeFile(
    `${binPath}/${dirName}/noraneko.manifest`,
    `content noraneko content/ contentaccessible=yes
content noraneko-startup startup/ contentaccessible=yes
skin noraneko classic/1.0 skin/
resource noraneko resource/ contentaccessible=yes
${!isDev ? "\ncontent noraneko-settings settings/ contentaccessible=yes" : ""}`,
  );

  await fs.symlink(
    path.relative(`${binPath}/${dirName}`, "./src/apps/main/_dist"),
    `${binPath}/${dirName}/content`,
    isWin ? "junction" : undefined,
  );

  await fs.symlink(
    path.relative(`${binPath}/${dirName}`, "./src/apps/startup/_dist"),
    `${binPath}/${dirName}/startup`,
    isWin ? "junction" : undefined,
  );

  await fs.symlink(
    path.relative(`${binPath}/${dirName}`, "./src/apps/designs/_dist"),
    `${binPath}/${dirName}/skin`,
    isWin ? "junction" : undefined,
  );

  await fs.symlink(
    path.relative(`${binPath}/${dirName}`, "./src/apps/modules/_dist"),
    `${binPath}/${dirName}/resource`,
    isWin ? "junction" : undefined,
  );

  if (!isDev) {
    await fs.symlink(
      path.relative(`${binPath}/${dirName}`, "./src/apps/settings/_dist"),
      `${binPath}/${dirName}/settings`,
      isWin ? "junction" : undefined,
    );
  }
}
