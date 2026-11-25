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

Deno.test("a platform QA assertion adds the early-check limitation", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    writeTaskState(state);
    const before = await buildEvidenceManifest(state, {
      os: { name: "testos", arch: "x86_64" },
    });
    const marker = "Platform QA (fdh platform-qa, KAN-145)";
    assert(!before.limitations.some((item) => item.includes(marker)));
    addCheck(
      state,
      "platform-qa-linux",
      "deno run -A tools/fdh.ts platform-qa run --platform linux",
    );
    writeTaskState(state);
    const manifest = await buildEvidenceManifest(state, {
      os: { name: "testos", arch: "x86_64" },
    });
    assert(manifest.limitations.some((item) => item.includes(marker)));
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

Deno.test("an external build record cannot clear the not-compiled limitation", async () => {
  await withTempDir(async (dir) => {
    const notCompiled = "latest source edits were not compiled";
    const state = makeState(dir);
    addCheck(
      state,
      "build-exit-code",
      "deno task feles-build build --phase before-mach",
    );
    state.events.push({
      timestamp: "2026-11-08T00:05:00.000Z",
      command: refusedBuildEventCommand("all"),
      note: "refused",
      exitCode: 1,
    });
    // Attested later than the refusal, but operator-attested: not counted.
    state.assertions.push({
      name: "external-build-exit-code",
      expected: "exit code 0",
      actual: "exit code 0",
      passed: true,
      at: "2026-11-08T00:09:00.000Z",
    });
    writeTaskState(state);
    const manifest = await buildEvidenceManifest(state, {
      os: { name: "testos", arch: "x86_64" },
    });
    assert(manifest.limitations.some((item) => item.includes(notCompiled)));
    assertEquals(validateEvidenceManifest(manifest), []);
  });
});

// ---------------------------------------------------------------------------
// Platform QA results bound to a commit (KAN-146)
// ---------------------------------------------------------------------------

const PLATFORM_HEAD = "a".repeat(40);
const OTHER_HEAD = "c".repeat(40);

async function sha256Of(bytes: Uint8Array): Promise<string> {
  const copy = new Uint8Array(bytes.length);
  copy.set(bytes);
  return Array.from(
    new Uint8Array(await crypto.subtle.digest("SHA-256", copy)),
    (b) => b.toString(16).padStart(2, "0"),
  ).join("");
}

/** A settled Linux platform job: report and screenshot on disk, pinned. */
async function addPlatformResult(
  state: FdhTaskState,
  options: { head?: string; clean?: boolean } = {},
): Promise<{ reportFile: string; pngFile: string }> {
  const dirRef = `_dist/fdh/platform/${state.taskId}`;
  const dir = path.join(state.worktree, ...dirRef.split("/"));
  Deno.mkdirSync(dir, { recursive: true });
  const png = new TextEncoder().encode("png-bytes");
  const pngFile = path.join(dir, "linux.png");
  Deno.writeFileSync(pngFile, png);
  const report = new TextEncoder().encode(JSON.stringify({
    schema: "fdh-platform-qa-report/v1",
    taskId: state.taskId,
    platform: "linux",
    outcome: "passed",
  }));
  const reportFile = path.join(dir, "linux-report.json");
  Deno.writeFileSync(reportFile, report);
  state.assertions.push({
    name: "platform-qa-linux",
    expected: "exit code 0 (QA passed on linux)",
    actual: "exit code 0; outcome passed",
    passed: true,
    at: "2026-11-08T00:03:00.000Z",
    platform: {
      platform: "linux",
      jobId: "platform-1",
      head: options.head ?? PLATFORM_HEAD,
      cleanWorktree: options.clean ?? true,
      outcome: "passed",
      payload: {
        name: "floorp-linux-x86_64.tar.xz",
        sha256: "d".repeat(64),
        version: "12.21.1@159.0.1",
      },
      target: "Linux container image fdh-qa-linux:41fc5d48b6ce",
      attachments: [
        {
          ref: `${dirRef}/linux-report.json`,
          sha256: await sha256Of(report),
          bytes: report.length,
        },
        {
          ref: `${dirRef}/linux.png`,
          sha256: await sha256Of(png),
          bytes: png.length,
        },
      ],
      problems: [],
    },
  });
  return { reportFile, pngFile };
}

Deno.test("a platform result at HEAD on a clean worktree counts and pins its files", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    addCheck(state, "host-test-exit-code", "deno task test:host");
    const { reportFile, pngFile } = await addPlatformResult(state);
    writeTaskState(state);
    const manifest = await buildEvidenceManifest(state, {
      os: { name: "testos", arch: "x86_64" },
      head: PLATFORM_HEAD,
    });
    assertEquals(validateEvidenceManifest(manifest), []);
    assertEquals(manifest.envelope.taskStatus.state, "succeeded");
    const atHead = manifest.assertions.find((a) =>
      a.name === "platform-qa-linux-at-head"
    );
    assertEquals(atHead?.passed, true);
    assert(atHead?.expected.includes(`recorded at HEAD ${PLATFORM_HEAD} `));
    const pinned = manifest.inputFidelity.inputs.map((i) => i.path);
    assert(pinned.includes(reportFile));
    assert(pinned.includes(pngFile));
    assert(
      manifest.environmentFidelity.deviations.some((d) =>
        d.includes("fdh-qa-linux:41fc5d48b6ce")
      ),
    );
  });
});

Deno.test("a platform result from another commit, a dirty worktree or no HEAD fails the manifest", async () => {
  for (
    const [label, record, head] of [
      ["other commit", { head: OTHER_HEAD }, PLATFORM_HEAD],
      ["dirty worktree", { clean: false }, PLATFORM_HEAD],
      ["no head given", {}, undefined],
    ] as const
  ) {
    await withTempDir(async (dir) => {
      const state = makeState(dir);
      addCheck(state, "host-test-exit-code", "deno task test:host");
      await addPlatformResult(state, record);
      writeTaskState(state);
      const manifest = await buildEvidenceManifest(state, {
        os: { name: "testos", arch: "x86_64" },
        head,
      });
      assertEquals(validateEvidenceManifest(manifest), [], label);
      assertEquals(manifest.envelope.taskStatus.state, "failed", label);
      assertEquals(
        manifest.assertions.find((a) => a.name === "platform-qa-linux-at-head")
          ?.passed,
        false,
        label,
      );
    });
  }
});

Deno.test("a changed platform report or screenshot stops the manifest build", async () => {
  for (const which of ["reportFile", "pngFile"] as const) {
    await withTempDir(async (dir) => {
      const state = makeState(dir);
      const files = await addPlatformResult(state);
      writeTaskState(state);
      Deno.writeTextFileSync(files[which], "edited");
      const error = await assertRejects(
        () =>
          buildEvidenceManifest(state, {
            os: { name: "testos", arch: "x86_64" },
            head: PLATFORM_HEAD,
          }),
        EvidenceValidationError,
      );
      assert(
        error.errors[0].message.includes(
          "changed since job platform-1 settled",
        ),
      );
    });
  }
});

Deno.test("only the latest platform result counts, and one without a binding fails", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    await addPlatformResult(state, { head: OTHER_HEAD });
    // A later run at HEAD replaces the older result.
    await addPlatformResult(state);
    writeTaskState(state);
    const manifest = await buildEvidenceManifest(state, {
      os: { name: "testos", arch: "x86_64" },
      head: PLATFORM_HEAD,
    });
    assertEquals(manifest.envelope.taskStatus.state, "succeeded");

    addCheck(
      state,
      "platform-qa-linux",
      "deno run -A tools/fdh.ts platform-qa run --platform linux",
    );
    writeTaskState(state);
    const unbound = await buildEvidenceManifest(state, {
      os: { name: "testos", arch: "x86_64" },
      head: PLATFORM_HEAD,
    });
    assertEquals(unbound.envelope.taskStatus.state, "failed");
    assert(
      unbound.assertions.some((a) =>
        a.name === "platform-qa-linux-at-head" && !a.passed &&
        a.actual.includes("no commit binding")
      ),
    );
  });
});

Deno.test("a platform binding that points outside its own files fails without reading them", async () => {
  await withTempDir(async (dir) => {
    const state = makeState(dir);
    await addPlatformResult(state);
    const record = state.assertions.at(-1)!.platform!;
    record.attachments[0] = { ...record.attachments[0], ref: "../../etc/x" };
    writeTaskState(state);
    const manifest = await buildEvidenceManifest(state, {
      os: { name: "testos", arch: "x86_64" },
      head: PLATFORM_HEAD,
    });
    assertEquals(manifest.envelope.taskStatus.state, "failed");
    assert(
      manifest.assertions.some((a) =>
        a.name === "platform-qa-linux-at-head" && a.actual.includes("malformed")
      ),
    );
    assert(
      !manifest.inputFidelity.inputs.some((i) => i.path.includes("etc")),
    );
  });
});
