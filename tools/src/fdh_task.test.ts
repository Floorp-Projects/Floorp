// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertThrows } from "@std/assert";
import * as path from "@std/path";
import {
  appendEvent,
  createTaskState,
  type FdhTaskState,
  type GitRunner,
  readTaskState,
  readTaskStateFor,
  recordAssertion,
  requireWorktree,
  resumeTaskState,
  runRecordedCommand,
  taskStatePath,
  validateTaskId,
  writeTaskState,
} from "./fdh_task.ts";

const BASE = "a".repeat(40);
const CREATED_AT = "2026-11-08T00:00:00.000Z";

async function withTempDir(
  fn: (dir: string) => Promise<void> | void,
): Promise<void> {
  const dir = await Deno.makeTempDir({ prefix: "fdh-task-test-" });
  try {
    await fn(Deno.realPathSync(dir));
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
}

function makeState(worktree: string): FdhTaskState {
  return createTaskState({
    taskId: "task-one",
    worktree,
    baseCommit: BASE,
    portOffset: 20,
    now: () => CREATED_AT,
  });
}

const okRunner = () =>
  Promise.resolve({
    code: 0,
    stdout: "ok\n",
    stderr: "",
    output: "ok\n",
  });

Deno.test("validateTaskId enforces the documented pattern", () => {
  assertEquals(validateTaskId("gate3-smoke"), "gate3-smoke");
  assertEquals(validateTaskId("a1"), "a1");
  assertThrows(() => validateTaskId("Gate3"), Error, "--task-id");
  assertThrows(() => validateTaskId("-leading"), Error, "--task-id");
  assertThrows(() => validateTaskId("has space"), Error, "--task-id");
});

Deno.test("task state round-trips through <worktree>/_dist/fdh/tasks", async () => {
  await withTempDir((dir) => {
    const state = makeState(dir);
    const file = writeTaskState(state);
    assertEquals(
      file,
      path.join(dir, "_dist", "fdh", "tasks", "task-one.json"),
    );
    assertEquals(file, taskStatePath(dir, "task-one"));

    const loaded = readTaskState(dir, "task-one");
    assertEquals(loaded.taskId, "task-one");
    assertEquals(loaded.worktree, dir);
    assertEquals(loaded.baseCommit, BASE);
    assertEquals(loaded.portOffset, 20);
    assertEquals(loaded.createdAt, CREATED_AT);
    assertEquals(loaded.status, "running");
    assertEquals(loaded.events.length, 1);
    assertEquals(loaded.operations, []);
    assertEquals(loaded.assertions, []);
    assertEquals(loaded.cleanup.result, "not-required");
    assertEquals(loaded.reproduction.result, "inconclusive");
  });
});

Deno.test("events, operations and assertions persist", async () => {
  await withTempDir((dir) => {
    const state = makeState(dir);
    writeTaskState(state);
    appendEvent(state, {
      timestamp: "2026-11-08T00:01:00.000Z",
      command: "task resume",
      note: "restored state",
      exitCode: 0,
    });
    recordAssertion(state, {
      name: "build-exit-code",
      expected: "exit code 0",
      actual: "exit code 0",
      passed: true,
      at: "2026-11-08T00:02:00.000Z",
    });
    writeTaskState(state);

    const loaded = readTaskState(dir, "task-one");
    assertEquals(loaded.events.length, 2);
    assertEquals(loaded.events[1].command, "task resume");
    assertEquals(loaded.assertions.length, 1);
    assertEquals(loaded.assertions[0].name, "build-exit-code");
    assertEquals(loaded.assertions[0].passed, true);
  });
});

Deno.test("runRecordedCommand records operation, event and assertion", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    writeTaskState(state);

    const result = await runRecordedCommand(
      state,
      {
        command: "/fake/deno",
        args: ["task", "test:host"],
        cwd: dir,
        displayCommand: "deno task test:host",
        assertionName: "host-test-exit-code",
        expected: "exit code 0",
      },
      okRunner,
    );

    assertEquals(result.code, 0);
    assertEquals(state.operations.length, 1);
    assertEquals(state.operations[0].displayCommand, "deno task test:host");
    assertEquals(state.operations[0].output, "ok\n");
    assertEquals(state.assertions[0].passed, true);

    const loaded = readTaskState(dir, "task-one");
    assertEquals(loaded.events.length, 2);
    assertEquals(loaded.operations[0].displayCommand, "deno task test:host");
    assertEquals(loaded.assertions.length, 1);
  });
});

Deno.test("a failing command records a failed assertion", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    writeTaskState(state);

    const result = await runRecordedCommand(
      state,
      {
        command: "/fake/deno",
        args: ["task", "test:host"],
        cwd: dir,
        displayCommand: "deno task test:host",
        assertionName: "host-test-exit-code",
      },
      () =>
        Promise.resolve({
          code: 7,
          stdout: "",
          stderr: "boom",
          output: "boom",
        }),
    );

    assertEquals(result.code, 7);
    assertEquals(state.assertions[0].passed, false);
    assertEquals(state.assertions[0].actual, "exit code 7");
  });
});

Deno.test("resume re-verifies local state without external operations", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    writeTaskState(state);

    const git: GitRunner = (cwd, args) => {
      if (args[0] === "rev-parse") return Promise.resolve(dir);
      if (args[0] === "cat-file") return Promise.resolve("");
      throw new Error(`unexpected git call in ${cwd}: ${args.join(" ")}`);
    };
    const probed: number[] = [];
    const { report } = await resumeTaskState({
      taskId: "task-one",
      worktree: dir,
      git,
      probePort: (port) => {
        probed.push(port);
        return Promise.resolve(probed.length === 1);
      },
      now: () => "2026-11-08T00:03:00.000Z",
    });

    assertEquals(report.taskId, "task-one");
    assertEquals(report.worktree, dir);
    assertEquals(report.baseCommit, BASE);
    assertEquals(report.portOffset, 20);
    assertEquals(report.status, "running");
    assertEquals(report.restored, [
      "state-file",
      "worktree",
      "base-commit",
      "port-offset",
    ]);
    assertEquals(report.stale.length, 2);
    assertEquals(probed.length > 0, true);

    const loaded = readTaskState(dir, "task-one");
    assertEquals(loaded.events.at(-1)?.command, "task resume");
    const note = JSON.parse(loaded.events.at(-1)?.note ?? "{}") as {
      restored: string[];
      stale: string[];
    };
    assertEquals(note.restored.length, 4);
    assertEquals(note.stale, report.stale);
  });
});

Deno.test("resume fails when the recorded base commit is gone", async () => {
  await withTempDir(async (dir) => {
    writeTaskState(makeState(dir));

    const git: GitRunner = (cwd, args) => {
      if (args[0] === "rev-parse") return Promise.resolve(dir);
      if (args[0] === "cat-file") return Promise.reject(new Error("missing"));
      throw new Error(`unexpected git call in ${cwd}: ${args.join(" ")}`);
    };
    await assertRejects(
      () =>
        resumeTaskState({
          taskId: "task-one",
          worktree: dir,
          git,
          probePort: () => Promise.resolve(false),
        }),
      Error,
      "base commit",
    );
  });
});

/** Write `state` under `dir` as if the file had been copied there. */
function writeStateUnder(dir: string, state: FdhTaskState): void {
  const file = taskStatePath(dir, state.taskId);
  Deno.mkdirSync(path.dirname(file), { recursive: true });
  Deno.writeTextFileSync(file, JSON.stringify(state));
}

Deno.test("readTaskStateFor accepts another spelling of this worktree and returns the canonical path", async () => {
  await withTempDir((dir) => {
    const here = path.join(dir, "repo");
    Deno.mkdirSync(path.join(here, "sub"), { recursive: true });
    const spellings = [
      `${here}${path.SEPARATOR}`,
      path.join(here, "sub") + `${path.SEPARATOR}..`,
    ];
    if (Deno.build.os === "windows") {
      spellings.push(here.toUpperCase(), here.replaceAll("\\", "/"));
    }
    for (const spelling of spellings) {
      writeStateUnder(here, { ...makeState(here), worktree: spelling });
      const state = readTaskStateFor(here, "task-one");
      assertEquals(state.worktree, here, spelling);
    }
  });
});

Deno.test("readTaskStateFor refuses a state that records another worktree", async () => {
  await withTempDir((dir) => {
    const here = path.join(dir, "a");
    const other = path.join(dir, "b");
    Deno.mkdirSync(here);
    Deno.mkdirSync(other);
    // A copied state file: it sits under `here` but records `other`, so every
    // derived path (ledger, locks, the state file) would point at `other`.
    writeStateUnder(here, makeState(other));
    assertThrows(
      () => readTaskStateFor(here, "task-one"),
      Error,
      `belongs to ${other}, not ${here}; this task cannot be resumed here`,
    );
  });
});

Deno.test("readTaskStateFor refuses a state whose recorded worktree is gone", async () => {
  await withTempDir((dir) => {
    const here = path.join(dir, "a");
    const moved = path.join(dir, "moved-away");
    Deno.mkdirSync(here);
    writeStateUnder(here, makeState(moved));
    assertThrows(
      () => readTaskStateFor(here, "task-one"),
      Error,
      "recorded worktree no longer exists",
    );
  });
});

Deno.test("requireWorktree rejects a subdirectory / non-root", async () => {
  await withTempDir(async (dir) => {
    const subdir = path.join(dir, "nested");
    Deno.mkdirSync(subdir);
    const git: GitRunner = () => Promise.resolve(dir);
    await assertRejects(
      () => requireWorktree(subdir, git),
      Error,
      "git work-tree root",
    );
  });
});

Deno.test("createTaskState rejects invalid input", () => {
  assertThrows(
    () =>
      createTaskState({
        taskId: "Bad ID",
        worktree: "/w",
        baseCommit: BASE,
        portOffset: 0,
      }),
    Error,
    "--task-id",
  );
  assertThrows(
    () =>
      createTaskState({
        taskId: "ok",
        worktree: "/w",
        baseCommit: BASE,
        portOffset: -1,
      }),
    Error,
    "port offset",
  );
  assertThrows(
    () =>
      createTaskState({
        taskId: "ok",
        worktree: "/w",
        baseCommit: "not-a-sha",
        portOffset: 0,
      }),
    Error,
    "base commit",
  );
});

// --- merge-on-write (KAN-129) ----------------------------------------------

const ev = (command: string) => ({
  timestamp: CREATED_AT,
  command,
  note: "",
  exitCode: 0,
});

Deno.test("writeTaskState keeps events appended by another writer since this state was read", async () => {
  await withTempDir((dir) => {
    writeTaskState(makeState(dir));
    // Two commands load the same snapshot (e.g. a build and `quota pending`).
    const build = readTaskState(dir, "task-one");
    const quota = readTaskState(dir, "task-one");
    appendEvent(quota, ev("quota pending"));
    writeTaskState(quota);
    appendEvent(build, ev("verify build"));
    recordAssertion(build, {
      name: "build-exit-code",
      expected: "exit code 0",
      actual: "exit code 0",
      passed: true,
      at: CREATED_AT,
    });
    writeTaskState(build);

    const onDisk = readTaskState(dir, "task-one");
    assertEquals(onDisk.events.slice(1).map((e) => e.command), [
      "quota pending",
      "verify build",
    ]);
    assertEquals(onDisk.assertions.length, 1);
    // The writer's object now mirrors the file.
    assertEquals(build.events.length, 3);
  });
});

Deno.test("writeTaskState: a field this writer changed wins, an unchanged field keeps the file's value", async () => {
  await withTempDir((dir) => {
    writeTaskState(makeState(dir));
    const prepare = readTaskState(dir, "task-one");
    const dev = readTaskState(dir, "task-one");
    dev.cleanup = { result: "completed", notes: "dev down" };
    writeTaskState(dev);
    prepare.status = "failed";
    writeTaskState(prepare);

    const onDisk = readTaskState(dir, "task-one");
    assertEquals(onDisk.status, "failed");
    assertEquals(onDisk.cleanup, { result: "completed", notes: "dev down" });
  });
});

Deno.test("writeTaskState twice from one object appends each entry once", async () => {
  await withTempDir((dir) => {
    writeTaskState(makeState(dir));
    const state = readTaskState(dir, "task-one");
    appendEvent(state, ev("first"));
    writeTaskState(state);
    const other = readTaskState(dir, "task-one");
    appendEvent(other, ev("other"));
    writeTaskState(other);
    appendEvent(state, ev("second"));
    writeTaskState(state);

    assertEquals(
      readTaskState(dir, "task-one").events.slice(1).map((e) => e.command),
      ["first", "other", "second"],
    );
  });
});

Deno.test("writeTaskState writes a new task's state as is", async () => {
  await withTempDir((dir) => {
    const state = makeState(dir);
    appendEvent(state, ev("extra"));
    writeTaskState(state);
    assertEquals(
      readTaskState(dir, "task-one").events.slice(1).map((e) => e.command),
      ["extra"],
    );
  });
});
