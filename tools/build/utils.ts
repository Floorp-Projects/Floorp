import { resolve } from "@std/path";
import { exists } from "@std/fs/exists";
import { ensureDir } from "@std/fs/ensure-dir";
import { remove } from "@std/fs/unstable-remove";
import type { Platform } from "./types.ts";

/**
 * Project root directory
 */
export const projectRoot = resolve(import.meta.dirname!, "../..");

/**
 * Resolve path from project root
 */
export const resolveFromRoot = (path: string) => resolve(projectRoot, path);

/**
 * Current platform information
 */
export const platform = Deno.build.os as Platform;
export const arch = Deno.build.arch;

/**
 * Safe file operations
 */
export async function safeRemove(path: string): Promise<void> {
  try {
    if (await exists(path)) {
      await remove(path, { recursive: true });
    }
  } catch (error) {
    console.warn(`Failed to remove ${path}:`, error);
  }
}

/**
 * Create a symlink (cross-platform)
 */
export async function createSymlink(
  link: string,
  target: string,
): Promise<void> {
  try {
    if (await exists(link)) {
      await safeRemove(link);
    }

    if (platform === "windows") {
      // Windows: Use junction for directories, symlink for files
      const isDir = (await Deno.stat(target).catch(() => null))?.isDirectory;
      const type = isDir ? "junction" : "file";
      await Deno.symlink(target, link, { type });
    } else {
      // Unix-like: Standard symlink
      await Deno.symlink(target, link);
    }
  } catch (error) {
    console.warn(`Failed to create symlink ${link} -> ${target}:`, error);
  }
}

/**
 * Sync version of exists check
 */
export function isExists(path: string): Promise<boolean> {
  return exists(path);
}

export function isExistsSync(
  path: string,
  options?: { isDirectory?: boolean },
): boolean {
  try {
    const stat = Deno.statSync(path);
    if (options?.isDirectory !== undefined) {
      return options.isDirectory ? stat.isDirectory : !stat.isDirectory;
    }
    return true;
  } catch {
    return false;
  }
}

/**
 * Check if a directory is a git repository
 */
export async function isGitInitialized(path: string): Promise<boolean> {
  const gitDir = resolve(path, ".git");
  return await exists(gitDir);
}

/**
 * Run a command and return the result
 */
export async function runCommandChecked(
  command: string,
  args: string[],
  options: { cwd?: string } = {},
): Promise<{ stdout: string; stderr: string; success: boolean }> {
  try {
    const cmd = new Deno.Command(command, {
      args,
      cwd: options.cwd,
      stdout: "piped",
      stderr: "piped",
    });

    const result = await cmd.output();
    const stdout = new TextDecoder().decode(result.stdout);
    const stderr = new TextDecoder().decode(result.stderr);

    return {
      stdout,
      stderr,
      success: result.success,
    };
  } catch (error) {
    return {
      stdout: "",
      stderr: String(error),
      success: false,
    };
  }
}

/**
 * Simple command runner (legacy compatibility)
 */
export async function runCommand(
  command: string,
  args: string[] = [],
  options: { cwd?: string } = {},
): Promise<void> {
  const result = await runCommandChecked(command, args, options);
  if (!result.success) {
    throw new Error(
      `Command failed: ${command} ${args.join(" ")}\n${result.stderr}`,
    );
  }
}

/**
 * Initialize _dist directory structure
 */
export async function initializeDistDirectory(): Promise<void> {
  const distDir = resolveFromRoot("_dist");
  const binDir = resolve(distDir, "bin");
  const profileDir = resolve(distDir, "profile");

  await ensureDir(distDir);
  await ensureDir(binDir);
  await ensureDir(profileDir);
}

/**
 * Run child build process
 */
export async function runChildBuild(isDev: boolean): Promise<void> {
  const { readBuildid2 } = await import("./tasks/update/buildid2.ts");
  const buildid2 =
    (await readBuildid2(resolveFromRoot("_dist"))) || "default-build-id";
  const mode = isDev ? "dev" : "prod";
  const childBuildPath = resolveFromRoot("tools/dev/launchDev/child-build.ts");

  const command = new Deno.Command(Deno.execPath(), {
    args: ["run", "-A", childBuildPath, mode, buildid2],
    cwd: projectRoot,
    stdout: "piped",
    stderr: "piped",
  });

  const process = command.spawn();
  const { code, stdout, stderr } = await process.output();

  if (code !== 0) {
    const errorOutput = new TextDecoder().decode(stderr);
    throw new Error(`Child build failed: ${errorOutput}`);
  }

  const output = new TextDecoder().decode(stdout);
  if (output.trim()) {
    console.log(`Build output: ${output}`);
  }
}

/**
 * Run child dev process
 */
export async function runChildDev(): Promise<void> {
  const { readBuildid2 } = await import("./tasks/update/buildid2.ts");
  const buildid2 =
    (await readBuildid2(resolveFromRoot("_dist"))) || "default-build-id";
  const childDevPath = resolveFromRoot("tools/dev/launchDev/child-dev.ts");

  const command = new Deno.Command(Deno.execPath(), {
    args: ["run", "-A", childDevPath, "dev", buildid2],
    cwd: projectRoot,
    stdin: "piped",
    stdout: "piped",
    stderr: "piped",
  });

  const _process = command.spawn();
  console.log("Development server started in background");
}

/**
 * Re-export commonly used functions directly
 */
export { ensureDir, exists };
