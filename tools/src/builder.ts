// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { PROJECT_ROOT, PATHS } from "./defines.ts";
import {
  createSymlink,
  exists,
  Logger,
  runCommandChecked,
  safeRemove,
} from "./utils.ts";
import { writeBuildid2 } from "./update.ts";

const logger = new Logger("builder");

/**
 * Get the full path to the deno executable.
 * This ensures we can find deno even if PATH resolution is problematic.
 */
function getDenoPath(): string {
  // First, check if DENO_EXECUTABLE_PATH is set (by CI/CD)
  const envPath = Deno.env.get("DENO_EXECUTABLE_PATH");
  if (envPath) {
    logger.debug(`Using Deno executable from DENO_EXECUTABLE_PATH: ${envPath}`);
    return envPath;
  }

  // Otherwise, use Deno.execPath() which gives us the current deno executable
  const denoPath = Deno.execPath();
  logger.debug(`Using Deno executable from Deno.execPath(): ${denoPath}`);
  return denoPath;
}

export function packageVersion(): string {
  const pkgPath = path.join(PROJECT_ROOT, "package.json");
  const content = Deno.readTextFileSync(pkgPath);
  return JSON.parse(content).version;
}

type CommandTuple = readonly [readonly string[], string];

export async function runInParallel(commands: CommandTuple[]): Promise<void> {
  const results = commands.map(([cmd, dir]) => {
    logger.info(`Running \`${cmd.join(" ")}\` in \`${dir}\``);
    // Use runCommandChecked (sync) to capture stdout/stderr concisely in Deno
    const res = runCommandChecked(cmd[0], cmd.slice(1), dir);
    return {
      success: res.code === 0,
      cmd,
      dir,
      out: res.stdout,
      err: res.stderr,
    };
  });

  for (const res of results) {
    if (!res.success) {
      throw new Error(
        `Build command \`${res.cmd.join(" ")}\` in \`${res.dir}\` failed\nSTDOUT:\n${res.out}\nSTDERR:\n${res.err}`,
      );
    }
  }
}

export async function run(mode = "dev", buildid2: string): Promise<void> {
  logger.info(`Building features with mode=${mode}`);

  // Ensure buildid2 is written to the expected path so other tools can read it.
  try {
    writeBuildid2(PATHS.buildid2, buildid2);
  } catch (e: any) {
    logger.error(`Failed to write buildid2: ${e?.message ?? e}`);
  }

  const version = packageVersion();
  const deno = getDenoPath(); // Get the full path to deno executable

  const devCommands: CommandTuple[] = [
    [
      [deno, "task", "build", `--env.MODE=${mode}`],
      path.join(PROJECT_ROOT, "bridge/startup"),
    ],
    [
      [
        deno,
        "task",
        "build",
        `--env.__BUILDID2__=${buildid2}`,
        `--env.__VERSION2__=${version}`,
      ],
      path.join(PROJECT_ROOT, "bridge/loader-modules"),
    ],
    [
      [
        deno,
        "run",
        "-A",
        "vite",
        "build",
        "--base",
        "chrome://noraneko/content",
      ],
      path.join(PROJECT_ROOT, "bridge/loader-features"),
    ],
  ];

  const prodCommands: CommandTuple[] = [
    [
      [deno, "task", "build", "--env.MODE=production"],
      path.join(PROJECT_ROOT, "bridge/startup"),
    ],
    [
      [
        deno,
        "run",
        "-A",
        "vite",
        "build",
        "--base",
        "chrome://noraneko/content",
      ],
      path.join(PROJECT_ROOT, "bridge/loader-features"),
    ],

    // Converted app builds
    [
      [
        deno,
        "run",
        "-A",
        "vite",
        "build",
        "--config",
        "vite.config.ts",
        "--base",
        "chrome://noraneko-newtab/content",
      ],
      path.join(PROJECT_ROOT, "browser-features/pages-newtab"),
    ],
    [
      [
        deno,
        "run",
        "-A",
        "vite",
        "build",
        "--config",
        "vite.config.ts",
        "--base",
        "chrome://noraneko-settings/content",
      ],
      path.join(PROJECT_ROOT, "browser-features/pages-settings"),
    ],
    [
      [
        deno,
        "run",
        "-A",
        "vite",
        "build",
        "--config",
        "vite.config.ts",
        "--base",
        "chrome://noraneko-welcome/content",
      ],
      path.join(PROJECT_ROOT, "browser-features/pages-welcome"),
    ],
    [
      [
        deno,
        "run",
        "-A",
        "vite",
        "build",
        "--config",
        "vite.config.ts",
        "--base",
        "chrome://noraneko-notes/content",
      ],
      path.join(PROJECT_ROOT, "browser-features/pages-notes"),
    ],
    [
      [
        deno,
        "run",
        "-A",
        "vite",
        "build",
        "--config",
        "vite.config.ts",
        "--base",
        "chrome://noraneko-modal-child/content",
      ],
      path.join(PROJECT_ROOT, "browser-features/pages-modal-child"),
    ],

    [
      [
        deno,
        "task",
        "build",
        `--env.__BUILDID2__=${buildid2}`,
        `--env.__VERSION2__=${version}`,
      ],
      path.join(PROJECT_ROOT, "bridge/loader-modules"),
    ],
    // [
    //   [
    //     deno,
    //     "run",
    //     "-A",
    //     "vite",
    //     "build",
    //     "--base",
    //     "chrome://noraneko-settings/content",
    //   ],
    //   path.join(PROJECT_ROOT, "src/ui/settings"),
    // ],
  ];

  if (mode.startsWith("dev")) {
    await runInParallel(devCommands);
  } else {
    await runInParallel(prodCommands);
  }

  if (mode.startsWith("production")) {
    const mounts: Array<[string, string]> = [
      ["content", "bridge/loader-features/_dist"],
      ["startup", "bridge/startup/_dist"],
      ["skin", "browser-features/skin"],
      ["resource", "bridge/loader-modules/_dist"],
      ["pages-newtab", "browser-features/pages-newtab/dist"],
      ["pages-settings", "browser-features/pages-settings/dist"],
      ["pages-welcome", "browser-features/pages-welcome/dist"],
      ["pages-notes", "browser-features/pages-notes/dist"],
      ["pages-modal-child", "browser-features/pages-modal-child/dist"],
    ];

    const dirPath = "_dist/noraneko";
    try {
      if (exists(dirPath)) {
        safeRemove(dirPath);
      }
    } catch {}
    Deno.mkdirSync(dirPath);

    for (const [subdir, target] of mounts) {
      const linkPath = path.resolve(dirPath, subdir);
      const targetPath = path.resolve(target);
      try {
        if (exists(linkPath)) {
          safeRemove(linkPath);
        }
      } catch {
        // ignore
      }

      try {
        createSymlink(linkPath, targetPath);
      } catch (e: any) {
        logger.warn(
          `Failed to create symlink ${linkPath} -> ${targetPath}: ${e?.message ?? e}`,
        );
      }
    }
  }

  logger.success("Build complete.");
}
