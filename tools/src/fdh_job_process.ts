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
 * process group (detached), and the group is signalled as well.
 */

import { listDevProcesses, terminateDevProcesses } from "./dev_processes.ts";
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
 * The process with `root`'s identity (if it still runs) and all of its
 * descendants in `snapshot`. When the root has exited, its children are
 * still selected by parent pid if they started at or after the root.
 */
export function selectProcessTree(
  snapshot: readonly DevProcessInfo[],
  root: ProcessIdentity,
  /**
   * When the root has exited, its direct children are only trusted if they
   * started no later than this (the time the attempt was declared lost): a
   * process that reused the root's pid and exited since cannot have
   * children that started before the loss.
   */
  notAfter?: string,
): DevProcessInfo[] {
  const byParent = new Map<number, DevProcessInfo[]>();
  for (const process of snapshot) {
    const list = byParent.get(process.ppid) ?? [];
    list.push(process);
    byParent.set(process.ppid, list);
  }
  const selected: DevProcessInfo[] = [];
  const holder = snapshot.find((p) => p.pid === root.pid);
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
  while (queue.length > 0) {
    const parent = queue.shift()!;
    for (const child of byParent.get(parent.pid) ?? []) {
      if (seen.has(child.pid)) continue;
      if (!startedNoEarlier(child.startTime, parent.startTime)) continue;
      if (
        live === undefined && parent.pid === root.pid &&
        !(notAfter !== undefined && startedNoLater(child.startTime, notAfter))
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

/** Did `child` start no later than the ISO time `bound`? Else false. */
export function startedNoLater(child: string, bound: string): boolean {
  const a = Date.parse(child);
  const b = Date.parse(bound);
  return Number.isFinite(a) && Number.isFinite(b) && a <= b;
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
  const snapshot = await listDevProcesses();
  const tree = selectProcessTree(snapshot, root, notAfter);
  const rootIsLive = tree.some((p) =>
    p.pid === root.pid && p.startTime === root.startTime
  );
  if (Deno.build.os !== "windows" && rootIsLive) {
    try {
      // The step command leads its own process group (detached spawn); only
      // signal the group while its leader is identity-matched.
      Deno.kill(-root.pid, "SIGTERM");
    } catch {
      // The group is gone; the per-process stop below still runs.
    }
  }
  if (tree.length > 0) await terminateDevProcesses(tree);
  return tree;
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
