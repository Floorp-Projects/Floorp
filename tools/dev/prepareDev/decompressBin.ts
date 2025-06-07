import {
  binDir,
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
 * Executes a shell command using Deno's subprocess API
 */
async function runCommand(command: string, args: string[]): Promise<void> {
  const cmd = new Deno.Command(command, {
    args,
    stdout: "piped",
    stderr: "piped",
  });

  const { code, stderr } = await cmd.output();

  if (code !== 0) {
    const errorText = new TextDecoder().decode(stderr);
    throw new Error(
      `Command failed: ${command} ${args.join(" ")}\n${errorText}`,
    );
  }
}

/**
 * Executes a PowerShell command on Windows
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
        // Windows - Use PowerShell's Expand-Archive with explicit module import
        await runPowerShellCommand(
          `Expand-Archive -LiteralPath "${binArchive}" -DestinationPath "${binRootDir}"`,
        );
        break;

      case "darwin":
        // macOS
        await decompressBinMacOS();
        break;

      case "linux":
        // Linux
        await runCommand("tar", ["-xf", binArchive, "-C", binRootDir]);
        await runCommand("chmod", ["-R", "755", `./${binDir}`]);
        await runCommand("chmod", ["755", binPathExe]);
        break;
    }

    await Deno.writeTextFile(binVersion, VERSION);

    console.log("[dev] decompress complete!");
  } catch (error: unknown) {
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
        runCommand("chmod", ["-R", "777", appDestBase]),
        runCommand("xattr", ["-rc", appDestBase]),
      ]);
    } finally {
      await runCommand("hdiutil", ["detach", mountDir]);
      await Deno.remove(mountDir, { recursive: true });
    }
  } catch (error) {
    console.error("[dev] Error during macOS decompression:", error);
    Deno.exit(1);
  }
}

// Direct execution support
if (import.meta.main) {
  try {
    await decompressBin();
  } catch (error) {
    console.error("[dev] Failed to decompress binary:", error);
    Deno.exit(1);
  }
}
