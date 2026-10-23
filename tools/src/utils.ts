// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { spawn } from "node:child_process";
import { closeSync, openSync } from "node:fs";
import { constants as osConstants } from "node:os";
import { PROJECT_ROOT as DEFINES_PROJECT_ROOT } from "./defines.ts";
import type { FileOutputProcess } from "./process_output_types.ts";

export const PROJECT_ROOT = DEFINES_PROJECT_ROOT;

const DECODER = new TextDecoder();

/**
 * Resolve a path relative to the repository root
 */
export function resolveFromRoot(p: string): string {
  return path.resolve(PROJECT_ROOT, p);
}

export type Platform = "windows" | "darwin" | "linux";

export function platform(): Platform {
  switch (Deno.build.os) {
    case "windows":
      return "windows";
    case "darwin":
      return "darwin";
    default:
      return "linux";
  }
}

export function arch(): "aarch64" | "x86_64" {
  return Deno.build.arch === "aarch64" ? "aarch64" : "x86_64";
}

export function safeRemove(p: string): void {
  try {
    if (exists(p)) {
      Deno.removeSync(p, { recursive: true });
    }
    // deno-lint-ignore no-explicit-any
  } catch (e: any) {
    // keep parity with Ruby behavior (warn)
    // No logger available here by default.
    // Consumers can log as they prefer.
    console.warn(`Failed to remove ${p}: ${e?.message ?? e}`);
  }
}

/**
 * Create a directory junction. Unlike a symlink this needs no elevation on
 * Windows, which makes it a usable fallback when symlink creation is denied.
 * Only succeeds when `target` is a directory.
 */
function createDirectoryJunction(link: string, target: string): boolean {
  try {
    Deno.symlinkSync(target, link, { type: "junction" });
    return true;
  } catch {
    return false;
  }
}

export function createSymlink(link: string, target: string): void {
  try {
    safeRemove(link);
    // Deno.symlinkSync(target, path) => target then path (link)
    Deno.symlinkSync(target, link);
    // deno-lint-ignore no-explicit-any
  } catch (e: any) {
    // Windows refuses symlink creation unless Developer Mode is enabled or the
    // process is elevated (os error 1314). A directory junction needs neither
    // and is equivalent for the build's purposes, so fall back to one instead
    // of leaving the link missing and failing the build further downstream.
    if (Deno.build.os === "windows" && createDirectoryJunction(link, target)) {
      return;
    }
    console.warn(
      `Failed to create symlink ${link} -> ${target}: ${e?.message ?? e}`,
    );
  }
}

export function exists(p: string): boolean {
  try {
    Deno.lstatSync(p);
    return true;
  } catch {
    return false;
  }
}

export function gitInitialized(p: string): boolean {
  return exists(path.join(p, ".git"));
}

export interface CommandResult {
  stdout: string;
  stderr: string;
  success: boolean;
  code: number;
}

export function runCommandChecked(
  command: string,
  args: string[] = [],
  cwd?: string,
): CommandResult {
  const cmd = new Deno.Command(command, {
    args,
    cwd: cwd ?? undefined,
    stdout: "piped",
    stderr: "piped",
    env: Deno.env.toObject(),
  });
  const result = cmd.outputSync();
  return {
    stdout: DECODER.decode(result.stdout),
    stderr: DECODER.decode(result.stderr),
    success: (result.code ?? 0) === 0,
    code: result.code ?? 0,
  };
}

export function runCommand(
  command: string,
  args: string[] = [],
  cwd?: string,
): CommandResult {
  const result = runCommandChecked(command, args, cwd);
  if (!result.success) {
    throw new Error(
      `Command failed: ${command} ${args.join(" ")}\n${result.stderr}`,
    );
  }
  return result;
}

export function initializeDistDirectory(): void {
  const distDir = resolveFromRoot("_dist");
  const binDir = path.join(distDir, "bin");
  const profileDir = path.join(distDir, "profile");
  [distDir, binDir, profileDir].forEach((d) => {
    try {
      Deno.mkdirSync(d, { recursive: true });
    } catch {
      // ignore
    }
  });
}

/* Logger class similar to Ruby utils.Logger */
export class Logger {
  private prefix: string;

  private static readonly COLORS = {
    info: "\x1b[34m",
    warn: "\x1b[33m",
    error: "\x1b[31m",
    success: "\x1b[32m",
    debug: "\x1b[90m",
    reset: "\x1b[0m",
  } as const;

  constructor(prefix = "build") {
    this.prefix = prefix;
  }

  private format(level: string, message: string) {
    return `[${this.prefix}] ${level.toUpperCase()}: ${message}`;
  }

  info(message: string, ...args: unknown[]) {
    console.log(
      `${Logger.COLORS.info}${
        this.format("INFO", message)
      }${Logger.COLORS.reset}`,
      ...args,
    );
  }

  warn(message: string, ...args: unknown[]) {
    console.log(
      `${Logger.COLORS.warn}${
        this.format("WARN", message)
      }${Logger.COLORS.reset}`,
      ...args,
    );
  }

  error(message: string, ...args: unknown[]) {
    console.log(
      `${Logger.COLORS.error}${
        this.format("ERROR", message)
      }${Logger.COLORS.reset}`,
      ...args,
    );
  }

  success(message: string, ...args: unknown[]) {
    console.log(
      `${Logger.COLORS.success}${
        this.format("SUCCESS", message)
      }${Logger.COLORS.reset}`,
      ...args,
    );
  }

  debug(message: string, ...args: unknown[]) {
    if (Deno.env.get("DEBUG")) {
      console.log(
        `${Logger.COLORS.debug}${
          this.format("DEBUG", message)
        }${Logger.COLORS.reset}`,
        ...args,
      );
    }
  }
}

/* Process utilities: stream stdout/stderr to callback */
export const ProcessUtils = {
  spawnCommandWithFileOutput(
    cmd: string[],
    files: Record<"stdout" | "stderr", string>,
    cwd?: string,
  ): FileOutputProcess {
    const descriptors: number[] = [];
    try {
      const stdout = openSync(files.stdout, "wx", 0o600);
      descriptors.push(stdout);
      const stderr = openSync(files.stderr, "wx", 0o600);
      descriptors.push(stderr);
      const child = spawn(cmd[0], cmd.slice(1), {
        cwd,
        stdio: ["ignore", stdout, stderr],
        env: Deno.env.toObject(),
      });
      const status = new Promise<{ code: number }>((resolve, reject) => {
        child.once("error", reject);
        child.once("close", (code) => resolve({ code: code ?? 1 }));
      });
      // A caller may wait for a readiness signal before awaiting status. Keep
      // early spawn errors observed without changing what awaiting status does.
      void status.catch(() => {});
      const pid = child.pid;
      if (pid === undefined) {
        throw new Error(
          `Failed to spawn ${cmd[0]}: no process ID was assigned`,
        );
      }
      return {
        pid,
        status,
        kill(signal = "SIGTERM") {
          if (child.exitCode !== null || child.signalCode !== null) return;
          const signalNumber: unknown = Reflect.get(
            osConstants.signals,
            signal,
          );
          if (typeof signalNumber !== "number") {
            throw new TypeError(`Unsupported process signal: ${signal}`);
          }
          if (!child.kill(signalNumber)) {
            throw new Error(`Failed to stop owned child process ${pid}`);
          }
        },
      };
    } finally {
      // spawn duplicates these handles into the child. Closing only our own
      // descriptors leaves output independent of the parent's event loop.
      for (const descriptor of descriptors) closeSync(descriptor);
    }
  },

  async runCommandWithLogging(
    cmd: string[],
    cb: (stream: "stdout" | "stderr", line: string) => void,
  ): Promise<number> {
    const child = new Deno.Command(cmd[0], {
      args: cmd.slice(1),
      stdin: "null",
      stdout: "piped",
      stderr: "piped",
      env: Deno.env.toObject(), // Pass current environment variables to subprocess
    }).spawn();

    const readStream = async (
      stream: ReadableStream<Uint8Array> | null,
      name: "stdout" | "stderr",
    ) => {
      if (!stream) return;
      const reader = stream.getReader();
      let buffer = "";
      while (true) {
        const { value, done } = await reader.read();
        if (done) {
          if (buffer.length) cb(name, buffer + "\n");
          break;
        }
        buffer += DECODER.decode(value);
        let idx;
        while ((idx = buffer.indexOf("\n")) !== -1) {
          const line = buffer.slice(0, idx + 1);
          buffer = buffer.slice(idx + 1);
          cb(name, line);
        }
      }
    };

    await Promise.all([
      readStream(child.stdout, "stdout"),
      readStream(child.stderr, "stderr"),
    ]);

    const status = await child.status;
    return status.code ?? 0;
  },
} as const;
