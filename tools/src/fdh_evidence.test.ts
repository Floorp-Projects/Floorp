// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects } from "@std/assert";
import * as path from "@std/path";
import {
  createTaskState,
  type FdhTaskState,
  writeTaskState,
} from "./fdh_task.ts";
import {
  buildEvidenceManifest,
  evidenceIds,
  EvidenceValidationError,
  type FdhEvidenceIds,
  validateEvidenceManifest,
  writeEvidenceManifest,
} from "./fdh_evidence.ts";
import { CORE_RELATIONSHIPS } from "../qa3/evidence/validate.ts";
import {
  guardFailedBuildEventCommand,
  refusedBuildEventCommand,
} from "./fdh_verify.ts";

const BASE = "b".repeat(40);
const CREATED_AT = "2026-11-08T00:00:00.000Z";

async function withTempDir(
  fn: (dir: string) => Promise<void> | void,
): Promise<void> {
  const dir = await Deno.makeTempDir({ prefix: "fdh-evidence-test-" });
  try {
    await fn(Deno.realPathSync(dir));
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
}

function makeState(worktree: string): FdhTaskState {
  return createTaskState({
    taskId: "evidence-one",
    worktree,
    baseCommit: BASE,
    portOffset: 20,
    now: () => CREATED_AT,
  });
}

function addCheck(
  state: FdhTaskState,
  name: string,
  displayCommand: string,
  passed = true,
): void {
  state.operations.push({
    command: "/fake/deno",
    args: displayCommand.split(" ").slice(1),
    displayCommand,
    cwd: state.worktree,
    startedAt: "2026-11-08T00:01:00.000Z",
    endedAt: "2026-11-08T00:02:00.000Z",
    exitCode: passed ? 0 : 1,
    output: passed ? "ok" : "failed",
  });
  state.assertions.push({
    name,
    expected: "exit code 0",
    actual: `exit code ${passed ? 0 : 1}`,
    passed,
    at: "2026-11-08T00:02:00.000Z",
  });
}

function issuerNamespace(issuer: string): string {
  return `${issuer}:`;
}

Deno.test("builds a valid manifest with all seven entities and five core relationships", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    addCheck(
      state,
      "build-exit-code",
      "deno task feles-build build --phase after-mach",
    );
    addCheck(state, "host-test-exit-code", "deno task test:host");
    addCheck(
      state,
      "browser-connected",
      "deno run -A tools/dev-tool.ts status",
    );
    writeTaskState(state);

    const manifest = await buildEvidenceManifest(state, {
      os: { name: "testos", version: "1.0", arch: "x86_64" },
    });

    assertEquals(manifest.schemaVersion, 1);
    assertEquals(manifest.envelope.entities.length, 7);
    assertEquals(
      new Set(manifest.envelope.entities.map((entity) => entity.kind)),
      new Set([
        "task",
        "job",
        "run",
        "attempt",
        "session",
        "workspace",
        "approval",
      ]),
    );
    for (const entity of manifest.envelope.entities) {
      assert(entity.id.startsWith(issuerNamespace(entity.issuer)));
    }

    const relationshipTypes = new Set(
      manifest.envelope.relationships.map((relation) => relation.type),
    );
    for (const required of CORE_RELATIONSHIPS) {
      assert(relationshipTypes.has(required));
    }

    const ids: FdhEvidenceIds = evidenceIds("evidence-one");
    assertEquals(manifest.attempts[0].run, ids.run);
    assertEquals(manifest.attempts[0].result, "passed");
    assertEquals(manifest.envelope.taskStatus.state, "succeeded");
    assertEquals(manifest.envelope.stateTransition.to, "succeeded");
    assertEquals(manifest.envelope.stateTransition.from, "running");
    assertEquals(manifest.envelope.cancelRequested, false);

    assertEquals(manifest.operations.map((operation) => operation.order), [
      1,
      2,
      3,
    ]);
    assertEquals(manifest.assertions.length, 3);
    assertEquals(manifest.environmentFidelity.matchesTarget, false);
    assertEquals(manifest.visualJudge, null);
    assertEquals(manifest.reproduction.result, "inconclusive");
    assertEquals(manifest.cleanup.result, "not-required");
    assert(
      manifest.inputFidelity.inputs[0].sha256.match(/^[0-9a-f]{64}$/) !== null,
    );
    assert(manifest.limitations.length >= 1);
    assert(
      manifest.limitations.some((item) =>
        item.toLowerCase().includes("visual")
      ),
    );
    assert(manifest.limitations.some((item) => item.includes("DSH")));

    assertEquals(validateEvidenceManifest(manifest), []);
  });
});

Deno.test("no assertions yields a validating running manifest, not a pass", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    writeTaskState(state);

    const manifest = await buildEvidenceManifest(state, {
      os: { name: "testos", arch: "x86_64" },
    });

    assertEquals(manifest.envelope.taskStatus.state, "running");
    assertEquals(manifest.envelope.stateTransition.from, "scheduled");
    assertEquals(manifest.envelope.stateTransition.to, "running");
    assertEquals(manifest.attempts[0].result, "inconclusive");
    assertEquals(manifest.assertions.length, 1);
    assertEquals(manifest.assertions[0].name, "task-state-recorded");
    assertEquals(manifest.operations.length, 1);
    assert(
      manifest.limitations.some((item) =>
        item.includes("Browser confirmation")
      ),
    );
    assert(
      manifest.limitations.some((item) =>
        item.includes("No build verification was recorded")
      ),
    );
    assert(
      manifest.limitations.some((item) =>
        item.includes("No host test verification was recorded")
      ),
    );
    assertEquals(validateEvidenceManifest(manifest), []);
  });
});

Deno.test("a failed assertion maps to a failed state and valid transition", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    addCheck(
      state,
      "build-exit-code",
      "deno task feles-build build --phase after-mach",
      false,
    );
    writeTaskState(state);

    const manifest = await buildEvidenceManifest(state, {
      os: { name: "testos", arch: "x86_64" },
    });

    assertEquals(manifest.envelope.taskStatus.state, "failed");
    assertEquals(manifest.envelope.stateTransition.from, "running");
    assertEquals(manifest.envelope.stateTransition.to, "failed");
    assertEquals(manifest.attempts[0].result, "failed");
    assertEquals(manifest.assertions[0].passed, false);
    assert(
      manifest.limitations.some((item) => item.includes("assertion failed")),
    );
    assertEquals(validateEvidenceManifest(manifest), []);
  });
});

Deno.test("a broken-then-fixed build ends succeeded while the failed attempt stays visible", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    addCheck(
      state,
      "build-exit-code",
      "deno task feles-build build --phase after-mach",
      false,
    );
    addCheck(
      state,
      "build-exit-code",
      "deno task feles-build build --phase after-mach",
    );
    addCheck(
      state,
      "browser-eval-contains",
      "deno run -A tools/dev-tool.ts eval document.title",
    );
    writeTaskState(state);

    const manifest = await buildEvidenceManifest(state, {
      os: { name: "testos", arch: "x86_64" },
    });

    assertEquals(manifest.envelope.taskStatus.state, "succeeded");
    assertEquals(manifest.envelope.stateTransition.to, "succeeded");
    assertEquals(manifest.attempts[0].result, "passed");
    assertEquals(manifest.assertions.length, 3);
    assertEquals(manifest.assertions[0].passed, false);
    assertEquals(manifest.assertions[1].passed, true);
    assertEquals(manifest.assertions[2].passed, true);
    assertEquals(validateEvidenceManifest(manifest), []);
  });
});

Deno.test("a browser pass followed by a fail keeps the browser-UNVERIFIED limitation", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    addCheck(
      state,
      "browser-connected",
      "deno run -A tools/dev-tool.ts status",
    );
    addCheck(
      state,
      "browser-connected",
      "deno run -A tools/dev-tool.ts status",
      false,
    );
    writeTaskState(state);

    const manifest = await buildEvidenceManifest(state, {
      os: { name: "testos", arch: "x86_64" },
    });

    assertEquals(manifest.envelope.taskStatus.state, "failed");
    assert(
      manifest.limitations.some((item) =>
        item.includes("Browser confirmation was not verified")
      ),
    );
    assertEquals(validateEvidenceManifest(manifest), []);
  });
});

Deno.test("a cancelled state uses the cancel_requested transition and not-required approval", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    state.status = "cancelled";
    writeTaskState(state);

    const manifest = await buildEvidenceManifest(state, {
      os: { name: "testos", arch: "x86_64" },
    });

    assertEquals(manifest.envelope.taskStatus.state, "cancelled");
    assertEquals(manifest.envelope.stateTransition.from, "cancel_requested");
    assertEquals(manifest.envelope.stateTransition.to, "cancelled");
    const approval = manifest.envelope.entities.find((entity) =>
      entity.kind === "approval"
    );
    assertEquals(
      (approval as { decision?: string } | undefined)?.decision,
      "not-required",
    );
    assert(
      manifest.limitations.some((item) => item.includes("No approval")),
    );
    assertEquals(validateEvidenceManifest(manifest), []);
  });
});

Deno.test("writeEvidenceManifest writes JSON that re-validates", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    addCheck(state, "host-test-exit-code", "deno task test:host");
    writeTaskState(state);

    const manifest = await buildEvidenceManifest(state, {
      os: { name: "testos", arch: "x86_64" },
    });
    const out = path.join(dir, "_dist", "fdh", "evidence", "evidence-one.json");
    const written = await writeEvidenceManifest(out, manifest);
    assertEquals(written.path, out);
    assert(written.bytes > 0);

    const parsed = JSON.parse(await Deno.readTextFile(out));
    assertEquals(validateEvidenceManifest(parsed), []);
  });
});

Deno.test("builder surfaces schema violations as EvidenceValidationError", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    state.cleanup = {
      result: "bogus",
      notes: "invalid on purpose",
    } as unknown as FdhTaskState["cleanup"];
    writeTaskState(state);

    await assertRejects(
      () =>
        buildEvidenceManifest(state, {
          os: { name: "testos", arch: "x86_64" },
        }),
      EvidenceValidationError,
    );
  });
});

Deno.test("a source build refused after the last source build is a limitation", async () => {
  await withTempDir(async (dir) => {
    const refusedLimitation = "latest source edits were not compiled";
    const state = makeState(dir);
    // build (00:02) passes, then a refused source build (00:05), then an
    // after-mach inject passes: the latest edits were never compiled.
    addCheck(
      state,
      "build-exit-code",
      "deno task feles-build build --phase before-mach",
    );
    state.events.push({
      timestamp: "2026-11-08T00:05:00.000Z",
      command: refusedBuildEventCommand("all"),
      note: "verify build --phase all refused: 12 dev process(es)",
      exitCode: 1,
    });
    addCheck(
      state,
      "build-inject-exit-code",
      "deno task feles-build build --phase after-mach",
    );
    writeTaskState(state);

    const os = { name: "testos", arch: "x86_64" };
    const manifest = await buildEvidenceManifest(state, { os });
    assert(
      manifest.limitations.some((item) => item.includes(refusedLimitation)),
    );
    assertEquals(validateEvidenceManifest(manifest), []);

    // A source build after the refusal clears the limitation.
    state.assertions.push({
      name: "build-exit-code",
      expected: "exit code 0",
      actual: "exit code 0",
      passed: true,
      at: "2026-11-08T00:09:00.000Z",
    });
    const rebuilt = await buildEvidenceManifest(state, { os });
    assertEquals(
      rebuilt.limitations.some((item) => item.includes(refusedLimitation)),
      false,
    );
  });
});

Deno.test("a blocked build or a bad timestamp keeps the not-compiled limitation", async () => {
  await withTempDir(async (dir) => {
    const notCompiled = "latest source edits were not compiled";
    const os = { name: "testos", arch: "x86_64" };
    const blockedEvent = (timestamp: string, command: string) => ({
      timestamp,
      command,
      note: "blocked",
      exitCode: 1,
    });

    // A guard failure after the last source build counts like a refusal.
    const failed = makeState(dir);
    addCheck(
      failed,
      "build-exit-code",
      "deno task feles-build build --phase before-mach",
    );
    failed.events.push(
      blockedEvent(
        "2026-11-08T00:05:00.000Z",
        guardFailedBuildEventCommand("all"),
      ),
    );
    writeTaskState(failed);
    const failedManifest = await buildEvidenceManifest(failed, { os });
    assert(
      failedManifest.limitations.some((item) => item.includes(notCompiled)),
    );
    assertEquals(validateEvidenceManifest(failedManifest), []);

    // A later source build with an unparseable time does not clear it.
    failed.assertions.push({
      name: "external-build-exit-code",
      expected: "exit code 0",
      actual: "exit code 0",
      passed: true,
      at: "not-a-time",
    });
    const badAt = await buildEvidenceManifest(failed, { os });
    assert(badAt.limitations.some((item) => item.includes(notCompiled)));

    // A blocked event with an unparseable timestamp cannot slip through: the
    // event times feed the manifest, which then fails validation.
    const badEvent = makeState(dir);
    addCheck(
      badEvent,
      "build-exit-code",
      "deno task feles-build build --phase before-mach",
    );
    badEvent.events.push(
      blockedEvent("garbage", refusedBuildEventCommand("before-mach")),
    );
    writeTaskState(badEvent);
    await assertRejects(
      () => buildEvidenceManifest(badEvent, { os }),
      EvidenceValidationError,
    );
  });
});
