// SPDX-License-Identifier: MPL-2.0

import * as posix from "@std/path/posix";
import * as windows from "@std/path/windows";
import type {
  DevProcessIdentity,
  DevProcessInfo,
  DevStopScope,
} from "./dev_process_types.ts";

export function normalizeDevPath(value: string, platform: string): string {
  const paths = platform === "windows" ? windows : posix;
  const normalized = paths.normalize(value).replace(/[\\/]+$/, "");
  return platform === "windows" ? normalized.toLowerCase() : normalized;
}

export function matchesDevIdentity(
  process: DevProcessInfo,
  identity: DevProcessIdentity,
): boolean {
  // A command line alone can be identical after a restart and PID reuse.
  return process.pid === identity.pid && identity.startTime !== "" &&
    process.startTime === identity.startTime &&
    process.commandLine === identity.commandLine;
}

/** Windows CRT quoting; Unix quoting when present in a supplied command line. */
export function splitDevCommandLine(
  command: string,
  platform: string,
): string[] {
  const result: string[] = [];
  let value = "";
  let quote = "";
  let started = false;
  for (let i = 0; i < command.length; i++) {
    const char = command[i];
    if (platform === "windows" && char === "\\") {
      let end = i;
      while (command[end] === "\\") end++;
      const count = end - i;
      if (command[end] === '"') {
        value += "\\".repeat(Math.floor(count / 2));
        if (count % 2) value += '"';
        else quote = quote ? "" : '"';
        i = end;
      } else {
        value += "\\".repeat(count);
        i = end - 1;
      }
      started = true;
    } else if (char === '"' || (platform !== "windows" && char === "'")) {
      if (!quote) quote = char;
      else if (quote === char) quote = "";
      else value += char;
      started = true;
    } else if (/\s/.test(char) && !quote) {
      if (started) result.push(value);
      value = "";
      started = false;
    } else {
      value += char;
      started = true;
    }
  }
  if (started) result.push(value);
  return result;
}

function hasCheckoutProfile(
  process: DevProcessInfo,
  scope: DevStopScope,
): boolean {
  const paths = scope.platform === "windows" ? windows : posix;
  // ps prints argv without quoting on macOS. Match the entire known argument,
  // including spaces, and require an argument boundary after it.
  const profile = normalizeDevPath(scope.profilePath, scope.platform);
  if (!paths.isAbsolute(profile)) return false;
  const argv = process.argv ??
    splitDevCommandLine(process.commandLine, scope.platform);
  if (
    argv.some((arg, index) => {
      const value = arg === "--profile"
        ? argv[index + 1]
        : arg.startsWith("--profile=")
        ? arg.slice(10)
        : undefined;
      return value !== undefined && paths.isAbsolute(value) &&
        normalizeDevPath(value, scope.platform) === profile;
    })
  ) return true;
  if (process.argv) return false;
  if (scope.platform !== "windows" && profile.includes(" ")) {
    const escaped = profile.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    // An unquoted ps argument followed by another option (or end of line).
    return new RegExp(`(?:^|\\s)--profile(?:=|\\s+)${escaped}(?=\\s+--|$)`)
      .test(process.commandLine);
  }
  return false;
}

/** Pure selection, shared by all platforms. Returns children before parents. */
export function selectDevProcesses(
  processes: readonly DevProcessInfo[],
  scope: DevStopScope,
): number[] {
  const byPid = new Map(processes.map((process) => [process.pid, process]));
  const roots = new Set<number>();
  const recorded = new Map(
    scope.recordedProcesses.map((entry) => [entry.pid, entry]),
  );
  if (scope.serverPid !== undefined) {
    const server = byPid.get(scope.serverPid);
    const identity = recorded.get(scope.serverPid);
    // Legacy PID files lack identity. Only trust them when the command itself
    // contains this checkout's absolute path (e.g. the Windows log redirect).
    const legacyRoot = normalizeDevPath(scope.projectRoot, scope.platform);
    const commandLine = scope.platform === "windows"
      ? server?.commandLine.replaceAll("/", "\\").toLowerCase()
      : server?.commandLine;
    if (
      server &&
      (identity
        ? matchesDevIdentity(server, identity)
        : server.commandLine.includes("feles-build") &&
          commandLine?.includes(
            legacyRoot +
              (scope.platform === "windows" ? "\\" : "/"),
          ))
    ) {
      roots.add(server.pid);
    }
  }
  for (const process of processes) {
    const identity = recorded.get(process.pid);
    const name = (scope.platform === "windows" ? windows : posix)
      .basename(process.name).toLowerCase();
    const floorp = /^(floorp|floorp-bin|floorp\.exe)$/.test(name);
    if (identity && matchesDevIdentity(process, identity)) {
      roots.add(process.pid);
    }
    if (floorp && hasCheckoutProfile(process, scope)) roots.add(process.pid);
    if (scope.all) {
      const legacyMatch = scope.platform === "windows"
        ? (name === "deno.exe" &&
          process.commandLine.includes("feles-build")) ||
          (floorp && /_dist.*profile.*test/i.test(process.commandLine))
        : process.commandLine.includes("feles-build") ||
          process.commandLine.includes("_dist/profile/test");
      if (legacyMatch) roots.add(process.pid);
    }
  }
  const excluded = new Set(scope.excludePids);
  const visited = new Set<number>();
  const result: number[] = [];
  const visit = (pid: number): void => {
    if (visited.has(pid) || excluded.has(pid) || pid <= 0) return;
    visited.add(pid);
    const parent = byPid.get(pid);
    for (const child of processes) {
      if (child.ppid !== pid) continue;
      // Windows can retain a dead parent's PID after it has been reused.
      if (
        parent && Date.parse(child.startTime) < Date.parse(parent.startTime)
      ) continue;
      visit(child.pid);
    }
    result.push(pid);
  };
  for (const pid of roots) visit(pid);
  return result;
}

export function parseDevProcessRegistry(value: unknown): DevProcessIdentity[] {
  if (
    typeof value !== "object" || value === null ||
    !("version" in value) || value.version !== 1 ||
    !("processes" in value) || !Array.isArray(value.processes)
  ) {
    throw new Error("Invalid dev process registry");
  }
  return value.processes.map((entry: unknown) => {
    if (
      typeof entry !== "object" || entry === null ||
      !("pid" in entry) || typeof entry.pid !== "number" ||
      !Number.isSafeInteger(entry.pid) || entry.pid <= 0 ||
      !("startTime" in entry) || typeof entry.startTime !== "string" ||
      !entry.startTime ||
      !("commandLine" in entry) || typeof entry.commandLine !== "string"
    ) {
      throw new Error("Invalid dev process identity");
    }
    return {
      pid: entry.pid,
      startTime: entry.startTime,
      commandLine: entry.commandLine,
    };
  });
}

/**
 * Parse `ps -o pid=,ppid=,lstart=,args=` output, or, with `withPgid`,
 * `ps -o pid=,ppid=,pgid=,lstart=,args=`.
 */
export function parseUnixDevProcesses(
  output: string,
  withPgid = false,
): DevProcessInfo[] {
  const lstart = String.raw`(\S+\s+\S+\s+\d+\s+\d+:\d+:\d+\s+\d+)`;
  const pattern = new RegExp(
    String.raw`^\s*(\d+)\s+(\d+)\s+${
      withPgid ? String.raw`(\d+)\s+` : ""
    }${lstart}\s+(.*)$`,
  );
  return output.split(/\r?\n/).flatMap((line) => {
    const match = line.match(pattern);
    if (!match) return [];
    const at = withPgid ? 4 : 3;
    const process: DevProcessInfo = {
      pid: Number(match[1]),
      ppid: Number(match[2]),
      startTime: match[at],
      commandLine: match[at + 1],
      name: "", // filled from a separate comm column
    };
    if (withPgid) process.pgid = Number(match[3]);
    return [process];
  });
}
