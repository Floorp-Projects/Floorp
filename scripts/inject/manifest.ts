import { symlink } from "@std/fs/unstable-symlink";
import { emptyDir } from "@std/fs/empty-dir";
import { relative, resolve } from "npm:pathe@^2.0.2";
import {SymlinkOptions} from '@std/fs/unstable-types'
import {ensureDir} from "@std/fs/ensure-dir"

export async function injectManifest(
  binPath: string,
  isDev: boolean,
  dirName = "noraneko",
) {
  if (isDev) {
    const manifest_chrome = await Deno.readTextFile(`${binPath}/chrome.manifest`);

    console.log(manifest_chrome);

    if (!manifest_chrome.includes(`manifest ${dirName}/noraneko.manifest`)) {
      await Deno.writeTextFile(
        `${binPath}/chrome.manifest`,
        `${manifest_chrome}\nmanifest ${dirName}/noraneko.manifest`,
      );
    }
  }

  await emptyDir(`${binPath}/${dirName}`)

  const option: SymlinkOptions= {type:"dir"}

  await ensureDir(`${binPath}/${dirName}`);

  await Deno.writeTextFile(
    `${binPath}/${dirName}/noraneko.manifest`,
    `content noraneko content/ contentaccessible=yes
content noraneko-startup startup/ contentaccessible=yes
skin noraneko classic/1.0 skin/
resource noraneko resource/ contentaccessible=yes
${!isDev ? "\ncontent noraneko-settings settings/ contentaccessible=yes" : ""}`,
  );

  await symlink(resolve(import.meta.dirname,"../../src/apps/main/_dist"),`${binPath}/${dirName}/content`,option)

  await symlink(resolve(import.meta.dirname,"../../src/apps/startup/_dist"),`${binPath}/${dirName}/startup`,option)
  await symlink(resolve(import.meta.dirname,"../../src/apps/designs/_dist"),`${binPath}/${dirName}/skin`,option)
  await symlink(resolve(import.meta.dirname,"../../src/apps/modules/_dist"),`${binPath}/${dirName}/resource`,option)

  if (!isDev) {
    await symlink(relative(`${binPath}/${dirName}`, "./src/apps/settings/_dist"),`${binPath}/${dirName}/settings`,option)
  }
}
