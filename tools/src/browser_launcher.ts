// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { BIN_PATH_EXE, PATHS, PROJECT_ROOT } from "./defines.ts";
import { ProcessUtils } from "./utils.ts";

export const MARIONETTE_STATE_FILE = path.join(PROJECT_ROOT, "_dist", "marionette-port.txt");

/**
 * Minimal port of tools/lib/browser_launcher.rb
 */

function printFirefoxLog(line: string) {
  if (
    /MOZ_CRASH|JavaScript error:|console\.error|\] Errors|\[fluent\] Couldn't find a message:|\[fluent\] Missing|EGL Error:/.test(
      line,
    )
  ) {
    console.log(`\x1b[31m${line}\x1b[0m`);
  } else if (/console\.warn|WARNING:|\[WARN|JavaScript warning:/.test(line)) {
    console.log(`\x1b[33m${line}\x1b[0m`);
  } else if (/console\.debug/.test(line)) {
    console.log(`\x1b[36m${line}\x1b[0m`);
  } else {
    console.log(line);
  }
}

export interface BrowserLaunchOptions {
  binaryPath?: string;
  noRemote?: boolean;
  port?: number;
  marionette?: boolean;
  profilePath?: string;
}

export interface IsolatedBrowserLaunchOptions {
  binaryPath: string;
  port: number;
}

export interface IsolatedBrowserLaunch {
  cleanup(): void;
  command: string[];
  port: number;
  profilePath: string;
}

export interface IsolatedBrowserPairLaunchOptions {
  first: IsolatedBrowserLaunchOptions;
  second: IsolatedBrowserLaunchOptions;
}

export interface IsolatedBrowserPairLaunch {
  first: IsolatedBrowserLaunch;
  second: IsolatedBrowserLaunch;
}

function assertMarionettePort(port: number): void {
  if (!Number.isInteger(port) || port < 1_024 || port > 65_535) {
    throw new Error(
      `Marionette port must be an integer between 1024 and 65535: ${port}`,
    );
  }
}

function assertIsolatedLaunchOptions(
  options: IsolatedBrowserLaunchOptions,
): void {
  if (options.binaryPath.trim().length === 0) {
    throw new Error("Browser binary path must not be empty");
  }
  assertMarionettePort(options.port);
}

function assertPrivateGeneratedPath(
  filePath: string,
  expectedType: "directory" | "file",
): void {
  const info = Deno.lstatSync(filePath);
  const actualType = info.isDirectory
    ? "directory"
    : info.isFile
    ? "file"
    : "other";
  if (info.isSymlink || actualType !== expectedType) {
    throw new Error(
      `Generated Marionette ${expectedType} has an unsafe type: ${filePath}`,
    );
  }
  if (
    Deno.build.os !== "windows" &&
    info.mode !== null &&
    (info.mode & 0o077) !== 0
  ) {
    throw new Error(
      `Generated Marionette ${expectedType} is not private: ${filePath}`,
    );
  }
}

function cleanupGeneratedProfile(profilePath: string): void {
  try {
    Deno.removeSync(profilePath, { recursive: true });
  } catch (error) {
    if (!(error instanceof Deno.errors.NotFound)) {
      throw error;
    }
  }
}

/**
 * Creates a new random, private profile-local Marionette configuration. The
 * profile path is never supplied by a caller, preventing accidental inheritance
 * of credentials, cookies, or remote-agent state from a shared profile.
 */
function prepareMarionetteProfile(port: number): string {
  assertMarionettePort(port);
  const profilePath = Deno.makeTempDirSync({
    prefix: "floorp-notes-sync-marionette-",
  });
  try {
    assertPrivateGeneratedPath(profilePath, "directory");
    const userJsPath = path.join(profilePath, "user.js");
    Deno.writeTextFileSync(
      userJsPath,
      'user_pref("remote.active-protocols", 0);\n' +
        'user_pref("marionette.enabled", true);\n' +
        `user_pref("marionette.port", ${port});\n`,
      { createNew: true, mode: 0o600 },
    );
    assertPrivateGeneratedPath(userJsPath, "file");
    return profilePath;
  } catch (error) {
    try {
      cleanupGeneratedProfile(profilePath);
    } catch {
      // Preserve the original setup failure; no caller-supplied path was used.
    }
    throw error;
  }
}

export function browserCommand(options: BrowserLaunchOptions = {}): string[] {
  const {
    binaryPath = BIN_PATH_EXE,
    marionette = true,
    noRemote = false,
    profilePath = PATHS.profile_test,
  } = options;
  const args = [
    binaryPath,
    "--profile",
    profilePath,
  ];
  if (marionette) {
    args.push("--marionette", "--remote-allow-system-access");
  }
  if (noRemote) {
    args.push("--no-remote");
  }
  return args;
}

export function createIsolatedBrowserLaunch(
  options: IsolatedBrowserLaunchOptions,
): IsolatedBrowserLaunch {
  assertIsolatedLaunchOptions(options);
  const profilePath = prepareMarionetteProfile(options.port);

  return {
    cleanup: () => cleanupGeneratedProfile(profilePath),
    command: browserCommand({
      binaryPath: options.binaryPath,
      marionette: true,
      noRemote: true,
      // The profile-local user.js is the single source of the port value.
      profilePath,
    }),
    port: options.port,
    profilePath,
  };
}

/**
 * Prepares two disposable browser configurations without launching either
 * browser or writing the legacy global Marionette state file. Callers must use
 * each returned port explicitly when connecting their corresponding client.
 */
export function createIsolatedBrowserPairLaunch(
  options: IsolatedBrowserPairLaunchOptions,
): IsolatedBrowserPairLaunch {
  assertIsolatedLaunchOptions(options.first);
  assertIsolatedLaunchOptions(options.second);
  if (options.first.port === options.second.port) {
    throw new Error("Isolated browser pair must use distinct ports");
  }
  const first = createIsolatedBrowserLaunch(options.first);
  try {
    const second = createIsolatedBrowserLaunch(options.second);
    if (first.profilePath === second.profilePath) {
      second.cleanup();
      throw new Error("Isolated browser pair generated a shared profile");
    }
    return { first, second };
  } catch (error) {
    first.cleanup();
    throw error;
  }
}

export async function run(portOrOptions: number | BrowserLaunchOptions = 5180): Promise<void> {
  const options: BrowserLaunchOptions = typeof portOrOptions === "number"
    ? { port: portOrOptions, marionette: true }
    : portOrOptions;
  const port = options.port ?? 5180;
  const marionette = options.marionette ?? true;
  const cmd = browserCommand({ ...options, port, marionette });

  console.log("[launcher] Launching browser with command: " + cmd.join(" "));

  await ProcessUtils.runCommandWithLogging(
    cmd,
    (_stream: "stdout" | "stderr", line: string) => {
      const m = line.match(/Marionette\tINFO\tListening on port (\d+)/);
      if (m) {
        console.log("nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-webdriver");
        Deno.writeTextFileSync(MARIONETTE_STATE_FILE, m[1]);
        console.log(`[launcher] Marionette port saved to ${MARIONETTE_STATE_FILE}`);
      }
      printFirefoxLog(line.trim());
    },
  );

  try {
    Deno.removeSync(MARIONETTE_STATE_FILE);
  } catch {
    // ignore if already removed
  }
  console.log("[launcher] Browser Closed");
}
