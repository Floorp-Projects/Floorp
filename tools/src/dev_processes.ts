// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { basename as posixBasename } from "@std/path/posix";
import { PLATFORM, PROJECT_ROOT } from "./defines.ts";
import { settleWithin } from "./async_utils.ts";
import {
  matchesDevIdentity,
  parseDevProcessRegistry,
  parseUnixDevProcesses,
} from "./dev_process_selection.ts";
import {
  captureWindowsBrowserListenerIdentity,
  listWindowsBrowserListeners,
  listWindowsBrowserProcesses,
} from "./dev_browser_identity.ts";
import type {
  WindowsBrowserIdentityTarget,
  WindowsBrowserListenerIdentity,
} from "./dev_browser_identity_types.ts";
import type {
  DevProcessIdentity,
  DevProcessInfo,
} from "./dev_process_types.ts";

export const DEV_PROCESSES_FILE = path.join(
  PROJECT_ROOT,
  "_dist",
  "dev-processes.json",
);

async function commandOutput(
  cmd: string,
  args: string[],
  input?: string,
  signal?: AbortSignal,
): Promise<string> {
  const command = new Deno.Command(cmd, {
    args,
    stdin: input === undefined ? "null" : "piped",
    stdout: "piped",
    stderr: "piped",
    env: { LC_ALL: "C" },
  });
  let output: Deno.CommandOutput;
  if (signal) {
    signal.throwIfAborted();
    const child = command.spawn();
    const cancelled = Promise.withResolvers<never>();
    const abort = () => {
      try {
        child.kill("SIGKILL");
      } catch { /* Already exited. */ }
      cancelled.reject(signal.reason);
    };
    signal.addEventListener("abort", abort, { once: true });
    if (signal.aborted) abort();
    const pending = Promise.resolve().then(() => child.output());
    try {
      output = await Promise.race([pending, cancelled.promise]);
    } catch (error) {
      abort();
      await settleWithin(pending, 1000);
      throw error;
    } finally {
      signal.removeEventListener("abort", abort);
    }
  } else if (input === undefined) {
    output = await command.output();
  } else {
    const child = command.spawn();
    const writer = child.stdin.getWriter();
    await writer.write(new TextEncoder().encode(input));
    await writer.close();
    output = await child.output();
  }
  signal?.throwIfAborted();
  if (!output.success) {
    throw new Error(
      `${cmd}: ${new TextDecoder().decode(output.stderr).trim()}`,
    );
  }
  return new TextDecoder().decode(output.stdout);
}

export async function listDevProcesses(
  pid?: number,
  signal?: AbortSignal,
  timeoutMs = 5000,
): Promise<DevProcessInfo[]> {
  if (!Number.isFinite(timeoutMs) || timeoutMs <= 0) {
    throw new TypeError("Invalid process inventory timeout");
  }
  const controller = new AbortController();
  const abort = () => controller.abort(signal?.reason);
  signal?.addEventListener("abort", abort, { once: true });
  if (signal?.aborted) abort();
  const timer = setTimeout(
    () => controller.abort(new Error("Process inventory timed out")),
    timeoutMs,
  );
  try {
    return await captureDevProcesses(pid, controller.signal);
  } finally {
    clearTimeout(timer);
    signal?.removeEventListener("abort", abort);
  }
}

export async function devProcessExitConfirmed(
  pid: number,
  inspect = listDevProcesses,
): Promise<boolean> {
  try {
    return (await inspect(pid)).length === 0;
  } catch {
    return false;
  }
}

async function captureDevProcesses(
  pid: number | undefined,
  signal: AbortSignal,
): Promise<DevProcessInfo[]> {
  signal.throwIfAborted();
  if (PLATFORM === "windows") {
    const filter = pid === undefined ? "" : ` -Filter 'ProcessId = ${pid}'`;
    const output = await commandOutput(
      "powershell",
      [
        "-NoProfile",
        "-NonInteractive",
        "-Command",
        "$ErrorActionPreference = 'Stop'; [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new(); " +
        `ConvertTo-Json -InputObject @(Get-CimInstance Win32_Process${filter} | ForEach-Object { ` +
        "@{ pid = [int]$_.ProcessId; ppid = [int]$_.ParentProcessId; name = $_.Name; " +
        "commandLine = [string]$_.CommandLine; startTime = $(if ($_.CreationDate) { $_.CreationDate.ToUniversalTime().ToString('o') } else { '' }) } " +
        "}) -Compress",
      ],
      undefined,
      signal,
    );
    if (!output.trim()) return [];
    const parsed: unknown = JSON.parse(output.replace(/^\uFEFF/, ""));
    if (!Array.isArray(parsed)) throw new Error("Invalid Windows process list");
    return parsed.map((entry: unknown) => {
      if (
        typeof entry !== "object" || entry === null ||
        !("pid" in entry) || typeof entry.pid !== "number" ||
        !("ppid" in entry) || typeof entry.ppid !== "number" ||
        !("name" in entry) || typeof entry.name !== "string" ||
        !("commandLine" in entry) || typeof entry.commandLine !== "string" ||
        !("startTime" in entry) || typeof entry.startTime !== "string"
      ) {
        throw new Error("Invalid Windows process entry");
      }
      return {
        pid: entry.pid,
        ppid: entry.ppid,
        name: entry.name,
        commandLine: entry.commandLine,
        startTime: entry.startTime,
      };
    });
  }
  // An all-process snapshot also handles a requested PID that exited before ps.
  const selector = ["-axww"];
  const output = await commandOutput(
    "ps",
    [
      ...selector,
      "-o",
      "pid=,ppid=,pgid=,lstart=,args=",
    ],
    undefined,
    signal,
  );
  const names = await commandOutput(
    "ps",
    [...selector, "-o", "pid=,comm="],
    undefined,
    signal,
  );
  const byPid = new Map(
    names.split("\n").flatMap((line) => {
      const match = line.match(/^\s*(\d+)\s+(.*)$/);
      return match
        ? [[Number(match[1]), posixBasename(match[2])] as const]
        : [];
    }),
  );
  const processes = parseUnixDevProcesses(output, true).filter((process) =>
    pid === undefined || process.pid === pid
  );
  for (const process of processes) {
    signal.throwIfAborted();
    process.name = byPid.get(process.pid) ?? "";
    if (PLATFORM === "linux") {
      try {
        process.argv =
          (await Deno.readTextFile(`/proc/${process.pid}/cmdline`, { signal }))
            .split("\0").filter(Boolean);
        // Include boot ID and kernel start ticks for sub-second PID identity.
        const stat = await Deno.readTextFile(`/proc/${process.pid}/stat`, {
          signal,
        });
        const fields = stat.slice(stat.lastIndexOf(")") + 2).split(" ");
        const ticks = fields[19];
        // The group id from the same read as the start time.
        process.pgid = Number(fields[2]);
        const boot =
          (await Deno.readTextFile("/proc/sys/kernel/random/boot_id", {
            signal,
          })).trim();
        process.startTime = `${boot}:${ticks}`;
      } catch {
        signal.throwIfAborted();
        // ps identity remains available for inaccessible/exited processes.
      }
    }
  }
  return processes;
}

export function readDevProcesses(
  file = DEV_PROCESSES_FILE,
): DevProcessIdentity[] {
  try {
    return parseDevProcessRegistry(JSON.parse(Deno.readTextFileSync(file)));
  } catch (error) {
    if (!(error instanceof Deno.errors.NotFound)) {
      console.error("[dev-tool] Cannot read process registry:", error);
    }
    return [];
  }
}

/** Merge after capture without an intervening await; keep orphan records. */
export async function recordDevProcess(
  pid: number,
  file = DEV_PROCESSES_FILE,
  expected?: DevProcessIdentity,
  signal?: AbortSignal,
): Promise<boolean> {
  try {
    const process = (await listDevProcesses(pid, signal)).find((entry) =>
      entry.pid === pid
    );
    if (!process?.startTime) {
      throw new Error(`Cannot capture identity for PID ${pid}`);
    }
    const identity: DevProcessIdentity = {
      pid,
      startTime: process.startTime,
      commandLine: process.commandLine,
    };
    // Reused PID detection happens before any registry write.
    if (expected && !matchesDevIdentity(process, expected)) {
      console.error(
        `[dev-tool] PID ${pid} no longer matches the expected browser generation.`,
      );
      return false;
    }
    signal?.throwIfAborted();
    const entries = readDevProcesses(file).filter((entry) => entry.pid !== pid);
    entries.push(identity);
    Deno.mkdirSync(path.dirname(file), { recursive: true });
    const temporary = `${file}.${Deno.pid}.tmp`;
    Deno.writeTextFileSync(
      temporary,
      JSON.stringify({ version: 1, processes: entries }, null, 2) + "\n",
    );
    Deno.renameSync(temporary, file);
    return true;
  } catch (error) {
    signal?.throwIfAborted();
    console.error("[dev-tool] Cannot record dev process:", error);
    return false;
  }
}

/** Register the actual Windows browser listener child before readiness. */
export async function registerWindowsBrowserListenerIdentity(
  target: WindowsBrowserIdentityTarget,
): Promise<WindowsBrowserListenerIdentity> {
  return await captureWindowsBrowserListenerIdentity(target, {
    listListeners: listWindowsBrowserListeners,
    listProcesses: listWindowsBrowserProcesses,
    async recordIdentity(identity) {
      return await recordDevProcess(
        identity.pid,
        DEV_PROCESSES_FILE,
        identity,
      );
    },
    async sleep(ms) {
      await new Promise((resolve) => setTimeout(resolve, ms));
    },
  });
}

export async function terminateDevProcesses(
  processes: readonly DevProcessInfo[],
): Promise<void> {
  if (processes.length === 0) return;
  if (PLATFORM === "windows") {
    // Recheck identity in the terminating process, and never use taskkill /T:
    // only the descendants selected from our snapshot may be terminated.
    await commandOutput("powershell", [
      "-NoProfile",
      "-NonInteractive",
      "-Command",
      "$ErrorActionPreference = 'Stop'; " +
      "[Console]::InputEncoding = [System.Text.UTF8Encoding]::new(); " +
      "$targets = ConvertFrom-Json ([Console]::In.ReadToEnd()); foreach ($target in $targets) { " +
      "$current = Get-CimInstance Win32_Process -Filter ('ProcessId = ' + $target.pid); " +
      "if ($current -and $current.CreationDate -and " +
      "$current.CreationDate.ToUniversalTime().ToString('o') -ceq $target.startTime -and " +
      "[string]$current.CommandLine -ceq $target.commandLine) { Stop-Process -Id $current.ProcessId -Force -ErrorAction SilentlyContinue } }",
    ], JSON.stringify(processes));
    return;
  }
  const signalMatching = async (signal: Deno.Signal): Promise<void> => {
    const current = new Map(
      (await listDevProcesses()).map((entry) => [entry.pid, entry]),
    );
    for (const process of processes) {
      const live = current.get(process.pid);
      if (!live || !matchesDevIdentity(live, process)) continue;
      try {
        Deno.kill(process.pid, signal);
      } catch (error) {
        if (!(error instanceof Deno.errors.NotFound)) throw error;
      }
    }
  };
  await signalMatching("SIGTERM");
  await new Promise((resolve) => setTimeout(resolve, 1500));
  await signalMatching("SIGKILL");
}

export async function reportDevStartupPortConflict(
  port: number | null,
  native: boolean,
): Promise<void> {
  if (
    !native && port !== null && Number.isInteger(port) && port > 0 &&
    port <= 65535
  ) {
    const controller = new AbortController();
    const timer = setTimeout(
      () => controller.abort(new Error("Port owner lookup timed out")),
      5000,
    );
    try {
      await reportDevPortConflict(
        `Port ${port} is already in use`,
        controller.signal,
      );
    } finally {
      clearTimeout(timer);
    }
  }
}

/** Called only after a startup log reports a port conflict. No ports change. */
export async function reportDevPortConflict(
  log: string,
  signal?: AbortSignal,
): Promise<boolean> {
  const lines = log.split(/\r?\n/).filter((line) =>
    /port[^\n]*already in use|EADDRINUSE/i.test(line)
  );
  if (lines.length === 0) return false;
  const ports = new Set(
    lines.flatMap((line) =>
      [...line.matchAll(/(?:port\s+|:)(\d{2,5})\b/ig)].map((match) =>
        Number(match[1])
      )
    ),
  );
  console.error(
    "[dev-tool] A port is already in use. Listening ports and owner PIDs:",
  );
  try {
    if (PLATFORM === "windows") {
      const filter = ports.size
        ? `-LocalPort ${[...ports].join(",")}`
        : "-State Listen";
      console.error(
        await commandOutput(
          "powershell",
          [
            "-NoProfile",
            "-NonInteractive",
            "-Command",
            `Get-NetTCPConnection ${filter} -ErrorAction Stop | ` +
            "Select-Object -Property LocalPort,OwningProcess -Unique | ForEach-Object { " +
            "'[dev-tool] Port ' + $_.LocalPort + ': PID ' + $_.OwningProcess }",
          ],
          undefined,
          signal,
        ),
      );
    } else {
      const filter = ports.size ? `-iTCP:${[...ports].join(",")}` : "-iTCP";
      console.error(
        await commandOutput(
          "lsof",
          ["-nP", filter, "-sTCP:LISTEN"],
          undefined,
          signal,
        ),
      );
    }
  } catch (error) {
    console.error("[dev-tool] Could not query port owners:", error);
  }
  console.error(
    "[dev-tool] Use 'dev-tool stop' in the owning checkout, or 'dev-tool stop --all' for machine-wide recovery (stops other checkouts too).",
  );
  return true;
}
