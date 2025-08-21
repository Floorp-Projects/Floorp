import * as path from "@std/path";
import { PROJECT_ROOT as DEFINES_PROJECT_ROOT } from "./defines.ts";

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
  } catch (e: any) {
    // keep parity with Ruby behavior (warn)
    // No logger available here by default.
    // Consumers can log as they prefer.
    console.warn(`Failed to remove ${p}: ${e?.message ?? e}`);
  }
}

export function createSymlink(link: string, target: string): void {
  try {
    safeRemove(link);
    // Deno.symlinkSync(target, path) => target then path (link)
    Deno.symlinkSync(target, link);
  } catch (e: any) {
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
    // eslint-disable-next-line no-console
    console.log(
      `${Logger.COLORS.info}${this.format("INFO", message)}${Logger.COLORS.reset}`,
      ...args,
    );
  }

  warn(message: string, ...args: unknown[]) {
    // eslint-disable-next-line no-console
    console.log(
      `${Logger.COLORS.warn}${this.format("WARN", message)}${Logger.COLORS.reset}`,
      ...args,
    );
  }

  error(message: string, ...args: unknown[]) {
    // eslint-disable-next-line no-console
    console.log(
      `${Logger.COLORS.error}${this.format("ERROR", message)}${Logger.COLORS.reset}`,
      ...args,
    );
  }

  success(message: string, ...args: unknown[]) {
    // eslint-disable-next-line no-console
    console.log(
      `${Logger.COLORS.success}${this.format("SUCCESS", message)}${Logger.COLORS.reset}`,
      ...args,
    );
  }

  debug(message: string, ...args: unknown[]) {
    if (Deno.env.get("DEBUG")) {
      console.log(
        `${Logger.COLORS.debug}${this.format("DEBUG", message)}${Logger.COLORS.reset}`,
        ...args,
      );
    }
  }
}

/* Process utilities: stream stdout/stderr to callback */
export const ProcessUtils = {
  async runCommandWithLogging(
    cmd: string[],
    cb: (stream: "stdout" | "stderr", line: string) => void,
  ): Promise<number> {
    const child = new Deno.Command(cmd[0], {
      args: cmd.slice(1),
      stdin: "null",
      stdout: "piped",
      stderr: "piped",
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
