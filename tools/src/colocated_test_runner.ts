// SPDX-License-Identifier: MPL-2.0

import { parseArgs } from "@std/cli";
import { walkSync } from "@std/fs";
import * as path from "@std/path";
import { PROJECT_ROOT } from "./defines.ts";
import {
  detectLayer,
  escapeRegExp,
  isTestFile,
  isResultMatchTarget,
  normalizeBrowserResultPath,
  normalizeSlashes,
  parseLayer,
  type TestLayer,
} from "./colocated_test_utils.ts";
import { sleep } from "./async_utils.ts";
import { MarionetteClient } from "./browser_connector.ts";
import {
  collectBrowserTestResults,
  type BrowserTestCollection,
  type BrowserTestResult,
} from "./browser_test_collector.ts";

interface RunnerOptions {
  near?: string;
  listOnly: boolean;
  layer: TestLayer;
  autoStart: boolean;
}

const TEST_LOG_DIR = path.join(PROJECT_ROOT, "logs", "test");
const MARIONETTE_PORT_FILE = path.join(
  PROJECT_ROOT,
  "_dist",
  "marionette-port.txt",
);
const AUTOSTART_READY_TIMEOUT_MS = 180_000;
const AUTOSTART_POLL_INTERVAL_MS = 1_000;
const AUTOSTART_STOP_TIMEOUT_MS = 5_000;

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

  const levelLabel =
    level === "ERROR" ? paint("ERROR", "1;31") : paint("INFO", "1;36");

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

  for (const entry of walkSync(PROJECT_ROOT, {
    includeDirs: false,
    followSymlinks: false,
    skip: WALK_SKIP_PATTERNS,
  })) {
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
    /^browser-features\/chrome\/(?:.*\/)?test\/.*\.test\.(?:ts|mts|tsx|js|mjs|jsx)$/.test(
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

function parseOptions(args: string[]): RunnerOptions {
  const parsed = parseArgs(args, {
    string: ["near", "env", "layer"],
    boolean: ["list", "no-autostart"],
    alias: {
      n: "near",
      l: "list",
    },
    default: {
      list: false,
      layer: "all",
    },
  });

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

async function isTcpPortReachable(
  port: number,
  timeoutMs = 1_500,
): Promise<boolean> {
  let timedOut = false;

  const connectPromise = Deno.connect({ hostname: "127.0.0.1", port })
    .then((conn) => {
      try {
        conn.close();
      } catch {
        // best-effort close
      }
      return !timedOut;
    })
    .catch(() => false);

  const timeoutPromise = sleep(timeoutMs).then(() => {
    timedOut = true;
    return false;
  });

  return await Promise.race([connectPromise, timeoutPromise]);
}

async function hasRunningTestBrowser(): Promise<boolean> {
  const port = readMarionettePortFromFile();
  if (!port) {
    return false;
  }

  return await isTcpPortReachable(port);
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
): Promise<void> {
  const deadline = Date.now() + AUTOSTART_READY_TIMEOUT_MS;

  while (Date.now() < deadline) {
    const maybeStatus = await Promise.race([
      child.status,
      sleep(AUTOSTART_POLL_INTERVAL_MS).then(() => null),
    ]);

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
    `Timed out after ${AUTOSTART_READY_TIMEOUT_MS}ms while waiting for test browser startup`,
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
      pids.filter((pid) => Number.isInteger(pid) && pid > 0 && !protectedPids.has(pid)),
    ),
  );
}

async function stopPosixProcessTree(
  child: Deno.ChildProcess,
  rootPid: number,
): Promise<void> {
  let descendants: number[] = [];
  try {
    descendants = await listDescendantPids(rootPid);
  } catch {
    // fallback to root process only
  }

  const initialTargets = sanitizeKillTargets([...descendants, rootPid]);
  if (initialTargets.length === 0) {
    return;
  }
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
    return;
  }

  if (remaining.size === 0) {
    return;
  }

  signalPids(sanitizeKillTargets(Array.from(remaining)), "SIGKILL");
}

async function stopAutoStartedBrowser(child: Deno.ChildProcess): Promise<void> {
  if (child.pid === undefined) {
    return;
  }

  if (Deno.build.os === "windows") {
    try {
      await new Deno.Command("taskkill", {
        args: ["/PID", String(child.pid), "/T", "/F"],
        stdout: "null",
        stderr: "null",
      }).output();
    } catch {
      // process may have exited already
    }
    return;
  }

  await stopPosixProcessTree(child, child.pid);
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
    targetRels.some((targetRel) => isResultMatchTarget(result.file, targetRel)),
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
        isResultMatchTarget(browserTarget, targetRel),
      ),
  );
}

function findUnknownAliasResults(results: BrowserTestResult[]): string[] {
  return Array.from(
    new Set(
      results
        .map((result) => normalizeBrowserResultPath(result.file))
        .filter((normalizedPath) =>
          normalizedPath.startsWith("[unknown-alias] "),
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

async function main(): Promise<number> {
  const startedAt = new Date();
  const logLines: string[] = [];
  const logFilePath = path.join(
    TEST_LOG_DIR,
    `colocated-${formatTimestampForFile(startedAt)}.log`,
  );
  let exitCode = 0;
  let autoStartedBrowser: Deno.ChildProcess | null = null;

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

    writeLine(
      "INFO",
      `Running browser integration tests (layer=${options.layer}${
        options.near ? `, near=${options.near}` : ""
      })`,
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
    writeLine("INFO", "Connecting to browser via Marionette...");

    const runningBeforeConnect = await hasRunningTestBrowser();
    if (options.autoStart && !runningBeforeConnect) {
      writeLine(
        "INFO",
        "No running test browser detected. Starting `deno task feles-build test` automatically...",
      );

      try {
        autoStartedBrowser = startTestBrowserProcess();
        await waitForAutoStartedBrowser(autoStartedBrowser);
        writeLine(
          "INFO",
          "Auto-started test browser is ready. Reconnecting...",
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

    let client: MarionetteClient;
    try {
      client = await MarionetteClient.connect();
    } catch (initialConnectError) {
      const running = await hasRunningTestBrowser();

      if (!options.autoStart || running || autoStartedBrowser) {
        writeLine(
          "ERROR",
          "Could not connect to browser. Ensure the browser is running in test mode:\n" +
            "  deno task feles-build test\n" +
            "Or start it manually:\n" +
            "  deno task dev-tool start",
        );
        writeLine(
          "ERROR",
          `Connect error: ${errorToMessage(initialConnectError)}`,
        );
        exitCode = 1;
        return exitCode;
      }

      writeLine(
        "INFO",
        "No reachable test browser detected after connection failure. Starting `deno task feles-build test` automatically...",
      );

      try {
        autoStartedBrowser = startTestBrowserProcess();
        await waitForAutoStartedBrowser(autoStartedBrowser);
        writeLine(
          "INFO",
          "Auto-started test browser is ready. Reconnecting...",
        );
        client = await MarionetteClient.connect();
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
    writeLine("INFO", "Connected. Waiting for browser test results...");

    try {
      const browserCollection = await collectBrowserTestResults(client);
      const browserResults = browserCollection.results;
      const results = filterBrowserResults(browserResults, targetRels);
      const missingTargets = findMissingTargets(results, targetRels);
      const unknownAliasResults = findUnknownAliasResults(browserResults);
      const strictDiscoveryReconciliation =
        options.layer === "all" && !options.near;
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
        if (result.ok) {
          writeLine(
            "INFO",
            `✓ ${normalizeBrowserResultPath(result.file)} (${result.mode}, ${result.durationMs}ms)`,
          );
        } else {
          writeLine(
            "ERROR",
            `✗ ${normalizeBrowserResultPath(result.file)} (${result.durationMs}ms)`,
          );
          writeLine("ERROR", `  ${result.error ?? "Unknown error"}`);
        }
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
      const failed =
        results.length -
        passed +
        missingTargets.length +
        unexpectedBrowserTargets.length +
        unknownAliasResults.length;

      writeSection("Summary");
      writeLine(
        "INFO",
        `Browser test result: ${passed} passed, ${failed} failed`,
      );
      if (failed > 0) {
        exitCode = 1;
      }
    } catch (error) {
      writeLine(
        "ERROR",
        `Browser test collection failed: ${errorToMessage(error)}`,
      );
      exitCode = 1;
    } finally {
      await client.close();
    }
  } catch (error) {
    writeLine("ERROR", `Unexpected error: ${errorToMessage(error)}`);
    exitCode = 1;
  } finally {
    if (autoStartedBrowser) {
      await stopAutoStartedBrowser(autoStartedBrowser);
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
