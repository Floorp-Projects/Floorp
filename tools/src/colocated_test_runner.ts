// SPDX-License-Identifier: MPL-2.0

import { parseArgs } from "@std/cli";
import { walkSync } from "@std/fs";
import * as path from "@std/path";
import { BIN_PATH_EXE, PATHS, PROJECT_ROOT } from "./defines.ts";
import {
  detectLayer,
  escapeRegExp,
  isResultMatchTarget,
  isTestFile,
  normalizeBrowserResultPath,
  normalizeSlashes,
  parseLayer,
  type TestLayer,
} from "./colocated_test_utils.ts";
import { sleep } from "./async_utils.ts";
import {
  type BrowserTestCollection,
  type BrowserTestResult,
  collectBrowserTestResultsFromPrefs,
} from "./browser_test_collector.ts";

export interface RunnerOptions {
  near?: string;
  listOnly: boolean;
  layer: TestLayer;
  autoStart: boolean;
  timeoutMs: number;
  startupTimeoutMs: number;
  help: boolean;
}

const TEST_LOG_DIR = path.join(PROJECT_ROOT, "logs", "test");
const MARIONETTE_PORT_FILE = path.join(
  PROJECT_ROOT,
  "_dist",
  "marionette-port.txt",
);
const TEST_FILTER_PREF = "nora.tests.filter";
const TEST_FILTER_COUNT_PREF = "nora.tests.filter.count";
const TEST_FILTER_ITEM_PREF_PREFIX = "nora.tests.filter.";
const TEST_RUN_ID_PREF = "nora.tests.run_id";
const TEST_CONTROL_FILE = "nora-tests-control.json";
const TEST_CONTROL_SCHEMA_VERSION = 1;
const MAX_TEST_EXECUTION_TIMEOUT_MS = 1_800_000;
const DEFAULT_TEST_COLLECTION_TIMEOUT_MS = MAX_TEST_EXECUTION_TIMEOUT_MS;
const DEFAULT_AUTOSTART_READY_TIMEOUT_MS = MAX_TEST_EXECUTION_TIMEOUT_MS;
const AUTOSTART_POLL_INTERVAL_MS = 1_000;
const AUTOSTART_STOP_TIMEOUT_MS = 5_000;
const ALLOWED_LONG_OPTIONS = new Set([
  "near",
  "env",
  "layer",
  "list",
  "no-autostart",
  "timeout-ms",
  "startup-timeout-ms",
  "help",
]);
const ALLOWED_SHORT_OPTIONS = new Set(["n", "l", "h"]);

const HELP = `
Usage: deno task test [path] [options]

Options:
  --near, -n <path>              Run tests near a directory or source file
  --list, -l                     List resolved targets without executing
  --layer <all|chrome|esm|pages> Filter by test layer (default: all)
  --timeout-ms <ms>              Browser result collection timeout (default/max: 1800000)
  --startup-timeout-ms <ms>      Auto-start browser ready timeout (default/max: 1800000)
  --no-autostart                 Do not auto-start browser when unavailable
  --env browser                  Accepted for compatibility; host mode is not supported
  --help, -h                     Show this help
`.trim();

type LogLevel = "INFO" | "ERROR";

const COLOR_ENABLED = (() => {
  try {
    return Deno.stdout.isTerminal() && !Deno.noColor;
  } catch {
    return false;
  }
})();

function paint(text: string, ansiCode: string): string {
  if (!COLOR_ENABLED) {
    return text;
  }
  return `\x1b[${ansiCode}m${text}\x1b[0m`;
}

function formatConsoleLine(level: LogLevel, message: string): string {
  if (message === "") {
    return "";
  }

  const levelLabel = level === "ERROR"
    ? paint("ERROR", "1;31")
    : paint("INFO", "1;36");

  let formattedMessage = message;
  if (level === "INFO" && message.startsWith("✓")) {
    formattedMessage = paint(message, "32");
  } else if (level === "ERROR" && message.startsWith("✗")) {
    formattedMessage = paint(message, "31");
  } else if (message.startsWith("Browser test result:")) {
    formattedMessage = message.includes("0 failed")
      ? paint(message, "1;32")
      : paint(message, "1;33");
  } else if (message.startsWith("=== ") && message.endsWith(" ===")) {
    formattedMessage = paint(message, "1;35");
  }

  return `[${levelLabel}] ${formattedMessage}`;
}

const SOURCE_EXTENSIONS = new Set([
  ".ts",
  ".mts",
  ".tsx",
  ".js",
  ".mjs",
  ".jsx",
]);

const WALK_SKIP_PATTERNS = [
  /(?:^|[\\/])_dist(?:[\\/]|$)/,
  /(?:^|[\\/])\.git(?:[\\/]|$)/,
  /(?:^|[\\/])node_modules(?:[\\/]|$)/,
  /(?:^|[\\/])\.venv(?:[\\/]|$)/,
  /(?:^|[\\/])\.vscode(?:[\\/]|$)/,
  /(?:^|[\\/])logs(?:[\\/]|$)/,
  /(?:^|[\\/])libs[\\/]@types[\\/]gecko(?:[\\/]|$)/,
];

function discoverAllTests(): string[] {
  const files: string[] = [];

  for (
    const entry of walkSync(PROJECT_ROOT, {
      includeDirs: false,
      followSymlinks: false,
      skip: WALK_SKIP_PATTERNS,
    })
  ) {
    const ext = path.extname(entry.path);
    if (!SOURCE_EXTENSIONS.has(ext)) {
      continue;
    }

    const relPath = path.relative(PROJECT_ROOT, entry.path);
    if (isTestFile(relPath)) {
      files.push(path.normalize(entry.path));
    }
  }

  return files.sort((a, b) => a.localeCompare(b));
}

function isBrowserDiscoverableTest(relPath: string): boolean {
  const normalized = relPath.replaceAll("\\", "/");
  return (
    /^browser-features\/chrome\/(?:.*\/)?test\/.*\.test\.(?:ts|mts|tsx|js|mjs|jsx)$/
      .test(
        normalized,
      ) ||
    /^browser-features\/modules\/.*\.test\.(?:ts|mts|tsx|js|mjs|jsx)$/.test(
      normalized,
    ) ||
    /^browser-features\/pages-[^/]+\/.*\.test\.(?:ts|mts|tsx|js|mjs|jsx)$/.test(
      normalized,
    )
  );
}

function discoverBrowserTests(): string[] {
  return discoverAllTests().filter((absFile) => {
    const relPath = path.relative(PROJECT_ROOT, absFile);
    return isBrowserDiscoverableTest(relPath);
  });
}

function filterByLayer(files: string[], layer: TestLayer): string[] {
  if (layer === "all") {
    return files;
  }

  return files.filter((absFile) => {
    const relPath = path.relative(PROJECT_ROOT, absFile);
    return detectLayer(relPath) === layer;
  });
}

function isInside(parentDir: string, candidate: string): boolean {
  const rel = path.relative(parentDir, candidate);
  return rel === "" || (!rel.startsWith("..") && !path.isAbsolute(rel));
}

function filterNear(files: string[], near: string): string[] {
  const nearAbs = path.isAbsolute(near)
    ? path.normalize(near)
    : path.normalize(path.join(PROJECT_ROOT, near));

  let nearStat: Deno.FileInfo;
  try {
    nearStat = Deno.statSync(nearAbs);
  } catch {
    throw new Error(`--near path was not found: ${near}`);
  }

  const nearDir = nearStat.isDirectory ? nearAbs : path.dirname(nearAbs);
  const nearName = nearStat.isDirectory
    ? ""
    : path.basename(nearAbs).replace(/\.[^.]+$/, "");

  const nearNameVariants = new Set<string>();
  if (nearName) {
    nearNameVariants.add(nearName);
    nearNameVariants.add(nearName.replace(/\.(sys|impl)$/, ""));
    nearNameVariants.add(nearName.replace(/\.(sys|impl|test|spec)$/, ""));
  }

  return files.filter((absFile) => {
    if (!isInside(nearDir, absFile)) {
      return false;
    }

    const relFromNear = path.relative(nearDir, absFile).replaceAll("\\", "/");
    if (relFromNear.startsWith("test/")) {
      return true;
    }

    if (!nearName) {
      return /\.(test|spec)\.(?:ts|mts|tsx|js|mjs|jsx)$/.test(relFromNear);
    }

    for (const name of nearNameVariants) {
      if (!name) {
        continue;
      }
      if (
        new RegExp(
          `^${escapeRegExp(name)}\\.(test|spec)\\.(?:ts|mts|tsx|js|mjs|jsx)$`,
        ).test(relFromNear)
      ) {
        return true;
      }
    }

    return false;
  });
}

function errorToMessage(error: unknown): string {
  if (error instanceof Error) {
    return error.message;
  }
  return String(error);
}

function ensureSupportedOptions(args: string[]): void {
  for (const arg of args) {
    if (arg === "--") {
      break;
    }

    if (!arg.startsWith("-") || arg === "-") {
      continue;
    }

    if (arg.startsWith("--")) {
      const [name] = arg.slice(2).split("=", 1);
      if (!ALLOWED_LONG_OPTIONS.has(name)) {
        throw new Error(
          `Unknown option: --${name}. Use --help to see supported options.`,
        );
      }
      continue;
    }

    const shortGroup = arg.slice(1);
    for (const shortName of shortGroup) {
      if (!ALLOWED_SHORT_OPTIONS.has(shortName)) {
        throw new Error(
          `Unknown option: -${shortName}. Use --help to see supported options.`,
        );
      }
    }
  }
}

function parsePositiveIntegerMs(
  rawValue: string | number | undefined,
  flagName: string,
  fallback: number,
  maxValue: number,
): number {
  if (rawValue === undefined) {
    return fallback;
  }

  const value = Number(rawValue);
  if (!Number.isInteger(value) || value <= 0 || value > maxValue) {
    throw new Error(
      `Invalid --${flagName} value: ${rawValue}. Use a positive integer (milliseconds) up to ${maxValue}.`,
    );
  }

  return value;
}

export function parseOptions(args: string[]): RunnerOptions {
  ensureSupportedOptions(args);

  const parsed = parseArgs(args, {
    string: ["near", "env", "layer", "timeout-ms", "startup-timeout-ms"],
    boolean: ["list", "no-autostart", "help"],
    alias: {
      n: "near",
      l: "list",
      h: "help",
    },
    default: {
      list: false,
      layer: "all",
      "timeout-ms": String(DEFAULT_TEST_COLLECTION_TIMEOUT_MS),
      "startup-timeout-ms": String(DEFAULT_AUTOSTART_READY_TIMEOUT_MS),
    },
  });

  if (parsed.help) {
    return {
      listOnly: true,
      layer: "all",
      autoStart: true,
      timeoutMs: DEFAULT_TEST_COLLECTION_TIMEOUT_MS,
      startupTimeoutMs: DEFAULT_AUTOSTART_READY_TIMEOUT_MS,
      help: true,
    };
  }

  const positional = parsed._.map(String);
  if (parsed.near && positional.length > 0) {
    throw new Error("Use either --near or a positional path, not both.");
  }
  if (positional.length > 1) {
    throw new Error("Only one positional path is supported.");
  }

  if (parsed.env && parsed.env !== "browser") {
    throw new Error(
      "Only browser integration tests are supported. Remove --env host.",
    );
  }

  return {
    near: parsed.near ?? positional[0],
    listOnly: Boolean(parsed.list),
    layer: parseLayer(parsed.layer),
    autoStart: parsed["no-autostart"] !== true,
    timeoutMs: parsePositiveIntegerMs(
      parsed["timeout-ms"],
      "timeout-ms",
      DEFAULT_TEST_COLLECTION_TIMEOUT_MS,
      MAX_TEST_EXECUTION_TIMEOUT_MS,
    ),
    startupTimeoutMs: parsePositiveIntegerMs(
      parsed["startup-timeout-ms"],
      "startup-timeout-ms",
      DEFAULT_AUTOSTART_READY_TIMEOUT_MS,
      MAX_TEST_EXECUTION_TIMEOUT_MS,
    ),
    help: false,
  };
}

function readMarionettePortFromFile(): number | null {
  try {
    const content = Deno.readTextFileSync(MARIONETTE_PORT_FILE).trim();
    const port = parseInt(content, 10);
    return Number.isNaN(port) ? null : port;
  } catch {
    return null;
  }
}

async function _isTcpPortReachable(
  port: number,
  timeoutMs = 1_500,
): Promise<boolean> {
  try {
    const conn = await Deno.connect({
      hostname: "127.0.0.1",
      port,
      signal: AbortSignal.timeout(timeoutMs),
    });
    try {
      return true;
    } finally {
      conn.close();
    }
  } catch {
    return false;
  }
}

async function hasRunningTestBrowser(): Promise<boolean> {
  const port = readMarionettePortFromFile();
  if (!port) {
    return false;
  }

  return await _isTcpPortReachable(port);
}

export interface WindowsProcessRecord {
  processId: number;
  parentProcessId: number;
  creationDate: string | null;
  executablePath: string | null;
  commandLine: string | null;
}

export interface WindowsListenerRecord {
  localAddress: string;
  localPort: number;
  state: string;
  owningProcess: number;
}

export interface WindowsProcessIdentity {
  processId: number;
  creationDate: string;
  executablePath: string;
  commandLine: string | null;
}

export interface WindowsAutoStartState {
  deno: WindowsProcessIdentity;
  floorpExecutablePath: string;
  ownedFloorp: Map<number, WindowsProcessIdentity>;
  blockedFloorpProcessIds: Set<number>;
  ambiguousFloorp: string[];
  listenerRoot: WindowsProcessIdentity | null;
  port: number | null;
  treeKillSafe: boolean;
}

export interface WindowsProcessControlDeps {
  listProcesses(): Promise<WindowsProcessRecord[]>;
  listListeners(port: number): Promise<WindowsListenerRecord[]>;
  taskkill(
    processId: number,
    includeTree: boolean,
  ): Promise<{ success: boolean; code: number }>;
  isPortReachable(port: number): Promise<boolean>;
  sleep(ms: number): Promise<void>;
}

const WINDOWS_PROCESS_SNAPSHOT_SCRIPT = String.raw`
$ErrorActionPreference = 'Stop'
$rows = @(
  Get-CimInstance -ClassName Win32_Process -ErrorAction Stop |
    ForEach-Object {
      [pscustomobject]@{
        ProcessId = [int]$_.ProcessId
        ParentProcessId = [int]$_.ParentProcessId
        CreationDate = if ($null -eq $_.CreationDate) { $null } else { [string]$_.CreationDate }
        ExecutablePath = if ($null -eq $_.ExecutablePath) { $null } else { [string]$_.ExecutablePath }
        CommandLine = if ($null -eq $_.CommandLine) { $null } else { [string]$_.CommandLine }
      }
    }
)
ConvertTo-Json -InputObject $rows -Compress -Depth 4
`.trim();

const WINDOWS_LISTENER_SNAPSHOT_SCRIPT = String.raw`
$ErrorActionPreference = 'Stop'
$port = 0
if (-not [int]::TryParse($env:FLOORP_COLOCATED_LOCAL_PORT, [ref]$port)) {
  throw 'FLOORP_COLOCATED_LOCAL_PORT must be an integer'
}
if ($port -lt 1 -or $port -gt 65535) {
  throw 'FLOORP_COLOCATED_LOCAL_PORT is outside the TCP port range'
}
$connections = @(Get-NetTCPConnection -ErrorAction Stop)
$rows = @(
  $connections |
    Where-Object { [int]$_.LocalPort -eq $port -and [string]$_.State -eq 'Listen' } |
    ForEach-Object {
      [pscustomobject]@{
        LocalAddress = [string]$_.LocalAddress
        LocalPort = [int]$_.LocalPort
        State = [string]$_.State
        OwningProcess = [int]$_.OwningProcess
      }
    }
)
ConvertTo-Json -InputObject $rows -Compress -Depth 4
`.trim();

const WINDOWS_DENO_IDENTITY_RETRY_COUNT = 5;
const WINDOWS_DENO_IDENTITY_RETRY_DELAY_MS = 50;
const WINDOWS_TEARDOWN_POLL_INTERVAL_MS = 250;
const WINDOWS_TEARDOWN_MAX_POLLS = Math.max(
  1,
  Math.ceil(AUTOSTART_STOP_TIMEOUT_MS / WINDOWS_TEARDOWN_POLL_INTERVAL_MS),
);

function parseJsonObjectRows(
  raw: string,
  label: string,
): Record<string, unknown>[] {
  const trimmed = raw.trim();
  if (!trimmed) {
    throw new Error(`${label} returned an empty response`);
  }

  let parsed: unknown;
  try {
    parsed = JSON.parse(trimmed);
  } catch (error) {
    throw new Error(`${label} returned invalid JSON: ${errorToMessage(error)}`);
  }

  if (parsed === null) {
    throw new Error(`${label} returned null`);
  }

  const rows = Array.isArray(parsed) ? parsed : [parsed];
  return rows.map((row, index) => {
    if (typeof row !== "object" || row === null || Array.isArray(row)) {
      throw new Error(`${label} row ${index} is not an object`);
    }
    return row as Record<string, unknown>;
  });
}

function requiredProperty(
  row: Record<string, unknown>,
  name: string,
  label: string,
): unknown {
  if (!(name in row)) {
    throw new Error(`${label} is missing ${name}`);
  }
  return row[name];
}

function requiredInteger(
  value: unknown,
  label: string,
  minimum: number,
): number {
  if (!Number.isInteger(value) || (value as number) < minimum) {
    throw new Error(`${label} must be an integer >= ${minimum}`);
  }
  return value as number;
}

function requiredString(value: unknown, label: string): string {
  if (typeof value !== "string" || value.length === 0) {
    throw new Error(`${label} must be a non-empty string`);
  }
  return value;
}

function nullableString(value: unknown, label: string): string | null {
  if (value === null) {
    return null;
  }
  if (typeof value !== "string") {
    throw new Error(`${label} must be a string or null`);
  }
  return value;
}

export function parseWindowsProcessSnapshot(
  raw: string,
): WindowsProcessRecord[] {
  return parseJsonObjectRows(raw, "Win32_Process snapshot").map(
    (row, index) => ({
      processId: requiredInteger(
        requiredProperty(row, "ProcessId", `Win32_Process row ${index}`),
        `Win32_Process row ${index}.ProcessId`,
        0,
      ),
      parentProcessId: requiredInteger(
        requiredProperty(
          row,
          "ParentProcessId",
          `Win32_Process row ${index}`,
        ),
        `Win32_Process row ${index}.ParentProcessId`,
        0,
      ),
      creationDate: nullableString(
        requiredProperty(row, "CreationDate", `Win32_Process row ${index}`),
        `Win32_Process row ${index}.CreationDate`,
      ),
      executablePath: nullableString(
        requiredProperty(row, "ExecutablePath", `Win32_Process row ${index}`),
        `Win32_Process row ${index}.ExecutablePath`,
      ),
      commandLine: nullableString(
        requiredProperty(row, "CommandLine", `Win32_Process row ${index}`),
        `Win32_Process row ${index}.CommandLine`,
      ),
    }),
  );
}

export function parseWindowsListenerSnapshot(
  raw: string,
): WindowsListenerRecord[] {
  return parseJsonObjectRows(raw, "TCP listener snapshot").map(
    (row, index) => ({
      localAddress: requiredString(
        requiredProperty(row, "LocalAddress", `TCP listener row ${index}`),
        `TCP listener row ${index}.LocalAddress`,
      ),
      localPort: requiredInteger(
        requiredProperty(row, "LocalPort", `TCP listener row ${index}`),
        `TCP listener row ${index}.LocalPort`,
        1,
      ),
      state: requiredString(
        requiredProperty(row, "State", `TCP listener row ${index}`),
        `TCP listener row ${index}.State`,
      ),
      owningProcess: requiredInteger(
        requiredProperty(row, "OwningProcess", `TCP listener row ${index}`),
        `TCP listener row ${index}.OwningProcess`,
        1,
      ),
    }),
  );
}

function normalizeWindowsExecutablePath(value: string): string {
  return value.trim().replaceAll("/", "\\").replace(/\\+$/, "").toLowerCase();
}

export function isSameWindowsExecutablePath(
  actual: string | null,
  expected: string,
): boolean {
  return actual !== null &&
    normalizeWindowsExecutablePath(actual) ===
      normalizeWindowsExecutablePath(expected);
}

function hasDenoFelesBuildTestMarker(commandLine: string | null): boolean {
  if (commandLine === null) {
    return false;
  }
  return /(?:^|\s)["']?task["']?\s+["']?feles-build["']?\s+["']?test["']?(?:\s|$)/i
    .test(commandLine);
}

function isContentProcessCommandLine(commandLine: string | null): boolean {
  return commandLine !== null &&
    /(?:^|\s)["']?-contentproc["']?(?:\s|$)/i.test(commandLine);
}

function commandLineParentPid(commandLine: string | null): number | null {
  if (commandLine === null) {
    return null;
  }
  const match = /(?:^|\s)["']?-parentPid["']?(?:\s+|=)["']?(\d+)["']?(?=\s|$)/i
    .exec(commandLine);
  if (!match) {
    return null;
  }
  const processId = Number(match[1]);
  return Number.isInteger(processId) && processId > 0 ? processId : null;
}

function processIndex(
  processes: WindowsProcessRecord[],
): Map<number, WindowsProcessRecord> {
  const result = new Map<number, WindowsProcessRecord>();
  for (const process of processes) {
    if (result.has(process.processId)) {
      throw new Error(
        `Win32_Process snapshot contains duplicate PID ${process.processId}`,
      );
    }
    result.set(process.processId, process);
  }
  return result;
}

function captureWindowsProcessIdentity(
  process: WindowsProcessRecord,
  label: string,
): WindowsProcessIdentity {
  if (process.processId <= 0) {
    throw new Error(`${label} has an invalid PID ${process.processId}`);
  }
  if (!process.creationDate) {
    throw new Error(`${label} PID ${process.processId} has no CreationDate`);
  }
  if (!process.executablePath) {
    throw new Error(`${label} PID ${process.processId} has no ExecutablePath`);
  }
  return {
    processId: process.processId,
    creationDate: process.creationDate,
    executablePath: process.executablePath,
    commandLine: process.commandLine,
  };
}

export function matchesWindowsProcessIdentity(
  process: WindowsProcessRecord,
  identity: WindowsProcessIdentity,
): boolean {
  return process.processId === identity.processId &&
    process.creationDate === identity.creationDate &&
    isSameWindowsExecutablePath(
      process.executablePath,
      identity.executablePath,
    );
}

function matchesWindowsDenoIdentity(
  process: WindowsProcessRecord,
  identity: WindowsProcessIdentity,
): boolean {
  return matchesWindowsProcessIdentity(process, identity) &&
    process.commandLine === identity.commandLine &&
    hasDenoFelesBuildTestMarker(process.commandLine);
}

export function assertWindowsAutoStartPreflight(
  processes: WindowsProcessRecord[],
  floorpExecutablePath = BIN_PATH_EXE,
): void {
  if (processes.length === 0) {
    throw new Error("Win32_Process preflight returned no processes");
  }
  processIndex(processes);
  const existing = processes.filter((process) =>
    isSameWindowsExecutablePath(
      process.executablePath,
      floorpExecutablePath,
    )
  );
  if (existing.length > 0) {
    throw new Error(
      `Refusing auto-start: ${existing.length} existing process(es) use the locked test browser executable`,
    );
  }
}

export function selectWindowsDenoIdentity(
  processes: WindowsProcessRecord[],
  processId: number,
  denoExecutablePath: string,
): WindowsProcessIdentity {
  const process = processIndex(processes).get(processId);
  if (!process) {
    throw new Error(`Auto-started Deno PID ${processId} was not found`);
  }
  if (
    !isSameWindowsExecutablePath(process.executablePath, denoExecutablePath)
  ) {
    throw new Error(
      `Auto-started Deno PID ${processId} does not match the spawned executable`,
    );
  }
  if (!hasDenoFelesBuildTestMarker(process.commandLine)) {
    throw new Error(
      `Auto-started Deno PID ${processId} does not contain the expected task marker`,
    );
  }
  return captureWindowsProcessIdentity(process, "Auto-started Deno");
}

export function createWindowsAutoStartState(
  deno: WindowsProcessIdentity,
  floorpExecutablePath = BIN_PATH_EXE,
): WindowsAutoStartState {
  return {
    deno,
    floorpExecutablePath,
    ownedFloorp: new Map<number, WindowsProcessIdentity>(),
    blockedFloorpProcessIds: new Set<number>(),
    ambiguousFloorp: [],
    listenerRoot: null,
    port: null,
    treeKillSafe: true,
  };
}

function isDescendantByParentProcessId(
  processId: number,
  ancestorProcessId: number,
  processes: Map<number, WindowsProcessRecord>,
): boolean {
  const seen = new Set<number>();
  let current = processes.get(processId);
  while (current) {
    if (seen.has(current.processId)) {
      throw new Error(
        `Win32_Process ancestry contains a cycle at PID ${current.processId}`,
      );
    }
    seen.add(current.processId);
    if (current.parentProcessId === ancestorProcessId) {
      return true;
    }
    if (current.parentProcessId <= 0) {
      return false;
    }
    current = processes.get(current.parentProcessId);
  }
  return false;
}

function addUniqueMessage(target: string[], message: string): void {
  if (!target.includes(message)) {
    target.push(message);
  }
}

export function reconcileWindowsFloorpOwnership(
  state: WindowsAutoStartState,
  processes: WindowsProcessRecord[],
  floorpExecutablePath = state.floorpExecutablePath,
): string[] {
  const issues: string[] = [];
  let byPid: Map<number, WindowsProcessRecord>;
  try {
    byPid = processIndex(processes);
  } catch (error) {
    state.treeKillSafe = false;
    throw error;
  }
  const denoProcess = byPid.get(state.deno.processId);
  const denoIsLive = denoProcess !== undefined &&
    matchesWindowsDenoIdentity(denoProcess, state.deno);
  if (
    denoProcess !== undefined &&
    !matchesWindowsDenoIdentity(denoProcess, state.deno)
  ) {
    issues.push(
      `Auto-started Deno PID ${state.deno.processId} no longer matches its captured identity`,
    );
  }

  const candidates = processes.filter((process) =>
    isSameWindowsExecutablePath(
      process.executablePath,
      floorpExecutablePath,
    )
  );
  const linked = new Set<number>();

  for (const [processId, identity] of state.ownedFloorp) {
    if (state.blockedFloorpProcessIds.has(processId)) {
      continue;
    }
    const current = byPid.get(processId);
    if (!current) {
      continue;
    }
    if (!matchesWindowsProcessIdentity(current, identity)) {
      issues.push(
        `Owned Floorp PID ${processId} no longer matches its captured identity`,
      );
      continue;
    }
    linked.add(processId);
  }

  if (denoIsLive && state.listenerRoot === null) {
    for (const candidate of candidates) {
      if (state.blockedFloorpProcessIds.has(candidate.processId)) {
        continue;
      }
      if (
        isDescendantByParentProcessId(
          candidate.processId,
          state.deno.processId,
          byPid,
        )
      ) {
        linked.add(candidate.processId);
      }
    }
  }

  let changed = true;
  while (changed) {
    changed = false;
    for (const candidate of candidates) {
      if (
        linked.has(candidate.processId) ||
        state.blockedFloorpProcessIds.has(candidate.processId)
      ) {
        continue;
      }
      const actualParentIsOwned = linked.has(candidate.parentProcessId);
      const commandParent = commandLineParentPid(candidate.commandLine);
      const commandParentIsOwned = commandParent !== null &&
        linked.has(commandParent);
      if (actualParentIsOwned || commandParentIsOwned) {
        linked.add(candidate.processId);
        changed = true;
      }
    }
  }

  for (const candidate of candidates) {
    if (state.blockedFloorpProcessIds.has(candidate.processId)) {
      addUniqueMessage(
        state.ambiguousFloorp,
        `Blocked identity-mismatched exact-path Floorp PID ${candidate.processId}`,
      );
      continue;
    }
    if (!linked.has(candidate.processId)) {
      addUniqueMessage(
        state.ambiguousFloorp,
        `Unlinked exact-path Floorp PID ${candidate.processId}`,
      );
      continue;
    }
    try {
      const identity = captureWindowsProcessIdentity(candidate, "Owned Floorp");
      const existing = state.ownedFloorp.get(candidate.processId);
      if (existing && !matchesWindowsProcessIdentity(candidate, existing)) {
        issues.push(
          `Owned Floorp PID ${candidate.processId} was reused by another process`,
        );
        continue;
      }
      state.ownedFloorp.set(candidate.processId, identity);
    } catch (error) {
      addUniqueMessage(
        state.ambiguousFloorp,
        `Exact-path Floorp PID ${candidate.processId} has an invalid identity: ${
          errorToMessage(error)
        }`,
      );
    }
  }

  if (issues.length > 0 || state.ambiguousFloorp.length > 0) {
    state.treeKillSafe = false;
  }

  return issues;
}

function matchesWindowsProcessIdentityStrict(
  process: WindowsProcessRecord,
  identity: WindowsProcessIdentity,
): boolean {
  return matchesWindowsProcessIdentity(process, identity) &&
    process.commandLine === identity.commandLine;
}

export function promoteWindowsReadyBrowserOwnership(
  state: WindowsAutoStartState,
  processes: WindowsProcessRecord[],
  listenerRoot: WindowsProcessIdentity,
  previouslyOwned: ReadonlyMap<number, WindowsProcessIdentity>,
): string[] {
  const byPid = processIndex(processes);
  const verified = new Map<number, WindowsProcessIdentity>();
  const issues: string[] = [];

  for (const [processId, identity] of previouslyOwned) {
    const current = byPid.get(processId);
    if (!current) {
      continue;
    }
    if (!matchesWindowsProcessIdentityStrict(current, identity)) {
      state.blockedFloorpProcessIds.add(processId);
      issues.push(
        `Previously owned Floorp PID ${processId} no longer matches its captured identity`,
      );
      continue;
    }
    verified.set(processId, identity);
  }

  if (!state.blockedFloorpProcessIds.has(listenerRoot.processId)) {
    verified.set(listenerRoot.processId, listenerRoot);
  }

  state.ownedFloorp.clear();
  for (const [processId, identity] of verified) {
    state.ownedFloorp.set(processId, identity);
  }

  if (issues.length > 0) {
    state.treeKillSafe = false;
  }
  return issues;
}

export function selectWindowsListenerRoot(
  port: number,
  listeners: WindowsListenerRecord[],
  processes: WindowsProcessRecord[],
  denoIdentity: WindowsProcessIdentity,
  floorpExecutablePath = BIN_PATH_EXE,
  previouslyOwned?: ReadonlyMap<number, WindowsProcessIdentity>,
): WindowsProcessIdentity {
  const listenRows = listeners.filter((listener) =>
    listener.localPort === port && listener.state.toLowerCase() === "listen"
  );
  const owners = new Set(listenRows.map((listener) => listener.owningProcess));
  if (owners.size !== 1) {
    throw new Error(
      `Expected exactly one owner for TCP port ${port}; found ${owners.size}`,
    );
  }

  const ownerPid = owners.values().next().value as number;
  const byPid = processIndex(processes);
  const denoProcess = byPid.get(denoIdentity.processId);
  if (denoProcess && !matchesWindowsDenoIdentity(denoProcess, denoIdentity)) {
    throw new Error(
      "Captured Deno identity no longer matches the process table",
    );
  }

  const owner = byPid.get(ownerPid);
  if (!owner) {
    throw new Error(`TCP port ${port} owner PID ${ownerPid} was not found`);
  }
  if (
    !isSameWindowsExecutablePath(owner.executablePath, floorpExecutablePath)
  ) {
    throw new Error(
      `TCP port ${port} owner PID ${ownerPid} is not the locked test browser executable`,
    );
  }
  if (owner.commandLine === null) {
    throw new Error(
      `TCP port ${port} owner PID ${ownerPid} has no command line`,
    );
  }
  if (isContentProcessCommandLine(owner.commandLine)) {
    throw new Error(`TCP port ${port} is owned by a Floorp content process`);
  }
  const ownerIdentity = captureWindowsProcessIdentity(
    owner,
    "TCP listener root",
  );
  const currentlyDescended = denoProcess !== undefined &&
    isDescendantByParentProcessId(
      owner.processId,
      denoIdentity.processId,
      byPid,
    );
  const previouslyCaptured = previouslyOwned?.get(ownerPid);
  const matchesPreviouslyCaptured = previouslyCaptured !== undefined &&
    matchesWindowsProcessIdentity(owner, previouslyCaptured);
  if (!currentlyDescended && !matchesPreviouslyCaptured) {
    throw new Error(
      `TCP port ${port} owner PID ${ownerPid} is not descended from Deno and has no matching previously-owned identity`,
    );
  }
  return ownerIdentity;
}

async function runPowerShellJson(
  script: string,
  environment?: Record<string, string>,
): Promise<string> {
  const output = await new Deno.Command("powershell.exe", {
    args: ["-NoProfile", "-NonInteractive", "-Command", script],
    env: environment,
    stdout: "piped",
    stderr: "null",
  }).output();
  if (!output.success) {
    throw new Error(
      `PowerShell inspection failed with exit code ${output.code}`,
    );
  }
  return new TextDecoder().decode(output.stdout);
}

function createWindowsProcessControlDeps(): WindowsProcessControlDeps {
  return {
    async listProcesses() {
      return parseWindowsProcessSnapshot(
        await runPowerShellJson(WINDOWS_PROCESS_SNAPSHOT_SCRIPT),
      );
    },
    async listListeners(port) {
      return parseWindowsListenerSnapshot(
        await runPowerShellJson(WINDOWS_LISTENER_SNAPSHOT_SCRIPT, {
          FLOORP_COLOCATED_LOCAL_PORT: String(port),
        }),
      );
    },
    async taskkill(processId, includeTree) {
      const args = ["/PID", String(processId)];
      if (includeTree) {
        args.push("/T");
      }
      args.push("/F");
      try {
        const output = await new Deno.Command("taskkill.exe", {
          args,
          stdout: "null",
          stderr: "null",
        }).output();
        return { success: output.success, code: output.code };
      } catch {
        return { success: false, code: -1 };
      }
    },
    isPortReachable: _isTcpPortReachable,
    sleep,
  };
}

async function captureWindowsDenoIdentityWithRetry(
  processId: number,
  deps: WindowsProcessControlDeps,
): Promise<WindowsProcessIdentity> {
  let lastError: unknown;
  for (
    let attempt = 0;
    attempt < WINDOWS_DENO_IDENTITY_RETRY_COUNT;
    attempt++
  ) {
    try {
      return selectWindowsDenoIdentity(
        await deps.listProcesses(),
        processId,
        Deno.execPath(),
      );
    } catch (error) {
      lastError = error;
      if (attempt + 1 < WINDOWS_DENO_IDENTITY_RETRY_COUNT) {
        await deps.sleep(WINDOWS_DENO_IDENTITY_RETRY_DELAY_MS);
      }
    }
  }
  throw new Error(
    `Could not capture the auto-started Deno identity: ${
      errorToMessage(lastError)
    }`,
  );
}

async function refreshWindowsOwnershipOrThrow(
  state: WindowsAutoStartState,
  deps: WindowsProcessControlDeps,
): Promise<void> {
  try {
    const issues = reconcileWindowsFloorpOwnership(
      state,
      await deps.listProcesses(),
    );
    if (issues.length > 0 || state.ambiguousFloorp.length > 0) {
      throw new Error(
        [...issues, ...state.ambiguousFloorp].join(" | "),
      );
    }
  } catch (error) {
    state.treeKillSafe = false;
    throw error;
  }
}

export async function captureWindowsReadyBrowser(
  state: WindowsAutoStartState,
  port: number,
  deps: WindowsProcessControlDeps,
): Promise<void> {
  state.port = port;
  const previouslyOwned = new Map(state.ownedFloorp);
  try {
    const processes = await deps.listProcesses();
    const listeners = await deps.listListeners(port);
    const listenerRoot = selectWindowsListenerRoot(
      port,
      listeners,
      processes,
      state.deno,
      state.floorpExecutablePath,
      previouslyOwned,
    );
    const promotionIssues = promoteWindowsReadyBrowserOwnership(
      state,
      processes,
      listenerRoot,
      previouslyOwned,
    );
    const preListenerIssues = reconcileWindowsFloorpOwnership(
      state,
      processes,
    );
    state.listenerRoot = listenerRoot;
    const issues = [
      ...promotionIssues,
      ...preListenerIssues,
      ...reconcileWindowsFloorpOwnership(state, processes),
    ];
    if (issues.length > 0 || state.ambiguousFloorp.length > 0) {
      throw new Error(
        `Windows browser ownership capture failed: ${
          [...issues, ...state.ambiguousFloorp].join(" | ")
        }`,
      );
    }
  } catch (error) {
    state.treeKillSafe = false;
    throw error;
  }
}

function orderedOwnedFloorpIdentities(
  state: WindowsAutoStartState,
): WindowsProcessIdentity[] {
  const identities = Array.from(state.ownedFloorp.values());
  return identities.sort((left, right) => {
    if (left.processId === state.listenerRoot?.processId) {
      return -1;
    }
    if (right.processId === state.listenerRoot?.processId) {
      return 1;
    }
    return left.processId - right.processId;
  });
}

export async function stopWindowsAutoStartedBrowser(
  state: WindowsAutoStartState,
  deps: WindowsProcessControlDeps,
  writeLog?: (level: LogLevel, message: string) => void,
): Promise<void> {
  const errors: string[] = [];
  const recordError = (message: string) => addUniqueMessage(errors, message);
  const readProcesses = async (
    stage: string,
  ): Promise<WindowsProcessRecord[] | null> => {
    try {
      return await deps.listProcesses();
    } catch (error) {
      state.treeKillSafe = false;
      recordError(`${stage}: ${errorToMessage(error)}`);
      return null;
    }
  };

  const initialProcesses = await readProcesses("initial process enumeration");
  if (initialProcesses) {
    try {
      const issues = reconcileWindowsFloorpOwnership(
        state,
        initialProcesses,
      );
      for (const issue of issues) {
        recordError(issue);
      }
      if (issues.length > 0 || state.ambiguousFloorp.length > 0) {
        state.treeKillSafe = false;
      }
    } catch (error) {
      state.treeKillSafe = false;
      recordError(`initial ownership reconciliation: ${errorToMessage(error)}`);
    }
  }

  const denoVerification = await readProcesses(
    `pre-taskkill verification for Deno PID ${state.deno.processId}`,
  );
  if (denoVerification) {
    try {
      const verificationIssues = reconcileWindowsFloorpOwnership(
        state,
        denoVerification,
      );
      for (const issue of verificationIssues) {
        recordError(issue);
      }
      if (
        verificationIssues.length > 0 || state.ambiguousFloorp.length > 0
      ) {
        state.treeKillSafe = false;
      }
      const denoProcess = processIndex(denoVerification).get(
        state.deno.processId,
      );
      if (denoProcess) {
        if (
          matchesWindowsDenoIdentity(denoProcess, state.deno)
        ) {
          try {
            const includeTree = state.treeKillSafe;
            const result = await deps.taskkill(
              state.deno.processId,
              includeTree,
            );
            writeLog?.(
              "INFO",
              `Diagnostic taskkill for auto-started Deno PID ${state.deno.processId}: tree=${includeTree}, success=${result.success}, code=${result.code}`,
            );
          } catch (error) {
            writeLog?.(
              "INFO",
              `Diagnostic taskkill for auto-started Deno PID ${state.deno.processId} threw: ${
                errorToMessage(error)
              }`,
            );
          }
        } else {
          state.treeKillSafe = false;
          recordError(
            `Skipped Deno PID ${state.deno.processId}: captured identity no longer matches`,
          );
        }
      }
    } catch (error) {
      state.treeKillSafe = false;
      recordError(`Deno identity verification: ${errorToMessage(error)}`);
    }
  }

  for (let poll = 0; poll < WINDOWS_TEARDOWN_MAX_POLLS; poll++) {
    const processes = await readProcesses(`teardown poll ${poll + 1}`);
    if (!processes) {
      break;
    }
    try {
      for (const issue of reconcileWindowsFloorpOwnership(state, processes)) {
        recordError(issue);
      }
    } catch (error) {
      state.treeKillSafe = false;
      recordError(`ownership reconciliation: ${errorToMessage(error)}`);
      break;
    }

    const byPid = processIndex(processes);
    const liveOwned = orderedOwnedFloorpIdentities(state).filter((identity) => {
      const current = byPid.get(identity.processId);
      if (!current) {
        return false;
      }
      if (!matchesWindowsProcessIdentity(current, identity)) {
        state.treeKillSafe = false;
        recordError(
          `Skipped Floorp PID ${identity.processId}: captured identity no longer matches`,
        );
        return false;
      }
      return true;
    });
    if (liveOwned.length === 0) {
      break;
    }

    for (const identity of liveOwned) {
      const freshProcesses = await readProcesses(
        `pre-taskkill verification for Floorp PID ${identity.processId}`,
      );
      if (!freshProcesses) {
        break;
      }
      let current: WindowsProcessRecord | undefined;
      try {
        current = processIndex(freshProcesses).get(identity.processId);
      } catch (error) {
        state.treeKillSafe = false;
        recordError(
          `pre-taskkill verification for Floorp PID ${identity.processId}: ${
            errorToMessage(error)
          }`,
        );
        break;
      }
      if (!current) {
        continue;
      }
      if (!matchesWindowsProcessIdentity(current, identity)) {
        state.treeKillSafe = false;
        recordError(
          `Skipped Floorp PID ${identity.processId}: identity changed before taskkill`,
        );
        continue;
      }
      try {
        const result = await deps.taskkill(identity.processId, false);
        writeLog?.(
          "INFO",
          `Owned Floorp root-only taskkill PID ${identity.processId}: success=${result.success}, code=${result.code}`,
        );
      } catch (error) {
        writeLog?.(
          "INFO",
          `Owned Floorp root-only taskkill PID ${identity.processId} threw: ${
            errorToMessage(error)
          }`,
        );
      }
    }

    try {
      await deps.sleep(WINDOWS_TEARDOWN_POLL_INTERVAL_MS);
    } catch (error) {
      recordError(`teardown polling delay: ${errorToMessage(error)}`);
      break;
    }
  }

  const finalProcesses = await readProcesses("final process enumeration");
  if (finalProcesses) {
    try {
      for (
        const issue of reconcileWindowsFloorpOwnership(state, finalProcesses)
      ) {
        recordError(issue);
      }
      const finalByPid = processIndex(finalProcesses);
      for (const identity of state.ownedFloorp.values()) {
        const current = finalByPid.get(identity.processId);
        if (current && matchesWindowsProcessIdentity(current, identity)) {
          recordError(
            `Owned Floorp PID ${identity.processId} survived teardown`,
          );
        } else if (current) {
          recordError(
            `Floorp PID ${identity.processId} was reused before final verification`,
          );
        }
      }
      const currentDeno = finalByPid.get(state.deno.processId);
      if (currentDeno && matchesWindowsDenoIdentity(currentDeno, state.deno)) {
        recordError(
          `Auto-started Deno PID ${state.deno.processId} survived teardown`,
        );
      } else if (currentDeno) {
        recordError(
          `Deno PID ${state.deno.processId} was reused before final verification`,
        );
      }
    } catch (error) {
      recordError(`final ownership verification: ${errorToMessage(error)}`);
    }
  }

  for (const ambiguity of state.ambiguousFloorp) {
    recordError(ambiguity);
  }

  if (state.port !== null) {
    try {
      const finalListeners = await deps.listListeners(state.port);
      if (finalListeners.length > 0) {
        recordError(
          `Captured Marionette port ${state.port} still has ${finalListeners.length} listener row(s)`,
        );
      }
    } catch (error) {
      recordError(
        `Could not query listeners for captured Marionette port ${state.port}: ${
          errorToMessage(error)
        }`,
      );
    }

    try {
      if (await deps.isPortReachable(state.port)) {
        recordError(
          `Captured Marionette port ${state.port} remained reachable`,
        );
      }
    } catch (error) {
      recordError(
        `Could not verify captured Marionette port ${state.port}: ${
          errorToMessage(error)
        }`,
      );
    }
  }

  if (errors.length > 0) {
    throw new Error(
      `Windows browser teardown failed closed: ${errors.join(" | ")}`,
    );
  }
}

function startTestBrowserProcess(): Deno.ChildProcess {
  const denoPath = Deno.execPath();
  return new Deno.Command(denoPath, {
    args: ["task", "feles-build", "test"],
    cwd: PROJECT_ROOT,
    stdout: "null",
    stderr: "null",
  }).spawn();
}

async function waitForAutoStartedBrowser(
  child: Deno.ChildProcess,
  startupTimeoutMs: number,
  windowsState?: WindowsAutoStartState,
  windowsDeps?: WindowsProcessControlDeps,
): Promise<void> {
  const deadline = Date.now() + startupTimeoutMs;

  while (Date.now() < deadline) {
    if (windowsState && windowsDeps) {
      await refreshWindowsOwnershipOrThrow(windowsState, windowsDeps);
    }

    const maybeStatus = await Promise.race([
      child.status,
      sleep(AUTOSTART_POLL_INTERVAL_MS).then(() => null),
    ]);

    if (windowsState && windowsDeps) {
      await refreshWindowsOwnershipOrThrow(windowsState, windowsDeps);
    }

    if (maybeStatus) {
      throw new Error(
        `deno task feles-build test exited early (code ${maybeStatus.code})`,
      );
    }

    if (await hasRunningTestBrowser()) {
      return;
    }
  }

  throw new Error(
    `Timed out after ${startupTimeoutMs}ms while waiting for test browser startup`,
  );
}

async function listDescendantPids(rootPid: number): Promise<number[]> {
  const output = await new Deno.Command("ps", {
    args: ["-axo", "pid=,ppid="],
    stdout: "piped",
    stderr: "null",
  }).output();

  if (!output.success) {
    return [];
  }

  const table = new Map<number, number[]>();
  const rows = new TextDecoder().decode(output.stdout).split("\n");

  for (const row of rows) {
    const trimmed = row.trim();
    if (!trimmed) {
      continue;
    }

    const [pidRaw, ppidRaw] = trimmed.split(/\s+/, 2);
    const pid = Number(pidRaw);
    const ppid = Number(ppidRaw);
    if (!Number.isFinite(pid) || !Number.isFinite(ppid)) {
      continue;
    }

    const children = table.get(ppid) ?? [];
    children.push(pid);
    table.set(ppid, children);
  }

  const descendants: number[] = [];
  const queue: number[] = [rootPid];
  const seen = new Set<number>(queue);

  while (queue.length > 0) {
    const parent = queue.shift();
    if (parent === undefined) {
      continue;
    }

    const children = table.get(parent) ?? [];
    for (const child of children) {
      if (seen.has(child)) {
        continue;
      }
      seen.add(child);
      descendants.push(child);
      queue.push(child);
    }
  }

  return descendants;
}

async function listRunningPids(pids: number[]): Promise<Set<number>> {
  const uniquePids = Array.from(new Set(pids));
  if (uniquePids.length === 0) {
    return new Set<number>();
  }

  const output = await new Deno.Command("ps", {
    args: ["-p", uniquePids.join(","), "-o", "pid="],
    stdout: "piped",
    stderr: "null",
  }).output();

  if (!output.success) {
    return new Set<number>();
  }

  const running = new Set<number>();
  const rows = new TextDecoder().decode(output.stdout).split("\n");
  for (const row of rows) {
    const pid = Number(row.trim());
    if (Number.isFinite(pid)) {
      running.add(pid);
    }
  }
  return running;
}

function signalPids(pids: number[], signal: Deno.Signal): void {
  for (const pid of pids) {
    try {
      Deno.kill(pid, signal);
    } catch {
      // process may have already exited
    }
  }
}

function sanitizeKillTargets(pids: number[]): number[] {
  const protectedPids = new Set<number>([Deno.pid, Deno.ppid]);
  return Array.from(
    new Set(
      pids.filter(
        (pid) => Number.isInteger(pid) && pid > 0 && !protectedPids.has(pid),
      ),
    ),
  );
}

async function stopPosixProcessTree(
  child: Deno.ChildProcess,
  rootPid: number,
  writeLog?: (level: LogLevel, message: string) => void,
): Promise<void> {
  writeLog?.(
    "INFO",
    `Stopping auto-started browser process tree (posix, root pid=${rootPid})`,
  );

  let descendants: number[] = [];
  try {
    descendants = await listDescendantPids(rootPid);
    writeLog?.(
      "INFO",
      `Discovered ${descendants.length} descendant process(es) for teardown`,
    );
  } catch {
    // fallback to root process only
    writeLog?.(
      "INFO",
      "Could not enumerate descendants; falling back to root process only",
    );
  }

  const initialTargets = sanitizeKillTargets([...descendants, rootPid]);
  if (initialTargets.length === 0) {
    writeLog?.("INFO", "No valid teardown targets after sanitization");
    return;
  }
  writeLog?.(
    "INFO",
    `Sending SIGTERM to ${initialTargets.length} process(es): ${
      initialTargets.join(", ")
    }`,
  );
  signalPids(initialTargets, "SIGTERM");

  await Promise.race([
    child.status,
    sleep(AUTOSTART_STOP_TIMEOUT_MS).then(() => null),
  ]);

  let remaining = new Set<number>();
  try {
    remaining = await listRunningPids(initialTargets);
  } catch {
    // if process table is unavailable, do not hard-fail teardown
    writeLog?.(
      "INFO",
      "Could not verify remaining processes after SIGTERM; skipping SIGKILL phase",
    );
    return;
  }

  if (remaining.size === 0) {
    writeLog?.("INFO", "All auto-started processes exited after SIGTERM");
    return;
  }

  const killTargets = sanitizeKillTargets(Array.from(remaining));
  writeLog?.(
    "INFO",
    `Sending SIGKILL to ${killTargets.length} remaining process(es): ${
      killTargets.join(", ")
    }`,
  );
  signalPids(killTargets, "SIGKILL");
}

export interface WindowsSpawnedRootControl {
  processId: number;
  killRoot(): void;
  status: Promise<unknown>;
}

export async function stopWindowsSpawnedChildRootOnly(
  control: WindowsSpawnedRootControl,
  sleepFn: (ms: number) => Promise<void> = sleep,
): Promise<void> {
  try {
    control.killRoot();
  } catch {
    // The root may already have exited; status remains the authoritative proof.
  }

  const outcome = await Promise.race([
    control.status.then(() => "exited" as const),
    sleepFn(AUTOSTART_STOP_TIMEOUT_MS).then(() => "timeout" as const),
  ]);
  if (outcome === "timeout") {
    throw new Error(
      `Timed out while stopping spawned Deno root PID ${control.processId}`,
    );
  }
}

export async function stopWindowsAutoStartedBrowserWithRootFallback(
  state: WindowsAutoStartState,
  deps: WindowsProcessControlDeps,
  rootControl: WindowsSpawnedRootControl,
  writeLog?: (level: LogLevel, message: string) => void,
  sleepFn: (ms: number) => Promise<void> = sleep,
): Promise<void> {
  try {
    await stopWindowsAutoStartedBrowser(state, deps, writeLog);
    return;
  } catch (teardownError) {
    const teardownMessage = errorToMessage(teardownError);
    writeLog?.(
      "INFO",
      `Verified Windows teardown failed; stopping only the spawned Deno root handle: ${teardownMessage}`,
    );
    try {
      await stopWindowsSpawnedChildRootOnly(rootControl, sleepFn);
    } catch (fallbackError) {
      throw new Error(
        `${teardownMessage}; spawned Deno root-only fallback failed: ${
          errorToMessage(fallbackError)
        }`,
      );
    }
    throw new Error(
      `${teardownMessage}; spawned Deno root-only fallback completed`,
    );
  }
}

async function stopAutoStartedBrowser(
  child: Deno.ChildProcess,
  windowsState: WindowsAutoStartState | null,
  windowsDeps: WindowsProcessControlDeps | null,
  writeLog?: (level: LogLevel, message: string) => void,
): Promise<void> {
  if (child.pid === undefined) {
    throw new Error(
      "Auto-started browser PID is unavailable; teardown cannot be verified",
    );
  }

  if (Deno.build.os === "windows") {
    const invalidStateReason = !windowsState || !windowsDeps
      ? "Windows teardown has no verified auto-start ownership state"
      : windowsState.deno.processId !== child.pid
      ? "Windows teardown state does not match the spawned Deno child PID"
      : null;
    if (invalidStateReason) {
      writeLog?.(
        "INFO",
        `${invalidStateReason}; stopping only the spawned Deno root handle`,
      );
      let rootStopError: unknown;
      try {
        await stopWindowsSpawnedChildRootOnly({
          processId: child.pid,
          killRoot: () => child.kill("SIGKILL"),
          status: child.status,
        });
      } catch (error) {
        rootStopError = error;
      }
      throw new Error(
        `${invalidStateReason}; root-only stop ${
          rootStopError
            ? `failed: ${errorToMessage(rootStopError)}`
            : "completed"
        }`,
      );
    }
    if (!windowsState || !windowsDeps) {
      throw new Error("Windows teardown state narrowing failed");
    }
    writeLog?.(
      "INFO",
      `Stopping verified auto-started browser processes (windows, deno pid=${child.pid})`,
    );
    await stopWindowsAutoStartedBrowserWithRootFallback(
      windowsState,
      windowsDeps,
      {
        processId: child.pid,
        killRoot: () => child.kill("SIGKILL"),
        status: child.status,
      },
      writeLog,
    );
    return;
  }

  await stopPosixProcessTree(child, child.pid, writeLog);
}

function formatScope(options: RunnerOptions): string {
  const scope: string[] = [];
  if (options.near) {
    scope.push(`near: ${options.near}`);
  }
  if (options.layer !== "all") {
    scope.push(`layer: ${options.layer}`);
  }
  return scope.length > 0 ? ` (${scope.join(", ")})` : "";
}

function formatTimestampForFile(date: Date): string {
  const pad = (value: number) => String(value).padStart(2, "0");
  const milli = String(date.getMilliseconds()).padStart(3, "0");
  return [
    date.getFullYear(),
    pad(date.getMonth() + 1),
    pad(date.getDate()),
    "-",
    pad(date.getHours()),
    pad(date.getMinutes()),
    pad(date.getSeconds()),
    "-",
    milli,
  ].join("");
}

function relPath(absFile: string): string {
  return normalizeSlashes(path.relative(PROJECT_ROOT, absFile));
}

function filterBrowserResults(
  results: BrowserTestResult[],
  targetRels: string[],
): BrowserTestResult[] {
  return results.filter((result) =>
    targetRels.some((targetRel) => isResultMatchTarget(result.file, targetRel))
  );
}

function findMissingTargets(
  results: BrowserTestResult[],
  targetRels: string[],
): string[] {
  return targetRels.filter(
    (targetRel) =>
      !results.some((result) => isResultMatchTarget(result.file, targetRel)),
  );
}

function findUnexpectedBrowserTargets(
  browserCollection: BrowserTestCollection,
  targetRels: string[],
): string[] {
  const normalizedBrowserTargets = Array.from(
    new Set(browserCollection.discoveredFiles.map(normalizeBrowserResultPath)),
  ).sort((a, b) => a.localeCompare(b));

  return normalizedBrowserTargets.filter(
    (browserTarget) =>
      !targetRels.some((targetRel) =>
        isResultMatchTarget(browserTarget, targetRel)
      ),
  );
}

function findUnknownAliasResults(results: BrowserTestResult[]): string[] {
  return Array.from(
    new Set(
      results
        .map((result) => normalizeBrowserResultPath(result.file))
        .filter((normalizedPath) =>
          normalizedPath.startsWith("[unknown-alias] ")
        ),
    ),
  ).sort((a, b) => a.localeCompare(b));
}

function writeRunLog(logFilePath: string, lines: string[]): void {
  Deno.mkdirSync(path.dirname(logFilePath), { recursive: true });
  const body = `${lines.join("\n")}\n`;
  Deno.writeTextFileSync(logFilePath, body);
  Deno.writeTextFileSync(path.join(TEST_LOG_DIR, "latest.log"), body);
}

function createRunId(): string {
  return crypto.randomUUID();
}

export function browserFilterTargetsForRun(
  targetRels: string[],
  scopedRun: boolean,
): string[] {
  return scopedRun ? targetRels : [];
}

export function replaceStringPref(
  content: string,
  prefName: string,
  value: string,
): string {
  const prefPattern = new RegExp(
    `^user_pref\\("${escapeRegExp(prefName)}",\\s*".*"\\);\\r?\\n?`,
    "m",
  );
  const withoutOldPref = content.replace(prefPattern, "");
  const prefix = withoutOldPref.length > 0 && !withoutOldPref.endsWith("\n")
    ? `${withoutOldPref}\n`
    : withoutOldPref;
  return `${prefix}user_pref("${prefName}", ${JSON.stringify(value)});\n`;
}

export interface BrowserTestControl {
  schemaVersion: typeof TEST_CONTROL_SCHEMA_VERSION;
  runId: string;
  expiresAtMs: number;
  filter: string[];
}

export function writeBrowserTestControlPrefs(
  targetRels: string[],
  runId: string,
  expiresAtMs: number,
  profileDir = PATHS.profile_test,
): void {
  const prefsPath = path.join(profileDir, "prefs.js");
  Deno.mkdirSync(path.dirname(prefsPath), { recursive: true });

  let content = "";
  try {
    content = Deno.readTextFileSync(prefsPath);
  } catch {
    // A fresh test profile may not have prefs.js yet.
  }

  const payload = JSON.stringify(targetRels);
  const withFilter = replaceStringPref(content, TEST_FILTER_PREF, payload);
  const withFilterCount = replaceStringPref(
    withFilter,
    TEST_FILTER_COUNT_PREF,
    String(targetRels.length),
  );
  const withFilterItems = targetRels.reduce(
    (currentContent, targetRel, index) =>
      replaceStringPref(
        currentContent,
        `${TEST_FILTER_ITEM_PREF_PREFIX}${index}`,
        targetRel,
      ),
    withFilterCount,
  );
  const withRunId = replaceStringPref(withFilterItems, TEST_RUN_ID_PREF, runId);

  Deno.writeTextFileSync(prefsPath, withRunId);
  const control: BrowserTestControl = {
    schemaVersion: TEST_CONTROL_SCHEMA_VERSION,
    runId,
    expiresAtMs,
    filter: targetRels,
  };
  Deno.writeTextFileSync(
    path.join(profileDir, TEST_CONTROL_FILE),
    `${JSON.stringify(control)}\n`,
  );
}

function readStringPref(content: string, prefName: string): string | undefined {
  const match = content.match(
    new RegExp(
      `^user_pref\\("${
        escapeRegExp(prefName)
      }",\\s*("(?:[^"\\\\]|\\\\.)*")\\);\\r?$`,
      "m",
    ),
  );
  if (!match) return undefined;
  try {
    const value: unknown = JSON.parse(match[1]);
    return typeof value === "string" ? value : undefined;
  } catch {
    return undefined;
  }
}

function removeOwnedControlPrefs(content: string): string {
  return content
    .split(/(?<=\n)/)
    .filter((line) => {
      const match = line.match(/^user_pref\("([^"]+)",/);
      if (!match) return true;
      const name = match[1];
      return name !== TEST_FILTER_PREF &&
        name !== TEST_FILTER_COUNT_PREF &&
        name !== TEST_RUN_ID_PREF &&
        !new RegExp(
          `^${escapeRegExp(TEST_FILTER_ITEM_PREF_PREFIX)}\\d+$`,
        ).test(name);
    })
    .join("");
}

export function clearBrowserTestControlPrefs(
  runId: string,
  profileDir = PATHS.profile_test,
): void {
  const errors: string[] = [];
  const prefsPath = path.join(profileDir, "prefs.js");
  try {
    const content = Deno.readTextFileSync(prefsPath);
    if (readStringPref(content, TEST_RUN_ID_PREF) === runId) {
      Deno.writeTextFileSync(prefsPath, removeOwnedControlPrefs(content));
    }
  } catch (error) {
    if (!(error instanceof Deno.errors.NotFound)) {
      errors.push(`prefs cleanup: ${errorToMessage(error)}`);
    }
  }

  const controlPath = path.join(profileDir, TEST_CONTROL_FILE);
  try {
    const parsed: unknown = JSON.parse(Deno.readTextFileSync(controlPath));
    if (
      typeof parsed === "object" && parsed !== null &&
      "runId" in parsed && parsed.runId === runId
    ) {
      Deno.removeSync(controlPath);
    }
  } catch (error) {
    if (
      !(error instanceof Deno.errors.NotFound) &&
      !(error instanceof SyntaxError)
    ) {
      errors.push(`control-file cleanup: ${errorToMessage(error)}`);
    }
  }
  if (errors.length > 0) {
    throw new Error(
      `Browser test control cleanup was incomplete. ${errors.join(" | ")}`,
    );
  }
}

async function main(): Promise<number> {
  const startedAt = new Date();
  const logLines: string[] = [];
  const logFilePath = path.join(
    TEST_LOG_DIR,
    `colocated-${formatTimestampForFile(startedAt)}.log`,
  );
  let exitCode = 0;
  let autoStartedBrowser: Deno.ChildProcess | null = null;
  let windowsAutoStartState: WindowsAutoStartState | null = null;
  let windowsProcessDeps: WindowsProcessControlDeps | null = null;
  let ownedRunId: string | undefined;

  const writeLine = (level: LogLevel, message: string) => {
    const timestamp = new Date().toISOString();
    logLines.push(`[${timestamp}] [${level}] ${message}`);
    const rendered = formatConsoleLine(level, message);
    if (level === "ERROR") {
      console.error(rendered);
    } else {
      console.log(rendered);
    }
  };

  const writeSection = (title: string) => {
    writeLine("INFO", "");
    writeLine("INFO", `=== ${title} ===`);
  };

  logLines.push(`[META] startedAt=${startedAt.toISOString()}`);
  logLines.push(`[META] args=${JSON.stringify(Deno.args)}`);

  try {
    let options: RunnerOptions;
    try {
      options = parseOptions(Deno.args);
    } catch (error) {
      writeLine("ERROR", errorToMessage(error));
      exitCode = 1;
      return exitCode;
    }

    if (options.help) {
      console.log(HELP);
      return exitCode;
    }

    const runDeadlineMs = Date.now() + MAX_TEST_EXECUTION_TIMEOUT_MS;
    const resolveStageTimeoutMs = (
      requestedMs: number,
      stageName: string,
    ): number => {
      const remainingMs = runDeadlineMs - Date.now();
      if (remainingMs <= 0) {
        throw new Error(
          `Overall test execution exceeded ${MAX_TEST_EXECUTION_TIMEOUT_MS}ms before ${stageName}.`,
        );
      }
      return Math.min(requestedMs, remainingMs);
    };

    writeLine(
      "INFO",
      `Running browser integration tests (layer=${options.layer}${
        options.near ? `, near=${options.near}` : ""
      })`,
    );
    writeLine(
      "INFO",
      `Configured timeouts: collection=${options.timeoutMs}ms, startup=${options.startupTimeoutMs}ms, overall-cap=${MAX_TEST_EXECUTION_TIMEOUT_MS}ms`,
    );
    writeSection("Discovered Test Targets");

    let testFiles = discoverBrowserTests();
    testFiles = filterByLayer(testFiles, options.layer);
    if (options.near) {
      testFiles = filterNear(testFiles, options.near);
    }

    if (testFiles.length === 0) {
      const scope = formatScope(options);
      writeLine("INFO", `No browser test files matched${scope}.`);
      return exitCode;
    }

    const targetRels = testFiles.map(relPath);

    writeLine("INFO", `Found ${targetRels.length} browser test file(s)`);
    for (const rel of targetRels) {
      writeLine("INFO", `  - ${rel}`);
    }

    if (options.listOnly) {
      return exitCode;
    }

    writeSection("Browser Connection");
    writeLine("INFO", "Waiting for browser test results via prefs file...");

    const runningBeforeConnect = await hasRunningTestBrowser();
    const scopedRun = options.near !== undefined || options.layer !== "all";
    if (runningBeforeConnect && scopedRun) {
      writeLine(
        "ERROR",
        "A running test browser was detected. Scoped runs require nora.tests.filter and nora.tests.run_id to be written before browser startup.",
      );
      writeLine(
        "ERROR",
        "Stop the running test browser and rerun this command, or start the collector with --no-autostart before launching the browser.",
      );
      exitCode = 1;
      return exitCode;
    }

    let runId: string | undefined;
    if (!runningBeforeConnect) {
      runId = createRunId();
      ownedRunId = runId;
      writeBrowserTestControlPrefs(
        browserFilterTargetsForRun(targetRels, scopedRun),
        runId,
        runDeadlineMs,
      );
    } else {
      writeLine(
        "INFO",
        "Running test browser detected; browser-side target filter can only apply when set before browser startup.",
      );
    }

    if (options.autoStart && !runningBeforeConnect) {
      writeLine(
        "INFO",
        "No running test browser detected. Starting `deno task feles-build test` automatically...",
      );

      try {
        if (Deno.build.os === "windows") {
          windowsProcessDeps = createWindowsProcessControlDeps();
          assertWindowsAutoStartPreflight(
            await windowsProcessDeps.listProcesses(),
          );
        }
        autoStartedBrowser = startTestBrowserProcess();
        if (Deno.build.os === "windows") {
          if (!windowsProcessDeps || autoStartedBrowser.pid === undefined) {
            throw new Error(
              "Windows auto-start did not provide the required process-control state",
            );
          }
          windowsAutoStartState = createWindowsAutoStartState(
            await captureWindowsDenoIdentityWithRetry(
              autoStartedBrowser.pid,
              windowsProcessDeps,
            ),
          );
        }
        await waitForAutoStartedBrowser(
          autoStartedBrowser,
          resolveStageTimeoutMs(options.startupTimeoutMs, "browser startup"),
          windowsAutoStartState ?? undefined,
          windowsProcessDeps ?? undefined,
        );
        if (Deno.build.os === "windows") {
          const marionettePort = readMarionettePortFromFile();
          if (
            !windowsAutoStartState || !windowsProcessDeps ||
            marionettePort === null || marionettePort < 1 ||
            marionettePort > 65_535
          ) {
            throw new Error(
              "Windows auto-start could not capture a valid Marionette port",
            );
          }
          await captureWindowsReadyBrowser(
            windowsAutoStartState,
            marionettePort,
            windowsProcessDeps,
          );
        }
        writeLine(
          "INFO",
          "Auto-started test browser is ready. Waiting for test results...",
        );
      } catch (autoStartError) {
        writeLine(
          "ERROR",
          `Failed to auto-start browser: ${errorToMessage(autoStartError)}`,
        );
        exitCode = 1;
        return exitCode;
      }
    }

    writeSection("Test Execution");
    writeLine("INFO", "Collecting browser test results from prefs file...");

    try {
      const browserCollection = await collectBrowserTestResultsFromPrefs(
        resolveStageTimeoutMs(options.timeoutMs, "browser test collection"),
        runId,
      );
      const browserResults = browserCollection.results;
      const results = filterBrowserResults(browserResults, targetRels);
      const missingTargets = browserCollection.aborted
        ? []
        : findMissingTargets(results, targetRels);
      const unknownAliasResults = findUnknownAliasResults(browserResults);
      const strictDiscoveryReconciliation = options.layer === "all" &&
        !options.near;
      const unexpectedBrowserTargets = strictDiscoveryReconciliation
        ? findUnexpectedBrowserTargets(browserCollection, targetRels)
        : [];

      writeLine("INFO", "");
      for (let i = 0; i < results.length; i++) {
        const result = results[i];
        const normalizedFile = normalizeBrowserResultPath(result.file);
        writeLine(
          "INFO",
          `[${i + 1}/${results.length}] Completed ${normalizedFile}`,
        );
        if (result.source === "downloaded-firefox" && result.upstreamPath) {
          writeLine(
            "INFO",
            `  Firefox source: ${result.upstreamPath}${
              result.manifestPath ? ` (${result.manifestPath})` : ""
            }`,
          );
        }
        if (result.ok) {
          writeLine(
            "INFO",
            `✓ ${
              normalizeBrowserResultPath(result.file)
            } (${result.mode}, ${result.durationMs}ms)`,
          );
        } else {
          writeLine(
            "ERROR",
            `✗ ${
              normalizeBrowserResultPath(result.file)
            } (${result.durationMs}ms)`,
          );
          writeLine("ERROR", `  ${result.error ?? "Unknown error"}`);
        }
        for (const task of result.tasks ?? []) {
          writeLine(
            task.ok ? "INFO" : "ERROR",
            `  ${
              task.ok ? "✓" : "✗"
            } task ${task.index}: ${task.name} (${task.durationMs}ms)${
              task.error ? ` — ${task.error}` : ""
            }`,
          );
        }
      }

      const skippedTargets = browserCollection.aborted
        ? targetRels.filter((targetRel) =>
          !results.some((result) => isResultMatchTarget(result.file, targetRel))
        )
        : [];
      if (browserCollection.aborted) {
        writeLine("ERROR", "");
        writeLine(
          "ERROR",
          browserCollection.abortReason ??
            "Browser-side runner aborted before completing all target tests.",
        );
        writeLine(
          "ERROR",
          `Skipped ${skippedTargets.length} remaining target test file(s).`,
        );
      }

      if (missingTargets.length > 0) {
        writeLine("ERROR", "");
        writeLine(
          "ERROR",
          `Missing browser result(s) for ${missingTargets.length} target test file(s):`,
        );
        for (const missingTarget of missingTargets) {
          writeLine("ERROR", `- ${missingTarget}`);
        }
      }

      if (unexpectedBrowserTargets.length > 0) {
        writeLine("ERROR", "");
        writeLine(
          "ERROR",
          `Browser discovered ${unexpectedBrowserTargets.length} unexpected test file(s) that are not in host discovery:`,
        );
        for (const unexpectedTarget of unexpectedBrowserTargets) {
          writeLine("ERROR", `- ${unexpectedTarget}`);
        }
      }

      if (unknownAliasResults.length > 0) {
        writeLine("ERROR", "");
        writeLine(
          "ERROR",
          `Browser results include ${unknownAliasResults.length} unresolved alias path(s):`,
        );
        for (const unresolvedPath of unknownAliasResults) {
          writeLine("ERROR", `- ${unresolvedPath}`);
        }
      }

      const passed = results.filter((r) => r.ok).length;
      const abortFailure = browserCollection.aborted &&
          !results.some((r) => r.timedOut)
        ? 1
        : 0;
      const failed = results.length -
        passed +
        missingTargets.length +
        unexpectedBrowserTargets.length +
        unknownAliasResults.length +
        abortFailure;
      const skipped = skippedTargets.length;
      const downloadedResults = results.filter((result) =>
        result.source === "downloaded-firefox"
      );
      const downloadedTasks = downloadedResults.flatMap((result) =>
        result.tasks ?? []
      );

      writeSection("Summary");
      writeLine(
        "INFO",
        `Browser test result: ${passed} passed, ${failed} failed${
          skipped > 0 ? `, ${skipped} skipped` : ""
        }`,
      );
      if (downloadedResults.length > 0) {
        const passedDownloadedTasks = downloadedTasks.filter((task) => task.ok)
          .length;
        writeLine(
          "INFO",
          `Downloaded Firefox coverage: ${downloadedResults.length} file(s), ${downloadedTasks.length} task(s), ${passedDownloadedTasks} passed`,
        );
      }
      if (failed > 0) {
        exitCode = 1;
      }
    } catch (error) {
      writeLine(
        "ERROR",
        `Browser test collection failed: ${errorToMessage(error)}`,
      );
      writeLine(
        "ERROR",
        `Hint: increase timeout with --timeout-ms <ms> (current: ${options.timeoutMs}) or ensure browser startup is stable.`,
      );
      exitCode = 1;
    }
  } catch (error) {
    writeLine("ERROR", `Unexpected error: ${errorToMessage(error)}`);
    exitCode = 1;
  } finally {
    if (autoStartedBrowser) {
      try {
        await stopAutoStartedBrowser(
          autoStartedBrowser,
          windowsAutoStartState,
          windowsProcessDeps,
          writeLine,
        );
      } catch (error) {
        writeLine(
          "ERROR",
          `Failed to stop auto-started browser safely: ${
            errorToMessage(error)
          }`,
        );
        exitCode = 1;
      }
    }

    if (ownedRunId) {
      try {
        clearBrowserTestControlPrefs(ownedRunId);
      } catch (error) {
        writeLine(
          "ERROR",
          `Failed to clear browser test control state: ${
            errorToMessage(error)
          }`,
        );
        exitCode = 1;
      }
    }

    const finishedAt = new Date();
    logLines.push(`[META] finishedAt=${finishedAt.toISOString()}`);
    logLines.push(
      `[META] durationMs=${finishedAt.getTime() - startedAt.getTime()}`,
    );
    logLines.push(`[META] exitCode=${exitCode}`);

    try {
      writeRunLog(logFilePath, logLines);
      const relLogPath = path
        .relative(PROJECT_ROOT, logFilePath)
        .replaceAll("\\", "/");
      if (exitCode === 0) {
        console.log(`Log written to ${relLogPath}`);
      } else {
        console.error(`Log written to ${relLogPath}`);
      }
    } catch (error) {
      console.error(`Failed to write log file: ${errorToMessage(error)}`);
      exitCode = 1;
    }
  }

  return exitCode;
}

if (import.meta.main) {
  const exitCode = await main();
  Deno.exit(exitCode);
}
