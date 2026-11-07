// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { basename as posixBasename } from "@std/path/posix";
import { PLATFORM, PROJECT_ROOT } from "./defines.ts";
import {
  matchesDevIdentity,
  parseDevProcessRegistry,
  parseUnixDevProcesses,
} from "./dev_process_selection.ts";
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
): Promise<string> {
  const command = new Deno.Command(cmd, {
    args,
    stdin: input === undefined ? "null" : "piped",
    stdout: "piped",
    stderr: "piped",
    env: { LC_ALL: "C" },
  });
  let output: Deno.CommandOutput;
  if (input === undefined) {
    output = await command.output();
  } else {
    const child = command.spawn();
    const writer = child.stdin.getWriter();
    await writer.write(new TextEncoder().encode(input));
    await writer.close();
    output = await child.output();
  }
  if (!output.success) {
    throw new Error(
      `${cmd}: ${new TextDecoder().decode(output.stderr).trim()}`,
    );
  }
  return new TextDecoder().decode(output.stdout);
}

export async function listDevProcesses(
  pid?: number,
): Promise<DevProcessInfo[]> {
  if (PLATFORM === "windows") {
    const filter = pid === undefined ? "" : ` -Filter 'ProcessId = ${pid}'`;
    const output = await commandOutput("powershell", [
      "-NoProfile",
      "-NonInteractive",
      "-Command",
      "$ErrorActionPreference = 'Stop'; [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new(); " +
      `ConvertTo-Json -InputObject @(Get-CimInstance Win32_Process${filter} | ForEach-Object { ` +
      "@{ pid = [int]$_.ProcessId; ppid = [int]$_.ParentProcessId; name = $_.Name; " +
      "commandLine = [string]$_.CommandLine; startTime = $(if ($_.CreationDate) { $_.CreationDate.ToUniversalTime().ToString('o') } else { '' }) } " +
      "}) -Compress",
    ]);
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
  const output = await commandOutput("ps", [
    ...selector,
    "-o",
    "pid=,ppid=,lstart=,args=",
  ]);
  const names = await commandOutput("ps", [...selector, "-o", "pid=,comm="]);
  const byPid = new Map(
    names.split("\n").flatMap((line) => {
      const match = line.match(/^\s*(\d+)\s+(.*)$/);
      return match
        ? [[Number(match[1]), posixBasename(match[2])] as const]
        : [];
    }),
  );
  const processes = parseUnixDevProcesses(output).filter((process) =>
    pid === undefined || process.pid === pid
  );
  for (const process of processes) {
    process.name = byPid.get(process.pid) ?? "";
    if (PLATFORM === "linux") {
      try {
        process.argv = (await Deno.readTextFile(`/proc/${process.pid}/cmdline`))
          .split("\0").filter(Boolean);
        // Include boot ID and kernel start ticks for sub-second PID identity.
        const stat = await Deno.readTextFile(`/proc/${process.pid}/stat`);
        const ticks = stat.slice(stat.lastIndexOf(")") + 2).split(" ")[19];
        const boot =
          (await Deno.readTextFile("/proc/sys/kernel/random/boot_id")).trim();
        process.startTime = `${boot}:${ticks}`;
      } catch {
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
): Promise<void> {
  try {
    const process = (await listDevProcesses(pid)).find((entry) =>
      entry.pid === pid
    );
    if (!process?.startTime) {
      throw new Error(`Cannot capture identity for PID ${pid}`);
    }
    const entries = readDevProcesses(file).filter((entry) => entry.pid !== pid);
    entries.push({
      pid,
      startTime: process.startTime,
      commandLine: process.commandLine,
    });
    Deno.mkdirSync(path.dirname(file), { recursive: true });
    const temporary = `${file}.${Deno.pid}.tmp`;
    Deno.writeTextFileSync(
      temporary,
      JSON.stringify({ version: 1, processes: entries }, null, 2) + "\n",
    );
    Deno.renameSync(temporary, file);
  } catch (error) {
    console.error("[dev-tool] Cannot record dev process:", error);
  }
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

/** Called only after a startup log reports a port conflict. No ports change. */
export async function reportDevPortConflict(log: string): Promise<boolean> {
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
        await commandOutput("powershell", [
          "-NoProfile",
          "-NonInteractive",
          "-Command",
          `Get-NetTCPConnection ${filter} -ErrorAction Stop | ` +
          "Select-Object -Property LocalPort,OwningProcess -Unique | ForEach-Object { " +
          "'[dev-tool] Port ' + $_.LocalPort + ': PID ' + $_.OwningProcess }",
        ]),
      );
    } else {
      const filter = ports.size ? `-iTCP:${[...ports].join(",")}` : "-iTCP";
      console.error(
        await commandOutput("lsof", ["-nP", filter, "-sTCP:LISTEN"]),
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
