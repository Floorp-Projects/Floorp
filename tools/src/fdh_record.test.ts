// SPDX-License-Identifier: MPL-2.0

/**
 * IMP-2 A案: external command records + IMP-5 setup field.
 *
 * fdh does not run these commands; `recordExternalOperation` pins the
 * operator-supplied observation file (sha256/bytes), appends an operation and
 * event, and records an `external-<kind>-exit-code` assertion only when a
 * verification kind is given. The evidence builder re-hashes the observation
 * and must reflect an external failure in taskStatus/attempts/assertions.
 *
 * Run with: `deno test --allow-read --allow-write tools/src/fdh_record.test.ts`
 */

import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert";
import * as path from "@std/path";
import {
  createTaskState,
  type ExternalVerification,
  type FdhTaskState,
  MAX_OBSERVATION_BYTES,
  parseTaskState,
  readTaskState,
  recordExternalOperation,
  writeTaskState,
} from "./fdh_task.ts";
import {
  buildEvidenceManifest,
  EvidenceValidationError,
  validateEvidenceManifest,
} from "./fdh_evidence.ts";

const BASE = "b".repeat(40);
const CREATED_AT = "2026-11-08T00:00:00.000Z";
const ATTESTED_AT = "2026-11-08T01:00:00.000Z";
const OS = { name: "testos", arch: "x86_64" };

async function withTempDir(
  fn: (dir: string) => Promise<void> | void,
): Promise<void> {
  const dir = await Deno.makeTempDir({ prefix: "fdh-record-test-" });
  try {
    await fn(Deno.realPathSync(dir));
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
}

function makeState(worktree: string): FdhTaskState {
  return createTaskState({
    taskId: "record-one",
    worktree,
    baseCommit: BASE,
    portOffset: 20,
    now: () => CREATED_AT,
  });
}

async function writeObservation(
  worktree: string,
  name: string,
  text: string,
): Promise<string> {
  const dir = path.join(worktree, "_dist", "fdh", "observations");
  await Deno.mkdir(dir, { recursive: true });
  const file = path.join(dir, name);
  await Deno.writeTextFile(file, text);
  return file;
}

/** Mirrors the state assertions an fdh verify command would record. */
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
    startedAt: ATTESTED_AT,
    endedAt: ATTESTED_AT,
    exitCode: passed ? 0 : 1,
    output: passed ? "ok" : "failed",
  });
  state.assertions.push({
    name,
    expected: "exit code 0",
    actual: `exit code ${passed ? 0 : 1}`,
    passed,
    at: ATTESTED_AT,
  });
}

Deno.test("records an external host-test failure with verbatim observation, hash and assertion", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    writeTaskState(state);
    const text =
      "ok line\nFAILED | 425 passed | 1 failed | 1 ignored\nerror: Test failed\nexit 1\n";
    const obs = await writeObservation(dir, "host-test.log", text);

    const result = await recordExternalOperation(state, {
      command: "deno task test:host",
      cwd: dir,
      exitCode: 1,
      observationFile: obs,
      operator: "agent-7",
      verification: "host-test",
      attestedAt: ATTESTED_AT,
    });

    assertEquals(state.operations.length, 1);
    const operation = state.operations[0];
    assertEquals(operation.exitCode, 1);
    assertEquals(operation.output, text);
    assertEquals(operation.external?.operator, "agent-7");
    assertEquals(operation.external?.verification, "host-test");
    assert(
      operation.external!.logRef.startsWith("_dist/fdh/observations/"),
    );
    assert(/^[0-9a-f]{64}$/.test(operation.external!.logSha256));
    assertEquals(
      operation.external!.bytes,
      new TextEncoder().encode(text).length,
    );
    assertEquals(result.assertion?.name, "external-host-test-exit-code");
    assertEquals(result.assertion?.passed, false);
    assertEquals(result.assertion?.actual, "exit code 1");
    assertEquals(
      state.events[state.events.length - 1].command,
      "external deno task test:host",
    );

    const reread = readTaskState(dir, "record-one");
    assertEquals(reread.operations[0].external, operation.external);

    const manifest = await buildEvidenceManifest(state, { os: OS });
    assertEquals(manifest.envelope.taskStatus.state, "failed");
    assertEquals(manifest.envelope.stateTransition.to, "failed");
    assertEquals(manifest.attempts[0].result, "failed");
    assert(
      manifest.assertions.some((assertion) =>
        assertion.name === "external-host-test-exit-code" &&
        assertion.passed === false
      ),
    );
    assert(
      manifest.operations[0].observation.startsWith(
        "external (not run by fdh); exit 1;",
      ),
    );
    assert(
      manifest.limitations.some((item) => item.includes("operator-attested")),
    );
    assert(
      !manifest.limitations.some((item) =>
        item.includes("No host test verification")
      ),
    );
    assertEquals(manifest.inputFidelity.inputs.length, 2);
    assertEquals(
      manifest.inputFidelity.inputs[1].sha256,
      operation.external!.logSha256,
    );
    assertEquals(validateEvidenceManifest(manifest), []);
  });
});

Deno.test("an external build pass records a passing assertion and no build-UNVERIFIED limitation", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    writeTaskState(state);
    const obs = await writeObservation(
      dir,
      "build.log",
      "Build complete.\nexit 0\n",
    );

    const result = await recordExternalOperation(state, {
      command: "deno task feles-build build",
      cwd: dir,
      exitCode: 0,
      observationFile: obs,
      operator: "agent-7",
      verification: "build",
      attestedAt: ATTESTED_AT,
    });

    assertEquals(result.assertion?.name, "external-build-exit-code");
    assertEquals(result.assertion?.passed, true);

    const manifest = await buildEvidenceManifest(state, { os: OS });
    assertEquals(manifest.envelope.taskStatus.state, "succeeded");
    assertEquals(manifest.assertions.length, 1);
    assertEquals(manifest.assertions[0].passed, true);
    assert(
      !manifest.limitations.some((item) =>
        item.includes("No build verification")
      ),
    );
    assert(
      !manifest.limitations.some((item) =>
        item.includes("Source compilation was not verified")
      ),
    );
    assertEquals(validateEvidenceManifest(manifest), []);
  });
});

Deno.test("verification omitted records an operation and event but no assertion", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    writeTaskState(state);
    const obs = await writeObservation(dir, "manual.log", "done\nexit 0\n");

    const result = await recordExternalOperation(state, {
      command: "deno run -A tools/dev-tool.ts status",
      cwd: dir,
      exitCode: 0,
      observationFile: obs,
      operator: "agent-7",
      attestedAt: ATTESTED_AT,
    });

    assertEquals(result.assertion, undefined);
    assertEquals(state.operations.length, 1);
    assertEquals(state.assertions.length, 0);
    assertEquals(state.events.length, 2);

    const manifest = await buildEvidenceManifest(state, { os: OS });
    assertEquals(manifest.envelope.taskStatus.state, "running");
    assertEquals(manifest.operations.length, 1);
    assert(
      manifest.limitations.some((item) => item.includes("operator-attested")),
    );
    assertEquals(validateEvidenceManifest(manifest), []);
  });
});

Deno.test("rejects cwd outside the worktree and observation outside the fdh observations root", async () => {
  await withTempDir(async (dir) => {
    const outside = Deno.realPathSync(
      await Deno.makeTempDir({ prefix: "fdh-record-outside-" }),
    );
    try {
      const state = makeState(dir);
      writeTaskState(state);
      const obs = await writeObservation(dir, "bounds.log", "ok\n");
      const base = {
        command: "deno task test:host",
        exitCode: 0,
        operator: "agent-7",
        attestedAt: ATTESTED_AT,
      };

      await assertRejects(
        () =>
          recordExternalOperation(state, {
            ...base,
            cwd: outside,
            observationFile: obs,
          }),
        Error,
        "inside the task worktree",
      );

      const outsideObs = path.join(dir, "_dist", "fdh", "outside.log");
      await Deno.writeTextFile(outsideObs, "ok\n");
      await assertRejects(
        () =>
          recordExternalOperation(state, {
            ...base,
            cwd: dir,
            observationFile: outsideObs,
          }),
        Error,
        "must stay under",
      );

      const big = await writeObservation(
        dir,
        "big.log",
        "x".repeat(MAX_OBSERVATION_BYTES + 1),
      );
      await assertRejects(
        () =>
          recordExternalOperation(state, {
            ...base,
            cwd: dir,
            observationFile: big,
          }),
        Error,
        "the limit is",
      );

      await assertRejects(
        () =>
          recordExternalOperation(state, {
            ...base,
            cwd: dir,
            observationFile: obs,
            exitCode: 1.5,
          }),
        Error,
        "--exit-code must be an integer",
      );
      await assertRejects(
        () =>
          recordExternalOperation(state, {
            ...base,
            cwd: dir,
            observationFile: obs,
            operator: "   ",
          }),
        Error,
        "--operator must not be empty",
      );
      await assertRejects(
        () =>
          recordExternalOperation(state, {
            ...base,
            cwd: dir,
            observationFile: obs,
            verification: "compile" as unknown as ExternalVerification,
          }),
        Error,
        "--verification must be one of",
      );
      assertEquals(state.operations.length, 0);
    } finally {
      await Deno.remove(outside, { recursive: true });
    }
  });
});

Deno.test("evidence build detects an observation file tampered after attestation", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    writeTaskState(state);
    const obs = await writeObservation(dir, "tamper.log", "exit 0\n");
    await recordExternalOperation(state, {
      command: "deno task feles-build build",
      cwd: dir,
      exitCode: 0,
      observationFile: obs,
      operator: "agent-7",
      verification: "build",
      attestedAt: ATTESTED_AT,
    });

    await Deno.writeTextFile(obs, "exit 0\nbut the log was edited later\n");
    const tampered = await assertRejects(
      () => buildEvidenceManifest(state, { os: OS }),
      EvidenceValidationError,
    );
    assert(
      tampered.errors.some((entry) =>
        entry.message.includes("changed since attestation")
      ),
    );

    await Deno.remove(obs);
    const unreadable = await assertRejects(
      () => buildEvidenceManifest(state, { os: OS }),
      EvidenceValidationError,
    );
    assert(
      unreadable.errors.some((entry) =>
        entry.message.includes("is not readable")
      ),
    );
  });
});

Deno.test("setup is parsed when present and malformed setup is rejected", async () => {
  await withTempDir((dir) => {
    const state = makeState(dir);
    state.setup = {
      guardEnv: { FLOORP_SKIP_PATCHER: "1" },
      runtimeSource: "/tmp/runtime-registry",
      profilePath: "/tmp/fdh-profile",
      viteClearedAt: ATTESTED_AT,
      commands: ["deno install"],
    };
    writeTaskState(state);

    const reread = readTaskState(dir, "record-one");
    assertEquals(reread.setup, state.setup);

    const raw = JSON.parse(JSON.stringify(reread)) as Record<string, unknown>;
    (raw.setup as Record<string, unknown>).guardEnv = {
      FLOORP_SKIP_PATCHER: 1,
    };
    assertThrows(
      () => parseTaskState(raw),
      Error,
      "setup.guardEnv.FLOORP_SKIP_PATCHER must be a string",
    );
  });
});

Deno.test("after-mach injection alone adds the source-compilation limitation; source+inject does not", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    addCheck(
      state,
      "build-inject-exit-code",
      "deno task feles-build build --phase after-mach",
    );
    writeTaskState(state);
    const injectOnly = await buildEvidenceManifest(state, { os: OS });
    assertEquals(injectOnly.envelope.taskStatus.state, "succeeded");
    assert(
      injectOnly.limitations.some((item) =>
        item.includes("Source compilation was not verified")
      ),
    );
    assertEquals(validateEvidenceManifest(injectOnly), []);
  });

  await withTempDir(async (dir) => {
    const state = makeState(dir);
    addCheck(
      state,
      "build-exit-code",
      "deno task feles-build build --phase before-mach",
    );
    addCheck(
      state,
      "build-inject-exit-code",
      "deno task feles-build build --phase after-mach",
    );
    writeTaskState(state);
    const full = await buildEvidenceManifest(state, { os: OS });
    assert(
      !full.limitations.some((item) =>
        item.includes("Source compilation was not verified")
      ),
    );
    assert(
      !full.limitations.some((item) => item.includes("No build verification")),
    );
    assertEquals(validateEvidenceManifest(full), []);
  });

  await withTempDir(async (dir) => {
    const state = makeState(dir);
    addCheck(
      state,
      "build-exit-code",
      "deno task feles-build build --phase before-mach",
    );
    writeTaskState(state);
    const beforeOnly = await buildEvidenceManifest(state, { os: OS });
    assert(
      beforeOnly.limitations.some((item) =>
        item.includes("XHTML injection was not verified")
      ),
    );
    assertEquals(validateEvidenceManifest(beforeOnly), []);
  });
});
