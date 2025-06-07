import { $ } from "npm:zx@^8.5.3/core";
import {
  binDir,
  binPath,
  binPathExe,
  binRootDir,
  binVersion,
  brandingBaseName,
  brandingName,
  getBinArchive,
  VERSION,
} from "../../build/defines.ts";
import { isExists } from "../../build/utils.ts";
import { downloadBinArchive } from "./downloadBin.ts";
import pathe from "pathe";

// Binary archive filename
const binArchive = getBinArchive();

/**
 * Decompresses the binary archive based on the platform.
 */
export async function decompressBin() {
  try {
    console.log(`[dev] decompressing ${binArchive}`);

    if (!(await isExists(binArchive))) {
      console.log(
        `[dev] ${binArchive} not found. We will download ${await getBinArchive()} from GitHub latest release.`,
      );
      await downloadBinArchive();
    }

    switch (Deno.build.os) {
      case "windows":
        // Windows
        await $`Expand-Archive -LiteralPath ${binArchive} -DestinationPath ${binRootDir}`;
        break;

      case "darwin":
        // macOS
        await decompressBinMacOS();
        break;

      case "linux":
        // Linux
        await $`tar -xf ${binArchive} -C ${binRootDir}`;
        await $`chmod ${["-R", "755", `./${binDir}`]}`;
        await $`chmod ${["755", binPathExe]}`;
        break;
    }

    await Deno.writeTextFile(binVersion, VERSION);

    console.log("[dev] decompress complete!");
  } catch (error: any) {
    console.error("[dev] Error during decompression:", error);
    Deno.exit(1);
  }
}

/**
 * Decompresses the binary archive on macOS.
 */
async function decompressBinMacOS() {
  const macConfig = {
    mountDir: "_dist/mount",
    appDirName: "Contents",
    resourcesDirName: "Resources",
  };
  const mountDir = macConfig.mountDir;

  try {
    await Deno.mkdir(mountDir, { recursive: true });
    await $`hdiutil ${["attach", "-mountpoint", mountDir, binArchive]}`;

    try {
      const appDestBase = pathe.join(
        "./_dist/bin",
        brandingBaseName,
        `${brandingName}.app`,
      );
      const destContents = pathe.join(appDestBase, macConfig.appDirName);
      await Deno.mkdir(destContents, { recursive: true });
      await Deno.mkdir(binDir, { recursive: true });

      const srcContents = pathe.join(
        mountDir,
        `${brandingName}.app/${macConfig.appDirName}`,
      );
      const items: Deno.DirEntry[] = [];
      for await (const item of Deno.readDir(srcContents)) {
        items.push(item);
      }

      for (const item of items) {
        const srcItem = pathe.join(srcContents, item.name);
        if (item.name === macConfig.resourcesDirName) {
          const resourcesItems: Deno.DirEntry[] = [];
          for await (const resItem of Deno.readDir(srcItem)) {
            resourcesItems.push(resItem);
          }
          await Promise.all(
            resourcesItems.map((resItem) =>
              Deno.copyFile(
                pathe.join(srcItem, resItem.name),
                pathe.join(binDir, resItem.name),
              )
            ),
          );
        } else {
          await Deno.copyFile(srcItem, pathe.join(destContents, item.name));
        }
      }

      await Promise.all([
        $`chmod ${["-R", "777", appDestBase]}`,
        $`xattr ${["-rc", appDestBase]}`,
      ]);
    } finally {
      await $`hdiutil ${["detach", mountDir]}`;
      await Deno.remove(mountDir, { recursive: true });
    }
  } catch (error) {
    console.error("[dev] Error during macOS decompression:", error);
    Deno.exit(1);
  }
}
