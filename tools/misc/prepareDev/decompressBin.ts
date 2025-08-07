import {
  binDir,
  binPathExe,
  binRootDir,
  binVersion,
  brandingBaseName,
  brandingName,
  getBinArchive,
  VERSION,
} from "../../utils/defines.ts";
import {
  ensureDir,
  isExists,
  runCommand,
  safeRemove,
} from "../../utils/utils.ts";
import { downloadBinArchive } from "./downloadBin.ts";
import pathe from "pathe";

// Binary archive file name
const binArchive = getBinArchive().filename;

/**
 * Run PowerShell command for Windows
 */
async function runPowerShellCommand(script: string): Promise<void> {
  const cmd = new Deno.Command("pwsh", {
    args: ["-Command", script],
    stdout: "piped",
    stderr: "piped",
  });
  const { code, stderr } = await cmd.output();
  if (code !== 0) {
    const errorText = new TextDecoder().decode(stderr);
    throw new Error(`PowerShell command failed: ${script}\n${errorText}`);
  }
}

/**
 * Extract binary archive for each platform
 */
export async function decompressBin() {
  try {
    console.log(`[dev] Binary extraction started: ${binArchive}`);
    if (!(await isExists(binArchive))) {
      console.log(
        `[dev] ${binArchive} not found. Downloading from GitHub release.`,
      );
      await downloadBinArchive();
    }
    switch (Deno.build.os) {
      case "windows":
        await runPowerShellCommand(
          `Expand-Archive -LiteralPath "${binArchive}" -DestinationPath "${binRootDir}"`,
        );
        break;
      case "darwin":
        await decompressBinMacOS();
        break;
      case "linux":
        await runCommand("tar", ["-xf", binArchive, "-C", binRootDir]);
        await runCommand("chmod", ["-R", "755", `./${binDir}`]);
        await runCommand("chmod", ["755", binPathExe]);
        break;
    }
    await Deno.writeTextFile(binVersion, VERSION);
    console.log("[dev] Extraction complete!");
  } catch (error) {
    console.error("[dev] Error during extraction:", error);
    Deno.exit(1);
  }
}

/**
 * Extract binary archive for macOS
 */
async function decompressBinMacOS() {
  const macConfig = {
    mountDir: "_dist/mount",
    appDirName: "Contents",
    resourcesDirName: "Resources",
  };
  const mountDir = macConfig.mountDir;
  try {
    await ensureDir(mountDir);
    await runCommand("hdiutil", [
      "attach",
      "-mountpoint",
      mountDir,
      binArchive,
    ]);
    try {
      const appDestBase = pathe.join(
        "./_dist/bin",
        brandingBaseName,
        `${brandingName}.app`,
      );
      const destContents = pathe.join(appDestBase, macConfig.appDirName);
      await ensureDir(destContents);
      await ensureDir(binDir);
      const srcContents = pathe.join(
        mountDir,
        `${brandingName}.app/${macConfig.appDirName}`,
      );
      const items: Deno.DirEntry[] = [];
      for await (const item of Deno.readDir(srcContents)) items.push(item);
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
        runCommand("chmod", ["-R", "777", appDestBase]),
        runCommand("xattr", ["-rc", appDestBase]),
      ]);
    } finally {
      await runCommand("hdiutil", ["detach", mountDir]);
      await safeRemove(mountDir);
    }
  } catch (error) {
    console.error("[dev] Error during macOS extraction:", error);
    Deno.exit(1);
  }
}

// Entry point when run directly
if (import.meta.main) {
  decompressBin().catch((error) => {
    console.error("[dev] Binary extraction failed:", error);
    Deno.exit(1);
  });
}
