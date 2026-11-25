// SPDX-License-Identifier: MPL-2.0

import { isAbsolute, join, resolve } from "@std/path";
import { BIN_PATH_EXE, PATHS, PLATFORM, PROJECT_ROOT } from "./defines.ts";
import {
  DEV_PROCESSES_FILE,
  listDevProcesses,
  terminateDevProcesses,
} from "./dev_processes.ts";
import {
  matchesDevIdentity,
  parseDevProcessRegistry,
  selectDevProcesses,
} from "./dev_process_selection.ts";
import type {
  DevProcessIdentity,
  DevProcessInfo,
} from "./dev_process_types.ts";
import { MarionetteClient } from "./browser_connector.ts";
import type { MarionetteStage } from "./browser_connector.types.ts";
import { DEV_STATE_TOKEN_ENV } from "./dev_state_lock.ts";
import { superviseM3dOwnedCommandOperations } from "./m3d_package_acquisition_command.ts";
import { startedNoEarlier } from "./fdh_job_process.ts";
import type {
  HubPanelMenuM3eBoundedReadOperation,
  HubPanelMenuM3eOwnedCommandOperations,
  HubPanelMenuM3eOwnedProcessRefresh,
} from "./hub_panel_menu_m3e_process.types.ts";
import type {
  HubPanelMenuM3eFailure,
  HubPanelMenuM3eOwnedCommandResult,
} from "./hub_panel_menu_m3e_acceptance.types.ts";

const OUTPUT_LIMIT = 256 * 1024;
const COMMAND_TIMEOUT_MS = 120_000;
const CLEANUP_TIMEOUT_MS = 10_000;

export function hubPanelMenuM3eLaunchEnvironment(
  environment: Record<string, string>,
): Record<string, string> {
  const names = [
    "PATH",
    "HOME",
    "USER",
    "LANG",
    "LC_ALL",
    "DISPLAY",
    "XAUTHORITY",
    "XDG_RUNTIME_DIR",
    "DBUS_SESSION_BUS_ADDRESS",
    "DENO_DIR",
    "DENO_INSTALL",
    DEV_STATE_TOKEN_ENV,
  ];
  return Object.fromEntries(
    names.filter((name) => environment[name] !== undefined).map((
      name,
    ) => [name, environment[name]]),
  );
}

export function hubPanelMenuM3eFailure(
  category: HubPanelMenuM3eFailure["category"],
  error: unknown = null,
  rowId?: string,
): HubPanelMenuM3eFailure {
  return {
    category,
    kind: error instanceof Deno.errors.NotFound
      ? "NotFound"
      : error instanceof Deno.errors.PermissionDenied
      ? "PermissionDenied"
      : error instanceof Error
      ? "Error"
      : "Other",
    ...(["A", "B", "C"].includes(rowId ?? "") ? { rowId } : {}),
  };
}

function safeFailure(value: unknown): HubPanelMenuM3eFailure {
  const input = typeof value === "object" && value !== null
    ? value as Record<string, unknown>
    : {};
  const category =
    ["launch", "run", "snapshot", "report", "cleanup", "incomplete"].includes(
        String(input.category),
      )
      ? input.category
      : "run";
  const kind = ["Error", "NotFound", "PermissionDenied", "Other"].includes(
      String(input.kind),
    )
    ? input.kind
    : "Other";
  return {
    category,
    kind,
    ...(["A", "B", "C"].includes(String(input.rowId))
      ? { rowId: input.rowId }
      : {}),
  } as HubPanelMenuM3eFailure;
}

export function serializeHubPanelMenuM3eEvidence(value: unknown): string {
  return JSON.stringify(value, (key, item: unknown) => {
    if ((key === "failure" || key === "runFailure") && item !== null) {
      return safeFailure(item);
    }
    if (key === "launchFailures" && Array.isArray(item)) {
      return item.map(safeFailure);
    }
    if (["commandLine", "sessionId"].includes(key)) return "omitted";
    if (typeof item === "string" && /^[\[{]/.test(item)) {
      try {
        const parsed: unknown = JSON.parse(item);
        return serializeHubPanelMenuM3eEvidence(parsed);
      } catch { /* Not encoded JSON evidence. */ }
    }
    return item;
  }, 2);
}

export async function hubPanelMenuM3eLaunchPids(
  processes: readonly DevProcessInfo[],
  token: string,
  readText: (path: string) => Promise<string> = Deno.readTextFile,
): Promise<number[]> {
  if (!token) throw new Error("M3e launch token is missing");
  const owned: number[] = [];
  for (const process of processes) {
    try {
      const environment = (await readText(`/proc/${process.pid}/environ`))
        .split("\0");
      const stat = await readText(`/proc/${process.pid}/stat`);
      const ticks = stat.slice(stat.lastIndexOf(")") + 2).split(" ")[19];
      if (
        environment.includes(`FLOORP_M3E_LAUNCH_TOKEN=${token}`) &&
        process.startTime.endsWith(`:${ticks}`)
      ) owned.push(process.pid);
    } catch { /* Exited and unrelated processes are not ownership evidence. */ }
  }
  return owned;
}

export async function withHubPanelMenuM3eCleanup<T>(
  diagnose: () => Promise<T>,
  terminate: () => Promise<void>,
): Promise<T> {
  try {
    return await diagnose();
  } finally {
    await terminate();
  }
}

export async function cleanupHubPanelMenuM3eLaunches<T>(
  launches: readonly T[],
  stop: (launch: T) => Promise<boolean>,
): Promise<boolean> {
  let proven = true;
  for (const launch of launches) {
    try {
      if (!await stop(launch)) proven = false;
    } catch {
      proven = false;
    }
  }
  return proven;
}

export function readHubPanelMenuM3eBoundedOutput(
  stream: ReadableStream<Uint8Array>,
  limit: number,
): HubPanelMenuM3eBoundedReadOperation {
  const reader = stream.getReader();
  let released = false;
  const chunks: Uint8Array[] = [];
  let length = 0;
  const snapshot = (): Uint8Array => {
    const output = new Uint8Array(length);
    let offset = 0;
    for (const chunk of chunks) {
      output.set(chunk, offset);
      offset += chunk.byteLength;
    }
    return output;
  };
  const promise = (async () => {
    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        const remaining = limit - length;
        if (value.byteLength > remaining) {
          if (remaining > 0) {
            chunks.push(value.slice(0, remaining));
            length += remaining;
          }
          throw new Error("M3e command output limit exceeded");
        }
        chunks.push(value.slice());
        length += value.byteLength;
      }
      return snapshot();
    } finally {
      reader.releaseLock();
      released = true;
    }
  })();
  return {
    promise,
    snapshot,
    cancel: async () => {
      if (released) await stream.cancel();
      else await reader.cancel();
    },
  };
}

export async function superviseHubPanelMenuM3eOwnedCommandOperations(
  operations: HubPanelMenuM3eOwnedCommandOperations,
  operationTimeoutMs = COMMAND_TIMEOUT_MS,
  cleanupTimeoutMs = CLEANUP_TIMEOUT_MS,
): Promise<HubPanelMenuM3eOwnedCommandResult> {
  const result = await superviseM3dOwnedCommandOperations(
    operations,
    operationTimeoutMs,
    cleanupTimeoutMs,
  );
  return {
    completed: result.completed,
    success: result.completed && result.success === true,
    stdout: new TextDecoder().decode(operations.stdout.snapshot()),
    stderr: new TextDecoder().decode(operations.stderr.snapshot()),
    cleanupProven: result.cleanupProven,
  };
}

export async function runHubPanelMenuM3eOwnedCommand(
  command: string,
  args: readonly string[],
  profilePath: string,
  stdoutLogPath: string,
  stderrLogPath: string,
  launchToken?: string,
): Promise<HubPanelMenuM3eOwnedCommandResult> {
  if (Deno.build.os !== "linux") {
    throw new Error("M3e native acceptance requires Linux");
  }
  if (!isAbsolute(profilePath)) {
    throw new Error("M3e profile path must be absolute");
  }
  const child = new Deno.Command(command, {
    args: [...args],
    cwd: PROJECT_ROOT,
    clearEnv: true,
    env: {
      ...hubPanelMenuM3eLaunchEnvironment(Deno.env.toObject()),
      ...(launchToken ? { FLOORP_M3E_LAUNCH_TOKEN: launchToken } : {}),
      FLOORP_TEST_PROFILE_DIR: profilePath,
      FLOORP_DEV_RUNTIME_DISTRIBUTION: "release",
      FLOORP_SKIP_PATCHER: "1",
      ...(args.length === 3 && args[0] === "task" && args[1] === "dev-tool" &&
          args[2] === "status"
        ? { FLOORP_M3E_STATUS_DIAGNOSTICS: "1" }
        : {}),
    },
    stdin: "null",
    stdout: "piped",
    stderr: "piped",
  }).spawn();
  const stdoutOperation = readHubPanelMenuM3eBoundedOutput(
    child.stdout,
    OUTPUT_LIMIT,
  );
  const stderrOperation = readHubPanelMenuM3eBoundedOutput(
    child.stderr,
    OUTPUT_LIMIT,
  );
  let exitCode: number | null = null;
  const operations: HubPanelMenuM3eOwnedCommandOperations = {
    stdout: stdoutOperation,
    stderr: stderrOperation,
    status: child.status.then((status) => {
      exitCode = status.code;
      return status;
    }),
    terminate: () => child.kill("SIGKILL"),
  };
  const result = await superviseHubPanelMenuM3eOwnedCommandOperations(
    operations,
  );
  await writeHubPanelMenuM3eCommandArtifacts(
    result,
    stdoutLogPath,
    stderrLogPath,
    exitCode,
  );
  return { ...result, exitCode };
}

export function hubPanelMenuM3eStatusStages(stderr: string): MarionetteStage[] {
  const stages: MarionetteStage[] = [];
  for (const line of stderr.slice(-OUTPUT_LIMIT).split("\n")) {
    const match = /^\[m3e-status\] (connect|handshake|command|timeout)\r?$/
      .exec(line);
    if (match) stages.push(match[1] as MarionetteStage);
  }
  return stages.slice(-32);
}

export async function writeHubPanelMenuM3eCommandArtifacts(
  result: HubPanelMenuM3eOwnedCommandResult,
  stdoutLogPath: string,
  stderrLogPath: string,
  exitCode: number | null,
): Promise<void> {
  await Deno.writeTextFile(
    stdoutLogPath,
    JSON.stringify({
      connected: result.stdout.includes("Connected to browser."),
      omitted: "raw output",
    }),
    { mode: 0o600 },
  );
  await Deno.writeTextFile(
    stderrLogPath,
    JSON.stringify({
      omitted: "raw output",
      stages: hubPanelMenuM3eStatusStages(result.stderr),
    }),
    { mode: 0o600 },
  );
  await Deno.writeTextFile(
    `${stdoutLogPath}.status.json`,
    JSON.stringify({
      completed: result.completed,
      success: result.success,
      exitCode,
      cleanupProven: result.cleanupProven,
    }),
    { mode: 0o600 },
  );
}

export async function seedHubPanelMenuM3eProfile(
  profilePath: string,
  featureEnabled: boolean,
  manualHostEnabled: boolean,
): Promise<void> {
  if (!isAbsolute(profilePath)) {
    throw new Error("M3e profile path must be absolute");
  }
  await Deno.mkdir(profilePath, { mode: 0o700 });
  const prefs = [
    `user_pref("nora.dev.hub_menu_feature", ${featureEnabled});`,
    `user_pref("nora.dev.hub_menu_manual_host", ${manualHostEnabled});`,
  ].join("\n") + "\n";
  await Deno.writeTextFile(join(profilePath, "prefs.js"), prefs, {
    createNew: true,
    mode: 0o600,
  });
}

export async function assertHubPanelMenuM3eUserJsDoesNotOverride(
  profilePath: string,
): Promise<void> {
  let userJs: string;
  try {
    userJs = await Deno.readTextFile(join(profilePath, "user.js"));
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return;
    throw error;
  }
  if (
    /user_pref\s*\(\s*["']nora\.dev\.hub_menu_(?:feature|manual_host)["']/.test(
      userJs,
    )
  ) {
    throw new Error("generated user.js overrides an M3e seeded preference");
  }
}

export function assertHubPanelMenuM3eProcessProfile(
  processes: readonly DevProcessInfo[],
  profilePath: string,
): DevProcessInfo {
  const matches = processes.filter((process) => {
    const argv = process.argv;
    if (!argv) return false;
    return argv.some((argument, index) =>
      argument === "--profile" && argv[index + 1] === profilePath
    );
  });
  if (matches.length !== 1) {
    throw new Error(
      "M3e browser process does not have one exact --profile argument",
    );
  }
  const process = matches[0];
  const executable = resolve(BIN_PATH_EXE);
  if (!process.argv || resolve(process.argv[0]) !== executable) {
    throw new Error(
      "M3e browser did not use the locked release runtime binary",
    );
  }
  return process;
}

export async function connectHubPanelMenuM3eBrowser(): Promise<
  MarionetteClient
> {
  const portPath = join(PROJECT_ROOT, "_dist", "marionette-port.txt");
  const raw = (await Deno.readTextFile(portPath)).trim();
  if (!/^\d+$/.test(raw)) throw new Error("M3e Marionette port is invalid");
  const port = Number(raw);
  if (port < 1 || port > 65_535) {
    throw new Error("M3e Marionette port is invalid");
  }
  return await MarionetteClient.connect(port);
}

export async function ownedM3eProcesses(): Promise<DevProcessInfo[]> {
  const processes = await listDevProcesses();
  if (PLATFORM !== "linux") return processes;
  return await refreshHubPanelMenuM3eProcessSnapshot(processes, async (pid) => {
    try {
      return await Deno.readTextFile(`/proc/${pid}/stat`);
    } catch (error) {
      if (error instanceof Deno.errors.NotFound) return null;
      throw error;
    }
  });
}

export async function refreshHubPanelMenuM3eProcessSnapshot(
  processes: readonly DevProcessInfo[],
  readStat: (pid: number) => Promise<string | null>,
): Promise<DevProcessInfo[]> {
  const refreshed = await Promise.all(processes.map(async (process) => {
    const stat = await readStat(process.pid);
    if (stat === null) return null;
    const state = stat.slice(stat.lastIndexOf(")") + 2).split(" ")[0];
    return state === "Z" || state === "X" ? null : process;
  }));
  return refreshed.filter((process): process is DevProcessInfo =>
    process !== null
  );
}

export function preexistingHubPanelMenuM3eProcesses(
  processes: readonly DevProcessInfo[],
  registered: readonly DevProcessIdentity[],
): readonly DevProcessInfo[] {
  const selected = new Set(selectDevProcesses(processes, {
    platform: PLATFORM,
    projectRoot: PROJECT_ROOT,
    profilePath: PATHS.profile_test,
    recordedProcesses: registered,
  }));
  const runtimePath = resolve(BIN_PATH_EXE);
  const conflicts = processes.filter((process) => {
    const executable = process.argv?.[0];
    const isRuntime = executable !== undefined &&
      resolve(executable) === runtimePath;
    const isRegistered = registered.some((identity) =>
      matchesDevIdentity(process, identity)
    );
    return selected.has(process.pid) || isRuntime || isRegistered;
  });
  return conflicts;
}

export async function assertNoPreexistingHubPanelMenuM3eProcesses(): Promise<
  void
> {
  const processes = await listDevProcesses();
  const registered = await readHubPanelMenuM3eProcessRegistry();
  const conflicts = preexistingHubPanelMenuM3eProcesses(processes, registered);
  const conflictPids = new Set(conflicts.map((process) => process.pid));
  for (const process of processes) {
    if (conflictPids.has(process.pid)) continue;
    const command = process.argv?.join(" ") ?? process.commandLine;
    if (!/(?:feles-build|vite)/i.test(command)) continue;
    const cwd = await Deno.realPath(`/proc/${process.pid}/cwd`).catch(() =>
      null
    );
    if (cwd === PROJECT_ROOT) conflictPids.add(process.pid);
  }
  if (conflictPids.size > 0) {
    throw new Error(
      `M3e launch refused; live checkout processes already exist: ${
        [...conflictPids].join(",")
      }`,
    );
  }
}

export function captureHubPanelMenuM3eLaunchProcesses(
  before: readonly DevProcessInfo[],
  after: readonly DevProcessInfo[],
  profilePath: string,
  registered: readonly DevProcessIdentity[],
  ownedPids: readonly number[] = [],
): readonly DevProcessIdentity[] {
  const selected = new Set(selectDevProcesses(after, {
    platform: PLATFORM,
    projectRoot: PROJECT_ROOT,
    profilePath,
    recordedProcesses: registered,
  }));
  const prior = new Map(before.map((process) => [process.pid, process]));
  const owned = after.filter((process) => {
    if (!selected.has(process.pid) || !ownedPids.includes(process.pid)) {
      return false;
    }
    const previous = prior.get(process.pid);
    return !previous || !matchesDevIdentity(previous, {
      pid: process.pid,
      startTime: process.startTime,
      commandLine: process.commandLine,
    });
  });
  if (owned.some((process) => !process.startTime || !process.commandLine)) {
    throw new Error("M3e cannot prove a started process identity");
  }
  return owned.map((process) => ({
    pid: process.pid,
    startTime: process.startTime,
    commandLine: process.commandLine,
  }));
}

export function refreshHubPanelMenuM3eOwnedProcesses(
  ownedIdentities: readonly DevProcessIdentity[],
  processes: readonly DevProcessInfo[],
): HubPanelMenuM3eOwnedProcessRefresh {
  const byPid = new Map(processes.map((process) => [process.pid, process]));
  const knownByPid = new Map(
    ownedIdentities.map((identity) => [identity.pid, identity]),
  );
  const refreshed = new Map<number, DevProcessInfo>();
  let ownershipProven = true;

  for (const identity of ownedIdentities) {
    const current = byPid.get(identity.pid);
    if (!current) continue;
    if (!matchesDevIdentity(current, identity)) {
      ownershipProven = false;
      continue;
    }
    refreshed.set(current.pid, current);
  }

  let changed = true;
  while (changed) {
    changed = false;
    for (const process of processes) {
      if (refreshed.has(process.pid)) continue;
      const previousIdentity = knownByPid.get(process.pid);
      if (previousIdentity) {
        ownershipProven = false;
        continue;
      }
      const parent = refreshed.get(process.ppid);
      if (!parent) continue;
      if (!startedNoEarlier(process.startTime, parent.startTime)) {
        ownershipProven = false;
        continue;
      }
      if (!process.startTime || !process.commandLine) {
        ownershipProven = false;
        continue;
      }
      refreshed.set(process.pid, process);
      changed = true;
    }
  }

  return {
    identities: [...new Map([
      ...ownedIdentities.map((identity) => [identity.pid, identity] as const),
      ...[...refreshed.values()].map((process) =>
        [process.pid, {
          pid: process.pid,
          startTime: process.startTime,
          commandLine: process.commandLine,
        }] as const
      ),
    ]).values()],
    liveIdentities: [...refreshed.values()].map((process) => ({
      pid: process.pid,
      startTime: process.startTime,
      commandLine: process.commandLine,
    })),
    ownershipProven,
  };
}

export async function listHubPanelMenuM3eOwnedListeningPorts(
  identities: readonly DevProcessIdentity[],
  profilePath: string,
  logPath: string,
): Promise<readonly number[]> {
  if (identities.length === 0) return [];
  const result = await runHubPanelMenuM3eOwnedCommand(
    "lsof",
    [
      "-nP",
      "-a",
      "-p",
      identities.map((identity) => identity.pid).join(","),
      "-iTCP",
      "-sTCP:LISTEN",
      "-Fn",
    ],
    profilePath,
    `${logPath}.stdout.log`,
    `${logPath}.stderr.log`,
  );
  return readHubPanelMenuM3eListeningPorts(result);
}

export function readHubPanelMenuM3eListeningPorts(
  result: HubPanelMenuM3eOwnedCommandResult,
): readonly number[] {
  const emptyInventory = result.exitCode === 1 && result.stdout === "" &&
    result.stderr === "";
  if (
    !result.completed || (!result.success && !emptyInventory) ||
    !result.cleanupProven
  ) {
    throw new Error("M3e could not inventory owned TCP listening ports");
  }
  return [
    ...new Set([...result.stdout.matchAll(/:(\d+)(?:->|$)/gm)].map(
      (match) => Number(match[1]),
    )),
  ].sort((left, right) => left - right);
}

export function hubPanelMenuM3eProcessExited(
  identity: DevProcessIdentity,
  stat: string | null,
): boolean {
  if (stat === null) return true;
  const fields = stat.slice(stat.lastIndexOf(")") + 2).trim().split(/\s+/);
  if (
    !/^\d+$/.test(fields[19]) || !/^[a-z0-9-]+:\d+$/.test(identity.startTime) ||
    Number(stat.slice(0, stat.indexOf(" "))) !== identity.pid
  ) throw new Error("M3e kernel process identity is invalid");
  return !identity.startTime.endsWith(`:${fields[19]}`) || fields[0] === "Z" ||
    fields[0] === "X";
}

export async function terminateHubPanelMenuM3eOwnedProcesses(
  identities: readonly DevProcessIdentity[],
): Promise<boolean> {
  const before = await ownedM3eProcesses();
  const byPid = new Map(before.map((process) => [process.pid, process]));
  const targets: DevProcessInfo[] = [];
  for (const identity of identities) {
    const process = byPid.get(identity.pid);
    if (!process) continue;
    if (!matchesDevIdentity(process, identity)) return false;
    targets.push(process);
  }
  await terminateDevProcesses(targets);
  const after = await ownedM3eProcesses();
  return identities.every((identity) => {
    const current = after.find((process) => process.pid === identity.pid);
    return !current || !matchesDevIdentity(current, identity);
  });
}

export function areHubPanelMenuM3ePortsClosed(
  ports: readonly number[],
): boolean {
  for (const port of ports) {
    for (const hostname of ["127.0.0.1", "::1"]) {
      try {
        const listener = Deno.listen({ transport: "tcp", hostname, port });
        listener.close();
      } catch {
        return false;
      }
    }
  }
  return true;
}

export async function readHubPanelMenuM3eProcessRegistry(): Promise<
  readonly DevProcessIdentity[]
> {
  try {
    return parseDevProcessRegistry(
      JSON.parse(await Deno.readTextFile(DEV_PROCESSES_FILE)),
    );
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return [];
    throw error;
  }
}
