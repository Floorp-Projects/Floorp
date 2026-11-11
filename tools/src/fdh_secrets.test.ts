// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";
import { createTaskState, writeTaskState } from "./fdh_task.ts";
import {
  assertNoSecretReferences,
  checkTaskSecrets,
  findSecretReferences,
  SECRETS_FILE,
} from "./fdh_secrets.ts";

const NOW = "2026-11-08T00:00:00.000Z";

Deno.test("fdh secrets: a task record that references .secrets is denied", () => {
  const state = createTaskState({
    taskId: "gate4-a0",
    worktree: "/tmp/wt",
    baseCommit: "bec27040",
    portOffset: 0,
    now: () => NOW,
  });
  state.events.push({
    timestamp: NOW,
    command: "note",
    note: `read ${SECRETS_FILE} for the signing token`,
    exitCode: 0,
  });
  const references = findSecretReferences(state);
  assertEquals(references.length, 1);
  assertEquals(references[0].marker, SECRETS_FILE);
  assertThrows(
    () => assertNoSecretReferences(state),
    Error,
    "secrets reference denied",
  );
});

Deno.test("fdh secrets: token names are rejected but clean records pass", () => {
  const state = createTaskState({
    taskId: "gate4-a0",
    worktree: "/tmp/wt",
    baseCommit: "bec27040",
    portOffset: 0,
    now: () => NOW,
  });
  assertNoSecretReferences(state);
  state.events.push({
    timestamp: NOW,
    command: "note",
    note: "SIGNPATH_API_TOKEN was exported for the build",
    exitCode: 0,
  });
  assertThrows(
    () => assertNoSecretReferences(state),
    Error,
    "secrets reference denied",
  );
  // unrelated token words do not match
  assertEquals(
    findSecretReferences({ note: "the token was approved" }).length,
    0,
  );
});

Deno.test("fdh secrets: checkTaskSecrets reads the task record, not .secrets", async () => {
  const root = await Deno.makeTempDir({ prefix: "fdh-secrets-" });
  try {
    const state = createTaskState({
      taskId: "gate4-a0",
      worktree: root,
      baseCommit: "bec27040",
      portOffset: 0,
      now: () => NOW,
    });
    writeTaskState(state);
    const clean = checkTaskSecrets(root, "gate4-a0");
    assertEquals(clean.denied, false);
    assertEquals(clean.references, []);
    assertEquals(clean.secretsFileRead, false);
    state.events.push({
      timestamp: NOW,
      command: "note",
      note: "copied .secrets into the report",
      exitCode: 0,
    });
    writeTaskState(state);
    const denied = checkTaskSecrets(root, "gate4-a0");
    assertEquals(denied.denied, true);
    assertEquals(denied.references[0].where.startsWith("$.events"), true);
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});
