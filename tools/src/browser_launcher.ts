// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { BIN_PATH_EXE, PATHS, PROJECT_ROOT } from "./defines.ts";
import { ProcessUtils } from "./utils.ts";

export const MARIONETTE_STATE_FILE = path.join(
  PROJECT_ROOT,
  "_dist",
  "marionette-port.txt",
);

/**
 * Minimal port of tools/lib/browser_launcher.rb
 */

function printFirefoxLog(line: string) {
  if (
    /MOZ_CRASH|JavaScript error:|console\.error|\] Errors|\[fluent\] Couldn't find a message:|\[fluent\] Missing|EGL Error:/
      .test(
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
  readonly command: readonly string[];
  readonly port: number;
  readonly profilePath: string;
}

export interface IsolatedBrowserLaunchView {
  readonly command: readonly string[];
  readonly port: number;
  readonly profilePath: string;
}

export interface IsolatedBrowserPairLaunchOptions {
  first: IsolatedBrowserLaunchOptions;
  second: IsolatedBrowserLaunchOptions;
}

export interface IsolatedBrowserPairLaunch {
  first: IsolatedBrowserLaunch;
  second: IsolatedBrowserLaunch;
}

export interface IsolatedBrowserChild {
  kill(signal?: Deno.Signal): void;
  pid: number;
  status: Promise<Deno.CommandStatus>;
}

export interface IsolatedBrowserSpawnOptions {
  clearEnv: boolean;
  env: Record<string, string>;
  stderr: "null";
  stdin: "null";
  stdout: "null";
}

export interface IsolatedBrowserSpawnDependencies {
  environment?: Record<string, string>;
  platform?: typeof Deno.build.os;
  processControl?: IsolatedBrowserProcessControl;
  shutdownTimeoutMilliseconds?: number;
  spawn?(
    command: readonly string[],
    options: IsolatedBrowserSpawnOptions,
  ): IsolatedBrowserChild;
}

export interface IsolatedBrowserProcessOwnership {
  platform: typeof Deno.build.os;
  rootPid: number;
}

/**
 * The controller owns process identity capture and must resolve `stop` or
 * `abort` only after it has verified the owned root/tree and Marionette port
 * are gone.
 */
export interface IsolatedBrowserProcessControl {
  abort(
    child: IsolatedBrowserChild,
    launch: IsolatedBrowserLaunchView,
    dependencies: IsolatedBrowserSpawnDependencies,
  ): Promise<void>;
  capture(
    child: IsolatedBrowserChild,
    launch: IsolatedBrowserLaunchView,
    platform: typeof Deno.build.os,
  ): Promise<IsolatedBrowserProcessOwnership>;
  stop(
    child: IsolatedBrowserChild,
    launch: IsolatedBrowserLaunchView,
    ownership: IsolatedBrowserProcessOwnership,
    dependencies: IsolatedBrowserSpawnDependencies,
  ): Promise<void>;
}

export interface RunningIsolatedBrowser {
  launch: IsolatedBrowserLaunchView;
  pid: number;
  stop(): Promise<void>;
}

export interface RunningIsolatedBrowserPair {
  first: RunningIsolatedBrowser;
  second: RunningIsolatedBrowser;
  stop(): Promise<void>;
}

const ISOLATED_BROWSER_ENV_ALLOWLIST = new Set([
  "APPDATA",
  "COMSPEC",
  "DISPLAY",
  "HOME",
  "LANG",
  "LC_ALL",
  "LC_CTYPE",
  "LOCALAPPDATA",
  "PATH",
  "SYSTEMROOT",
  "TEMP",
  "TMP",
  "TMPDIR",
  "USERPROFILE",
  "WAYLAND_DISPLAY",
  "WINDIR",
  "XAUTHORITY",
  "XDG_RUNTIME_DIR",
  "__CF_USER_TEXT_ENCODING",
]);

type IsolatedBrowserLaunchPhase = "prepared" | "running" | "cleaned";

interface IsolatedBrowserLaunchState {
  phase: IsolatedBrowserLaunchPhase;
  profilePath: string;
}

const isolatedBrowserLaunchStates = new WeakMap<
  IsolatedBrowserLaunch,
  IsolatedBrowserLaunchState
>();

function isolatedBrowserLaunchState(
  launch: IsolatedBrowserLaunch,
): IsolatedBrowserLaunchState {
  const state = isolatedBrowserLaunchStates.get(launch);
  if (state === undefined) {
    throw new Error("Isolated browser launch was not created by this launcher");
  }
  return state;
}

function beginIsolatedBrowserLaunch(launch: IsolatedBrowserLaunch): void {
  const state = isolatedBrowserLaunchState(launch);
  if (state.phase !== "prepared") {
    throw new Error(`Isolated browser launch is not prepared: ${state.phase}`);
  }
  state.phase = "running";
}

function cleanupIsolatedBrowserLaunch(
  launch: IsolatedBrowserLaunch,
  verifiedStopped = false,
): void {
  const state = isolatedBrowserLaunchState(launch);
  if (state.phase === "cleaned") {
    return;
  }
  if (state.phase === "running" && !verifiedStopped) {
    throw new Error(
      "Cannot clean an isolated browser profile while its process may be running",
    );
  }
  cleanupGeneratedProfile(state.profilePath);
  state.phase = "cleaned";
}

function isolatedBrowserLaunchView(
  launch: IsolatedBrowserLaunch,
): IsolatedBrowserLaunchView {
  return Object.freeze({
    command: Object.freeze([...launch.command]),
    port: launch.port,
    profilePath: launch.profilePath,
  });
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

function isolatedBrowserEnvironment(
  environment: Record<string, string>,
  platform: typeof Deno.build.os,
): Record<string, string> {
  const filtered: Record<string, string> = {};
  for (const [name, value] of Object.entries(environment)) {
    const allowedName = platform === "windows" ? name.toUpperCase() : name;
    if (ISOLATED_BROWSER_ENV_ALLOWLIST.has(allowedName)) {
      filtered[allowedName] = value;
    }
  }
  return filtered;
}

function defaultIsolatedBrowserSpawner(
  command: readonly string[],
  options: IsolatedBrowserSpawnOptions,
): IsolatedBrowserChild {
  const [executable, ...args] = command;
  if (executable === undefined) {
    throw new Error("Isolated browser command is empty");
  }
  return new Deno.Command(executable, {
    args,
    clearEnv: options.clearEnv,
    env: options.env,
    stderr: options.stderr,
    stdin: options.stdin,
    stdout: options.stdout,
  }).spawn();
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
  const state: IsolatedBrowserLaunchState = {
    phase: "prepared",
    profilePath,
  };
  const launch: IsolatedBrowserLaunch = {
    cleanup: () => cleanupIsolatedBrowserLaunch(launch),
    command: Object.freeze(
      browserCommand({
        binaryPath: options.binaryPath,
        marionette: true,
        noRemote: true,
        // The profile-local user.js is the single source of the port value.
        profilePath,
      }),
    ),
    port: options.port,
    profilePath,
  };
  isolatedBrowserLaunchStates.set(launch, state);
  return Object.freeze(launch);
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

export async function startIsolatedBrowser(
  launch: IsolatedBrowserLaunch,
  dependencies: IsolatedBrowserSpawnDependencies = {},
): Promise<RunningIsolatedBrowser> {
  const processControl = dependencies.processControl;
  if (processControl === undefined) {
    launch.cleanup();
    throw new Error(
      "Isolated browser launch requires an ownership controller before spawning",
    );
  }
  const launchView = isolatedBrowserLaunchView(launch);
  const environment = isolatedBrowserEnvironment(
    dependencies.environment ?? Deno.env.toObject(),
    dependencies.platform ?? Deno.build.os,
  );
  const spawnOptions: IsolatedBrowserSpawnOptions = {
    clearEnv: true,
    env: environment,
    stderr: "null",
    stdin: "null",
    stdout: "null",
  };
  const spawn = dependencies.spawn ?? defaultIsolatedBrowserSpawner;
  let child: IsolatedBrowserChild;
  beginIsolatedBrowserLaunch(launch);
  try {
    child = spawn(launch.command, spawnOptions);
  } catch (error) {
    cleanupIsolatedBrowserLaunch(launch, true);
    throw error;
  }
  const platform = dependencies.platform ?? Deno.build.os;
  let ownership: IsolatedBrowserProcessOwnership;
  try {
    ownership = await processControl.capture(child, launchView, platform);
  } catch (captureError) {
    try {
      await processControl.abort(child, launchView, dependencies);
      cleanupIsolatedBrowserLaunch(launch, true);
    } catch (abortError) {
      const captureMessage = captureError instanceof Error
        ? `: ${captureError.message}`
        : "";
      throw new AggregateError(
        [captureError, abortError],
        `Failed to capture isolated browser ownership and abort its process${captureMessage}`,
      );
    }
    throw captureError;
  }

  let stopPromise: Promise<void> | undefined;
  return {
    launch: launchView,
    pid: child.pid,
    stop: () => {
      if (stopPromise === undefined) {
        const attempt = (async () => {
          await processControl.stop(child, launchView, ownership, dependencies);
          cleanupIsolatedBrowserLaunch(launch, true);
        })();
        stopPromise = attempt;
        void attempt.catch(() => {
          if (stopPromise === attempt) {
            stopPromise = undefined;
          }
        });
      }
      return stopPromise;
    },
  };
}

export async function startIsolatedBrowserPair(
  options: IsolatedBrowserPairLaunchOptions,
  dependencies: IsolatedBrowserSpawnDependencies = {},
): Promise<RunningIsolatedBrowserPair> {
  const launches = createIsolatedBrowserPairLaunch(options);
  let first: RunningIsolatedBrowser | undefined;
  let secondAttempted = false;
  try {
    first = await startIsolatedBrowser(launches.first, dependencies);
    secondAttempted = true;
    const second = await startIsolatedBrowser(launches.second, dependencies);
    const runningFirst = first;
    let stopPromise: Promise<void> | undefined;
    return {
      first: runningFirst,
      second,
      stop: () => {
        if (stopPromise === undefined) {
          const attempt = (async () => {
            const results = await Promise.allSettled([
              runningFirst.stop(),
              second.stop(),
            ]);
            const failures = results.flatMap((result) =>
              result.status === "rejected" ? [result.reason] : []
            );
            if (failures.length > 0) {
              throw new AggregateError(
                failures,
                "Failed to stop isolated browser pair",
              );
            }
          })();
          stopPromise = attempt;
          void attempt.catch(() => {
            if (stopPromise === attempt) {
              stopPromise = undefined;
            }
          });
        }
        return stopPromise;
      },
    };
  } catch (error) {
    const cleanupFailures: unknown[] = [];
    if (first !== undefined) {
      const result = await Promise.allSettled([first.stop()]);
      if (result[0].status === "rejected") {
        cleanupFailures.push(result[0].reason);
      }
    }
    if (!secondAttempted) {
      try {
        launches.second.cleanup();
      } catch (cleanupError) {
        cleanupFailures.push(cleanupError);
      }
    }
    if (cleanupFailures.length > 0) {
      throw new AggregateError(
        [error, ...cleanupFailures],
        "Failed to start isolated browser pair cleanly",
      );
    }
    throw error;
  }
}

export async function run(
  portOrOptions: number | BrowserLaunchOptions = 5180,
): Promise<void> {
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
        console.log(
          `[launcher] Marionette port saved to ${MARIONETTE_STATE_FILE}`,
        );
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
