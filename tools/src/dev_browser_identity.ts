// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path/windows";
import {
  matchesDevIdentity,
  normalizeDevPath,
  splitDevCommandLine,
} from "./dev_process_selection.ts";
import type { DevProcessIdentity } from "./dev_process_types.ts";
import { commandOutput } from "./dev_process_command.ts";
import { PLATFORM } from "./defines.ts";
import type {
  DevBrowserListenerRecord,
  DevBrowserProcessRecord,
  WindowsBrowserIdentityDeps,
  WindowsBrowserIdentityTarget,
  WindowsBrowserListenerIdentity,
  WindowsBrowserListenerSelection,
} from "./dev_browser_identity_types.ts";

const DEFAULT_RETRY_COUNT = 10;
const DEFAULT_RETRY_DELAY_MS = 100;

export async function ownsDevBrowserListener(
  port: number,
  browserPids: readonly number[],
  signal: AbortSignal,
  timeoutMs: number,
): Promise<boolean> {
  if (
    !Number.isInteger(port) || port < 1 || port > 65535 ||
    !Number.isFinite(timeoutMs) || timeoutMs <= 0
  ) throw new TypeError("Invalid listener inspection budget or port");
  const controller = new AbortController();
  const abort = () => controller.abort(signal.reason);
  signal.addEventListener("abort", abort, { once: true });
  if (signal.aborted) abort();
  const timer = setTimeout(
    () => controller.abort(new Error("Listener inspection timed out")),
    timeoutMs,
  );
  try {
    const signal = controller.signal;
    signal.throwIfAborted();
    if (PLATFORM === "windows") {
      const rows = await listWindowsBrowserListeners(port, signal, timeoutMs);
      signal.throwIfAborted();
      return rows.length > 0 &&
        rows.every((row) =>
          row.localPort === port && row.state.toLowerCase() === "listen" &&
          browserPids.includes(row.owningProcess)
        );
    }
    if (PLATFORM === "darwin") {
      const output = await commandOutput(
        "lsof",
        ["-nP", `-iTCP:${port}`, "-sTCP:LISTEN", "-Fp"],
        undefined,
        signal,
      );
      signal.throwIfAborted();
      const rows = output.trim().split(/\r?\n/);
      return rows.length > 0 &&
        rows.every((row) =>
          /^p\d+$/.test(row) && browserPids.includes(Number(row.slice(1)))
        );
    }
    const inodes = new Set<string>();
    const hexPort = port.toString(16).toUpperCase().padStart(4, "0");
    for (const file of ["/proc/net/tcp", "/proc/net/tcp6"]) {
      const content = await Deno.readTextFile(file, { signal });
      for (const line of content.split("\n").slice(1)) {
        const fields = line.trim().split(/\s+/);
        if (
          fields[1]?.endsWith(`:${hexPort}`) && fields[3] === "0A" && fields[9]
        ) inodes.add(fields[9]);
      }
    }
    if (inodes.size === 0) return false;
    const owned = new Set<string>();
    for (const pid of browserPids) {
      for await (const descriptor of Deno.readDir(`/proc/${pid}/fd`)) {
        signal.throwIfAborted();
        try {
          const target = await Deno.readLink(
            `/proc/${pid}/fd/${descriptor.name}`,
          );
          const match = target.match(/^socket:\[(\d+)\]$/);
          if (match && inodes.has(match[1])) owned.add(match[1]);
        } catch (error) {
          if (!(error instanceof Deno.errors.NotFound)) throw error;
        }
      }
    }
    signal.throwIfAborted();
    return [...inodes].every((inode) => owned.has(inode));
  } finally {
    clearTimeout(timer);
    signal.removeEventListener("abort", abort);
  }
}

const WINDOWS_LISTENER_SNAPSHOT_SCRIPT = String.raw`
$ErrorActionPreference = 'Stop'
$port = 0
if (-not [int]::TryParse($env:DEV_BROWSER_LISTENER_PORT, [ref]$port)) {
  throw 'DEV_BROWSER_LISTENER_PORT must be an integer'
}
if ($port -lt 1 -or $port -gt 65535) {
  throw 'DEV_BROWSER_LISTENER_PORT is outside the TCP port range'
}
$rows = @(
  Get-NetTCPConnection -State Listen -ErrorAction Stop |
    Where-Object { [int]$_.LocalPort -eq $port } |
    ForEach-Object {
      [pscustomobject]@{
        localPort = [int]$_.LocalPort
        state = [string]$_.State
        owningProcess = [int]$_.OwningProcess
      }
    }
)
ConvertTo-Json -InputObject $rows -Compress -Depth 3
`.trim();

const WINDOWS_PROCESS_SNAPSHOT_SCRIPT = String.raw`
$ErrorActionPreference = 'Stop'
$rows = @(
  Get-CimInstance Win32_Process |
    ForEach-Object {
      [pscustomobject]@{
        pid = [int]$_.ProcessId
        ppid = [int]$_.ParentProcessId
        name = [string]$_.Name
        executablePath = if ($null -eq $_.ExecutablePath) { $null } else { [string]$_.ExecutablePath }
        commandLine = if ($null -eq $_.CommandLine) { $null } else { [string]$_.CommandLine }
        startTime = if ($null -eq $_.CreationDate) { '' } else { [string]$_.CreationDate.ToUniversalTime().ToString('o') }
      }
    }
)
ConvertTo-Json -InputObject $rows -Compress -Depth 3
`.trim();

async function runPowerShellJson(
  script: string,
  environment: Record<string, string> = {},
  signal?: AbortSignal,
  timeoutMs = 5000,
): Promise<string> {
  if (!Number.isFinite(timeoutMs) || timeoutMs <= 0) {
    throw new TypeError("Invalid listener inspection timeout");
  }
  const controller = new AbortController();
  const abort = () => controller.abort(signal?.reason);
  signal?.addEventListener("abort", abort, { once: true });
  if (signal?.aborted) abort();
  const timer = setTimeout(
    () => controller.abort(new Error("Listener inspection timed out")),
    timeoutMs,
  );
  try {
    return await commandOutput(
      "powershell",
      ["-NoProfile", "-NonInteractive", "-Command", script],
      undefined,
      controller.signal,
      environment,
    );
  } finally {
    clearTimeout(timer);
    signal?.removeEventListener("abort", abort);
  }
}

function objectRows(raw: string, label: string): Record<string, unknown>[] {
  const trimmed = raw.trim();
  if (!trimmed) throw new Error(`${label} returned an empty response`);
  let parsed: unknown;
  try {
    parsed = JSON.parse(trimmed.replace(/^\uFEFF/, ""));
  } catch {
    throw new Error(`${label} returned invalid JSON`);
  }
  if (parsed === null) throw new Error(`${label} returned null`);
  const rows = Array.isArray(parsed) ? parsed : [parsed];
  return rows.map((row: unknown, index: number) => {
    if (typeof row !== "object" || row === null || Array.isArray(row)) {
      throw new Error(`${label} row ${index} is not an object`);
    }
    return row as Record<string, unknown>;
  });
}

function integerProperty(
  row: Record<string, unknown>,
  name: string,
  label: string,
  minimum: number,
): number {
  const value = row[name];
  if (!Number.isInteger(value) || (value as number) < minimum) {
    throw new Error(`${label}.${name} must be an integer >= ${minimum}`);
  }
  return value as number;
}

function stringProperty(
  row: Record<string, unknown>,
  name: string,
  label: string,
): string {
  const value = row[name];
  if (typeof value !== "string" || value.length === 0) {
    throw new Error(`${label}.${name} must be a non-empty string`);
  }
  return value;
}

function nullableStringProperty(
  row: Record<string, unknown>,
  name: string,
  label: string,
): string | null {
  const value = row[name];
  if (value === null) return null;
  if (typeof value !== "string") {
    throw new Error(`${label}.${name} must be a string or null`);
  }
  return value;
}

export function parseWindowsBrowserListenerSnapshot(
  raw: string,
): DevBrowserListenerRecord[] {
  return objectRows(raw, "TCP listener snapshot").map((row, index) => ({
    localPort: integerProperty(
      row,
      "localPort",
      `TCP listener row ${index}`,
      1,
    ),
    state: stringProperty(row, "state", `TCP listener row ${index}`),
    owningProcess: integerProperty(
      row,
      "owningProcess",
      `TCP listener row ${index}`,
      1,
    ),
  }));
}

export function parseWindowsBrowserProcessSnapshot(
  raw: string,
): DevBrowserProcessRecord[] {
  // System/idle rows without a lifetime can never establish ownership.
  return objectRows(raw, "Win32_Process snapshot")
    .filter((row) => row.pid !== 0 && row.startTime !== "")
    .map((row, index) => ({
      pid: integerProperty(row, "pid", `Win32_Process row ${index}`, 1),
      ppid: integerProperty(
        row,
        "ppid",
        `Win32_Process row ${index}`,
        0,
      ),
      name: stringProperty(row, "name", `Win32_Process row ${index}`),
      executablePath: nullableStringProperty(
        row,
        "executablePath",
        `Win32_Process row ${index}`,
      ),
      commandLine: nullableStringProperty(
        row,
        "commandLine",
        `Win32_Process row ${index}`,
      ),
      startTime: stringProperty(
        row,
        "startTime",
        `Win32_Process row ${index}`,
      ),
    }));
}

export async function listWindowsBrowserListeners(
  port: number,
  signal?: AbortSignal,
  timeoutMs = 5000,
): Promise<DevBrowserListenerRecord[]> {
  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    throw new Error("Listener port must be an integer in 1..65535");
  }
  return parseWindowsBrowserListenerSnapshot(
    await runPowerShellJson(
      WINDOWS_LISTENER_SNAPSHOT_SCRIPT,
      {
        DEV_BROWSER_LISTENER_PORT: String(port),
      },
      signal,
      timeoutMs,
    ),
  );
}

export async function listWindowsBrowserProcesses(
  signal?: AbortSignal,
  timeoutMs = 5000,
): Promise<
  DevBrowserProcessRecord[]
> {
  return parseWindowsBrowserProcessSnapshot(
    await runPowerShellJson(
      WINDOWS_PROCESS_SNAPSHOT_SCRIPT,
      {},
      signal,
      timeoutMs,
    ),
  );
}

function processIndex(
  processes: readonly DevBrowserProcessRecord[],
): Map<number, DevBrowserProcessRecord> {
  const result = new Map<number, DevBrowserProcessRecord>();
  for (const process of processes) {
    if (result.has(process.pid)) {
      throw new Error(
        `Win32_Process snapshot contains duplicate PID ${process.pid}`,
      );
    }
    result.set(process.pid, process);
  }
  return result;
}

function sameExecutablePath(actual: string | null, expected: string): boolean {
  return actual !== null &&
    normalizeDevPath(actual, "windows") ===
      normalizeDevPath(expected, "windows");
}

function hasExactProfileArgument(
  commandLine: string,
  expectedProfilePath: string,
): boolean {
  const expected = normalizeDevPath(expectedProfilePath, "windows");
  if (!path.isAbsolute(expectedProfilePath)) return false;
  const argv = splitDevCommandLine(commandLine, "windows");
  const profiles = argv.flatMap((arg, index) => {
    const value = arg === "--profile"
      ? argv[index + 1]
      : arg.startsWith("--profile=")
      ? arg.slice(10)
      : undefined;
    return value === undefined ? [] : [value];
  });
  return profiles.length === 1 && path.isAbsolute(profiles[0]) &&
    normalizeDevPath(profiles[0], "windows") === expected;
}

function isContentProcess(commandLine: string): boolean {
  return /(?:^|\s)["']?-contentproc["']?(?:\s|$)/i.test(commandLine);
}

function matchesRecordedIdentity(
  process: DevBrowserProcessRecord,
  identity: DevProcessIdentity,
): boolean {
  return matchesDevIdentity({
    pid: process.pid,
    ppid: process.ppid,
    name: process.name,
    commandLine: process.commandLine ?? "",
    startTime: process.startTime,
  }, identity);
}

function captureIdentity(
  process: DevBrowserProcessRecord,
): WindowsBrowserListenerIdentity {
  if (!process.startTime) {
    throw new Error(`TCP listener root PID ${process.pid} has no start time`);
  }
  if (!process.executablePath) {
    throw new Error(
      `TCP listener root PID ${process.pid} has no executable path`,
    );
  }
  return {
    pid: process.pid,
    startTime: process.startTime,
    commandLine: process.commandLine ?? "",
    executablePath: process.executablePath,
  };
}

/** Select the unique actual browser listener root from one Windows snapshot. */
export function selectWindowsBrowserListenerIdentity(
  selection: WindowsBrowserListenerSelection,
): WindowsBrowserListenerIdentity {
  if (
    !Number.isInteger(selection.port) || selection.port < 1 ||
    selection.port > 65535
  ) {
    throw new Error("Listener port must be an integer in 1..65535");
  }
  if (!path.isAbsolute(selection.expectedBinaryPath)) {
    throw new Error("Expected browser executable must be absolute");
  }
  if (!path.isAbsolute(selection.expectedProfilePath)) {
    throw new Error("Expected owned profile must be absolute");
  }
  const listenRows = selection.listeners.filter((listener) =>
    listener.localPort === selection.port &&
    listener.state.toLowerCase() === "listen"
  );
  const owners = new Set(listenRows.map((listener) => listener.owningProcess));
  if (owners.size !== 1) {
    throw new Error(
      `Expected exactly one owner for TCP port ${selection.port}; found ${owners.size}`,
    );
  }
  const ownerPid = owners.values().next().value as number;
  const byPid = processIndex(selection.processes);
  const owner = byPid.get(ownerPid);
  if (!owner) {
    throw new Error(
      `TCP port ${selection.port} owner PID ${ownerPid} was not found`,
    );
  }
  if (!sameExecutablePath(owner.executablePath, selection.expectedBinaryPath)) {
    throw new Error(
      `TCP port ${selection.port} owner PID ${ownerPid} is not the expected browser executable`,
    );
  }
  if (owner.commandLine === null) {
    throw new Error(
      `TCP port ${selection.port} owner PID ${ownerPid} has no command line`,
    );
  }
  if (isContentProcess(owner.commandLine)) {
    throw new Error(`TCP port ${selection.port} is owned by a content process`);
  }
  if (
    !hasExactProfileArgument(owner.commandLine, selection.expectedProfilePath)
  ) {
    throw new Error(
      `TCP port ${selection.port} owner PID ${ownerPid} does not use the exact owned profile`,
    );
  }
  const identity = captureIdentity(owner);
  const recorded = selection.recordedProcesses.find((entry) =>
    entry.pid === ownerPid
  );
  if (recorded && matchesRecordedIdentity(owner, recorded)) return identity;
  if (owner.ppid <= 0) {
    throw new Error(`TCP listener root PID ${ownerPid} has no direct parent`);
  }
  const parent = byPid.get(owner.ppid);
  if (!parent) {
    throw new Error(
      `TCP listener root PID ${ownerPid} parent ${owner.ppid} is not live`,
    );
  }
  const recordedParent = selection.recordedProcesses.find((entry) =>
    entry.pid === parent.pid
  );
  if (!recordedParent || !matchesRecordedIdentity(parent, recordedParent)) {
    throw new Error(
      `TCP listener root PID ${ownerPid} parent ${parent.pid} does not match a recorded generation`,
    );
  }
  const parentStart = Date.parse(parent.startTime);
  const childStart = Date.parse(identity.startTime);
  if (!Number.isFinite(parentStart) || !Number.isFinite(childStart)) {
    throw new Error("Listener parent and child need parseable start times");
  }
  if (childStart < parentStart) {
    throw new Error(
      `TCP listener root PID ${ownerPid} predates its recorded parent`,
    );
  }
  return identity;
}

/** Recheck the candidate against a fresh snapshot before any registry write. */
export function revalidateWindowsBrowserListenerIdentity(
  identity: WindowsBrowserListenerIdentity,
  current: DevBrowserProcessRecord | undefined,
): boolean {
  return current !== undefined && current.pid === identity.pid &&
    current.startTime === identity.startTime &&
    (current.commandLine ?? "") === identity.commandLine &&
    sameExecutablePath(current.executablePath, identity.executablePath);
}

/** Bounded capture/registration of the actual Windows listener root. */
export async function captureWindowsBrowserListenerIdentity(
  target: WindowsBrowserIdentityTarget,
  deps: WindowsBrowserIdentityDeps,
): Promise<WindowsBrowserListenerIdentity> {
  const retryCount = target.retryCount ?? DEFAULT_RETRY_COUNT;
  const retryDelayMs = target.retryDelayMs ?? DEFAULT_RETRY_DELAY_MS;
  const timeoutMs = target.timeoutMs ?? 30_000;
  if (
    !Number.isInteger(retryCount) || retryCount < 1 || retryCount > 100 ||
    !Number.isInteger(retryDelayMs) || retryDelayMs < 0 || retryDelayMs > 5000
  ) {
    throw new Error("Listener retry budget must be finite and bounded");
  }
  if (!Number.isFinite(timeoutMs) || timeoutMs <= 0) {
    throw new TypeError("Invalid listener registration timeout");
  }
  const controller = new AbortController();
  const abort = () => controller.abort(target.signal?.reason);
  target.signal?.addEventListener("abort", abort, { once: true });
  if (target.signal?.aborted) abort();
  const deadline = performance.now() + timeoutMs;
  const timeout = new Error("Listener registration timed out");
  const remaining = () => {
    const budget = deadline - performance.now();
    if (budget <= 0) controller.abort(timeout);
    controller.signal.throwIfAborted();
    return budget;
  };
  const timer = setTimeout(() => controller.abort(timeout), timeoutMs);
  const signal = controller.signal;
  try {
    let lastError: unknown;
    for (let attempt = 0; attempt < retryCount; attempt++) {
      remaining();
      try {
        const [listeners, processes] = await Promise.allSettled([
          Promise.resolve().then(() =>
            deps.listListeners(target.port, signal, remaining())
          ),
          Promise.resolve().then(() => deps.listProcesses(signal, remaining())),
        ]);
        remaining();
        if (listeners.status === "rejected") throw listeners.reason;
        if (processes.status === "rejected") throw processes.reason;
        const identity = selectWindowsBrowserListenerIdentity({
          port: target.port,
          expectedBinaryPath: target.expectedBinaryPath,
          expectedProfilePath: target.expectedProfilePath,
          recordedProcesses: target.recordedProcesses,
          listeners: listeners.value,
          processes: processes.value,
        });
        const fresh = await deps.listProcesses(signal, remaining());
        remaining();
        const current = fresh.find((process) => process.pid === identity.pid);
        if (!revalidateWindowsBrowserListenerIdentity(identity, current)) {
          throw new Error(
            "Listener candidate identity changed before persistence",
          );
        }
        const alreadyRegistered = target.recordedProcesses.find((entry) =>
          entry.pid === identity.pid && matchesRecordedIdentity(current!, entry)
        );
        if (
          !alreadyRegistered && !(await deps.recordIdentity(identity, signal))
        ) {
          throw new Error("Listener identity could not be recorded");
        }
        remaining();
        return identity;
      } catch (error) {
        signal.throwIfAborted();
        lastError = error;
        if (attempt + 1 < retryCount) {
          await deps.sleep(Math.min(retryDelayMs, remaining()), signal);
        }
      }
    }
    throw lastError instanceof Error
      ? lastError
      : new Error("Windows browser listener identity capture failed");
  } finally {
    clearTimeout(timer);
    target.signal?.removeEventListener("abort", abort);
  }
}
