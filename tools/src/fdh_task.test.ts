// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertThrows } from "@std/assert";
import * as path from "@std/path";
import {
  appendEvent,
  createTaskState,
  type FdhTaskState,
  type GitRunner,
  readTaskState,
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
