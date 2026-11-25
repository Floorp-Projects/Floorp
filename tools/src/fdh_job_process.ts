// SPDX-License-Identifier: MPL-2.0

/**
 * Real processes for the fdh job Executor (B-02, KAN-129): start a step
 * command with its output in a log file, identify its root process, and stop
 * its process tree, including a tree whose executor was lost.
 *
 * A tree is selected from a process snapshot (pid, parent pid, start time,
 * command line) and stopped through `terminateDevProcesses`, which re-checks
 * each identity before it stops anything, so a reused pid is never killed.
 * On Windows a child keeps the parent pid of a dead parent, so the tree of a
 * lost step is still found; children are matched only when they started at
 * or after their parent. On POSIX the step command is started in its own
 * process group (detached), and the group is signalled as well. When a POSIX
 * root has exited, its children are re-parented (to init or a subreaper), so
 * they are found by that process group instead (a group id is not reused
 * while the group has members).
 */

import { listDevProcesses, terminateDevProcesses } from "./dev_processes.ts";
import { matchesDevIdentity } from "./dev_process_selection.ts";
import type { DevProcessInfo } from "./dev_process_types.ts";
import type {
  ProcessIdentity,
  RunningStep,
  StepCommand,
} from "./fdh_executor.ts";

/** Resolve `deno` to this Deno, so a recorded argv runs the same toolchain. */
export function resolveProgram(program: string): string {
  return program === "deno" ? Deno.execPath() : program;
}

/**
 * Did `child` start at or after `parent`? Windows and macOS give ISO or
 * ps `lstart` times; Linux gives `<boot-id>:<start-ticks>`, compared
 * numerically within one boot. Anything else is not comparable: false.
 */
export function startedNoEarlier(child: string, parent: string): boolean {
  const ticks = /^(.+):(\d+)$/;
  const c = ticks.exec(child);
  const p = ticks.exec(parent);
  if (c && p && !/^\d{4}-/.test(child)) {
    return c[1] === p[1] && Number(c[2]) >= Number(p[2]);
  }
  const a = Date.parse(child);
  const b = Date.parse(parent);
  if (Number.isFinite(a) && Number.isFinite(b)) return a >= b;
  return false;
}

/**
 * Linux start times are `<boot-id>:<start-ticks>`; this turns them into wall
 * time (boot time from /proc/stat, which has whole-second precision).
 */
export interface LinuxClock {
  bootId: string;
  bootTimeMs: number;
  ticksPerSecond: number;
}

export interface TreeSelection {
  /** POSIX: process group id by pid, read with each snapshot entry. */
  groups?: ReadonlyMap<number, number>;
  /** Linux: to compare tick start times with `notAfter`. */
  clock?: LinuxClock;
}

/**
 * The process with `root`'s identity (if it still runs) and all of its
 * descendants in `snapshot`. When the root has exited, its children are
 * still selected by parent pid if they started at or after the root, and
 * (POSIX, with `groups`) the members of the root's process group too.
 */
export function selectProcessTree(
  snapshot: readonly DevProcessInfo[],
  root: ProcessIdentity,
  /**
   * When the root has exited, its direct children and its group members are
   * only trusted if they started no later than this (the time the attempt
   * was declared lost): a process that reused the root's pid and exited
   * since cannot have children that started before the loss.
   */
  notAfter?: string,
  selection: TreeSelection = {},
): DevProcessInfo[] {
  const byParent = new Map<number, DevProcessInfo[]>();
  for (const process of snapshot) {
    const list = byParent.get(process.ppid) ?? [];
    list.push(process);
    byParent.set(process.ppid, list);
  }
  const selected: DevProcessInfo[] = [];
  // A zombie root (exited, not yet reaped by a stalled executor) still holds
  // its pid, so nothing reused it: treat it as gone, not as a reuser. Only
  // when its start time is the root's (a zombie keeps it: /proc ticks on
  // Linux, lstart on macOS, where ps shows the zombie as "<defunct>").
  const holder = snapshot.find((p) =>
    p.pid === root.pid &&
    !(/<defunct>\s*$/.test(p.commandLine) && p.startTime === root.startTime)
  );
  const live = holder !== undefined && holder.startTime === root.startTime &&
      holder.commandLine === root.commandLine
    ? holder
    : undefined;
  // The root's pid now belongs to another process: children with that
  // parent pid may be that process's, not ours. Select nothing.
  if (holder !== undefined && live === undefined) return [];
  if (live !== undefined) selected.push(live);
  const queue: Array<{ pid: number; startTime: string }> = [{
    pid: root.pid,
    startTime: root.startTime,
  }];
  const seen = new Set<number>([root.pid]);
  if (selection.groups !== undefined) {
    // POSIX: the step command leads its own process group. After the root
    // exited, its children are re-parented and only the group still ties
    // them to the step. The group id is the root's pid and is not reused
    // while the group has members; the time bounds guard against a group
    // that a later process with the reused pid started after the bound (not
    // one started, and left leaderless, before it).
    for (const member of snapshot) {
      if (seen.has(member.pid)) continue;
      if (selection.groups.get(member.pid) !== root.pid) continue;
      if (!startedNoEarlier(member.startTime, root.startTime)) continue;
      if (
        live === undefined &&
        !(notAfter !== undefined &&
          startedNoLater(member.startTime, notAfter, selection.clock))
      ) {
        continue;
      }
      seen.add(member.pid);
      selected.push(member);
      queue.push({ pid: member.pid, startTime: member.startTime });
    }
  }
  while (queue.length > 0) {
    const parent = queue.shift()!;
    for (const child of byParent.get(parent.pid) ?? []) {
      if (seen.has(child.pid)) continue;
      if (!startedNoEarlier(child.startTime, parent.startTime)) continue;
      if (
        live === undefined && parent.pid === root.pid &&
        !(notAfter !== undefined &&
          startedNoLater(child.startTime, notAfter, selection.clock))
      ) {
        // Root gone: without a bound we cannot tell its children from those
        // of a later process that reused its pid. Select nothing for it.
        continue;
      }
      seen.add(child.pid);
      selected.push(child);
      queue.push({ pid: child.pid, startTime: child.startTime });
    }
  }
  return selected;
}

/**
 * Did `child` start no later than the ISO time `bound`? A Linux tick start
 * time is compared through `clock` (same boot only). Else false.
 */
export function startedNoLater(
  child: string,
  bound: string,
  clock?: LinuxClock,
): boolean {
  const b = Date.parse(bound);
  if (!Number.isFinite(b)) return false;
  const ticks = /^(.+):(\d+)$/.exec(child);
  if (ticks && !/^\d{4}-/.test(child)) {
    if (clock === undefined || ticks[1] !== clock.bootId) return false;
    // btime and the ticks are both rounded down, so this is at or before
    // the real start (by under a second): no slack is needed.
    const at = clock.bootTimeMs +
      Number(ticks[2]) * 1000 / clock.ticksPerSecond;
    return at <= b;
  }
  const a = Date.parse(child);
  return Number.isFinite(a) && a <= b;
}

async function output(cmd: string, args: string[]): Promise<string> {
  const result = await new Deno.Command(cmd, {
    args,
    stdin: "null",
    stdout: "piped",
    stderr: "piped",
    env: { LC_ALL: "C" },
  }).output();
  if (!result.success) {
    throw new Error(
      `${cmd}: ${new TextDecoder().decode(result.stderr).trim()}`,
    );
  }
  return new TextDecoder().decode(result.stdout);
}

/** Process group id by pid, from the snapshot entries (POSIX). */
export function groupsOf(
  snapshot: readonly DevProcessInfo[],
): Map<number, number> {
  return new Map(
    snapshot.flatMap((p) =>
      p.pgid === undefined ? [] : [[p.pid, p.pgid] as [number, number]]
    ),
  );
}

let clockOnce: Promise<LinuxClock | undefined> | undefined;

/**
 * Linux: the clock for tick start times; undefined elsewhere or unreadable.
 * Read once per process (boot id and boot time do not change).
 */
export function linuxClock(): Promise<LinuxClock | undefined> {
  clockOnce ??= readLinuxClock();
  return clockOnce;
}

async function readLinuxClock(): Promise<LinuxClock | undefined> {
  if (Deno.build.os !== "linux") return undefined;
  try {
    const bootId = (await Deno.readTextFile("/proc/sys/kernel/random/boot_id"))
      .trim();
    const btime = /^btime\s+(\d+)$/m.exec(
      await Deno.readTextFile("/proc/stat"),
    );
    if (btime === null) return undefined;
    let ticksPerSecond = 100; // USER_HZ on every Linux architecture in use
    try {
      const value = Number((await output("getconf", ["CLK_TCK"])).trim());
      if (Number.isInteger(value) && value > 0) ticksPerSecond = value;
    } catch {
      // keep the default
    }
    return { bootId, bootTimeMs: Number(btime[1]) * 1000, ticksPerSecond };
  } catch {
    return undefined;
  }
}

async function snapshotWithGroups(): Promise<{
  snapshot: DevProcessInfo[];
  groups: Map<number, number>;
}> {
  const snapshot = await listDevProcesses();
  return { snapshot, groups: groupsOf(snapshot) };
}

/** Identity of a running process, or null when it is gone. */
export async function identifyProcess(
  pid: number,
): Promise<ProcessIdentity | null> {
  const [process] = await listDevProcesses(pid);
  if (process === undefined || process.startTime === "") return null;
  return {
    pid: process.pid,
    startTime: process.startTime,
    commandLine: process.commandLine,
  };
}

/**
 * Stop the process tree rooted at `root` (identity-checked). Returns the
 * processes it selected. A tree that is already gone selects nothing.
 */
export async function stopProcessTree(
  root: ProcessIdentity,
  notAfter?: string,
): Promise<DevProcessInfo[]> {
  const { snapshot, groups } = await snapshotWithGroups();
  const tree = selectProcessTree(snapshot, root, notAfter, {
    groups,
    clock: await linuxClock(),
  });
  const rootIsLive = tree.some((p) =>
    p.pid === root.pid && p.startTime === root.startTime
  );
  if (Deno.build.os !== "windows" && tree.length > 0) {
    // The step command leads its own process group (detached spawn). Signal
    // the group (this also reaches processes started after the snapshot)
    // only while one of its selected members is identity-matched in it: its
    // leader, or, once the leader exited, another member (the group id is
    // not reused while that member is in the group).
    const members = rootIsLive
      ? tree.filter((p) => p.pid === root.pid)
      : tree.filter((p) => groups.get(p.pid) === root.pid);
    // A failed re-check only skips the group signal: the per-process stop
    // below (identity-checked on its own snapshot) still runs.
    if (
      members.length > 0 &&
      (rootIsLive ||
        await stillInGroup(members, root.pid).catch(() => false))
    ) {
      try {
        Deno.kill(-root.pid, "SIGTERM");
      } catch {
        // The group is gone; the per-process stop below still runs.
      }
    }
  }
  if (tree.length > 0) await terminateDevProcesses(tree);
  return tree;
}

/** Is one of `members` (identity-checked) still in process group `pgid`? */
async function stillInGroup(
  members: readonly DevProcessInfo[],
  pgid: number,
): Promise<boolean> {
  const { snapshot, groups } = await snapshotWithGroups();
  const byPid = new Map(snapshot.map((p) => [p.pid, p]));
  return members.some((member) => {
    const now = byPid.get(member.pid);
    return now !== undefined && matchesDevIdentity(now, member) &&
      groups.get(member.pid) === pgid;
  });
}

/**
 * Start one step command with stdout and stderr appended to `logPath`.
 * `kill` stops the command's whole process tree.
 */
export function spawnStepCommand(
  command: StepCommand,
  cwd: string,
  logPath: string,
  env: Record<string, string> = {},
): RunningStep {
  const [program, ...args] = command.argv;
  const log = Deno.openSync(logPath, {
    create: true,
    append: true,
    write: true,
  });
  const header = new TextEncoder().encode(
    `[fdh job] ${command.displayCommand}\n`,
  );
  let child: Deno.ChildProcess;
  try {
    log.writeSync(header);
    child = new Deno.Command(resolveProgram(program), {
      args,
      cwd,
      env,
      stdin: "null",
      stdout: "piped",
      stderr: "piped",
      // POSIX: its own process group, so the tree can be signalled as one.
      detached: Deno.build.os !== "windows",
    }).spawn();
  } catch (error) {
    // Do not keep the log open (on Windows that blocks its deletion).
    log.close();
    throw error;
  }
  const copy = async (stream: ReadableStream<Uint8Array>) => {
    for await (const chunk of stream) {
      try {
        log.writeSync(chunk);
      } catch {
        // A log write failure must not stop the step.
      }
    }
  };
  const copies = Promise.all([copy(child.stdout), copy(child.stderr)]);
  const status = (async () => {
    const result = await child.status;
    await copies.catch(() => {});
    try {
      log.close();
    } catch {
      // already closed
    }
    return result.code;
  })();
  // Identify the root at once, while it surely runs, so a later kill can
  // still find its children after it exited.
  const identity = identifyProcess(child.pid).catch(() => null);
  return {
    status,
    pid: child.pid,
    identity,
    kill: async () => {
      const root = await identity;
      if (root !== null) {
        await stopProcessTree(root);
      } else {
        try {
          child.kill("SIGKILL");
        } catch {
          // already exited
        }
      }
    },
  };
}
