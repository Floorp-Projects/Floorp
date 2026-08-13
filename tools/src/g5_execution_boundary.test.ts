// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals } from "@std/assert";
import {
  assessG5ExecutionBoundary,
  type G5BoundaryFacts,
  type G5ExecutionBlocker,
} from "./g5_execution_boundary.ts";

const RUN_ID = "g5-run-20260812-001";
const EXECUTOR_INSTANCE_ID = "g5-executor-20260812-001";

function completeFacts(): G5BoundaryFacts {
  return {
    host: "disposable-macos-vm",
    runId: RUN_ID,
    supervision: {
      descendants: "causally-complete",
      eventStream: "complete",
      kind: "launch-bound-event-supervisor",
      pidGeneration: "high-resolution",
    },
    executorInstanceId: EXECUTOR_INSTANCE_ID,
  };
}

Deno.test("G5 boundary admits a GitHub-hosted macOS candidate only for independent verification", () => {
  const assessment = assessG5ExecutionBoundary({
    ...completeFacts(),
    host: "github-hosted-macos",
  });

  assertEquals(assessment.trustedExecutorVerification, "required");
  assertEquals(assessment.trustedExecutorVerificationBlockers, []);
  assertEquals(assessment.cleanupBoundary, "not-established");
  assertEquals(assessment.g5Result, "not-assessed");
});

Deno.test("G5 boundary requires a nonblank executor-instance correlation ID", () => {
  for (const executorInstanceId of ["", "  "]) {
    const assessment = assessG5ExecutionBoundary({
      ...completeFacts(),
      executorInstanceId,
    });

    assertEquals(assessment.trustedExecutorVerification, "blocked");
    assertEquals(assessment.trustedExecutorVerificationBlockers, [
      "run-or-executor-instance-identity-absent",
    ]);
    assertEquals(assessment.cleanupBoundary, "not-established");
    assertEquals(assessment.g5Result, "not-assessed");
  }
});

Deno.test("G5 boundary rejects malformed runtime facts without coercion", () => {
  const malformed = [
    null,
    [],
    { ...completeFacts(), runId: { trim: () => "forged" } },
    { ...completeFacts(), executorInstanceId: { trim: () => "forged" } },
    { ...completeFacts(), supervision: {} },
    {
      ...completeFacts(),
      supervision: { ...completeFacts().supervision, kind: "unrecognized" },
    },
    { ...completeFacts(), unexpected: "not-accepted" },
  ];

  for (const facts of malformed) {
    const assessment = assessG5ExecutionBoundary(facts);
    assertEquals(assessment.trustedExecutorVerification, "blocked");
    assertEquals(assessment.trustedExecutorVerificationBlockers, [
      "malformed-boundary-facts",
    ]);
    assertEquals(assessment.cleanupBoundary, "not-established");
    assertEquals(assessment.g5Result, "not-assessed");
  }
});

Deno.test("G5 boundary fails closed rather than throwing for a revoked Proxy", () => {
  const { proxy, revoke } = Proxy.revocable(completeFacts(), {});
  revoke();

  const assessment = assessG5ExecutionBoundary(proxy);
  assertEquals(assessment.trustedExecutorVerification, "blocked");
  assertEquals(assessment.trustedExecutorVerificationBlockers, [
    "malformed-boundary-facts",
  ]);
  assertEquals(assessment.cleanupBoundary, "not-established");
  assertEquals(assessment.g5Result, "not-assessed");
});

Deno.test("G5 boundary freezes every returned assessment surface", () => {
  const assessment = assessG5ExecutionBoundary(completeFacts());

  assert(Object.isFrozen(assessment));
  assert(Object.isFrozen(assessment.cleanupBlockers));
  assert(Object.isFrozen(assessment.trustedExecutorVerificationBlockers));
});

Deno.test("G5 boundary blocks every incomplete local ownership proof", () => {
  const cases: Array<{
    readonly facts: G5BoundaryFacts;
    readonly blocker: G5ExecutionBlocker;
  }> = [
    {
      facts: { ...completeFacts(), host: "persistent-host" },
      blocker: "persistent-or-unknown-host",
    },
    {
      facts: { ...completeFacts(), host: "unknown" },
      blocker: "persistent-or-unknown-host",
    },
    {
      facts: {
        ...completeFacts(),
        supervision: { ...completeFacts().supervision, kind: "polling-only" },
      },
      blocker: "polling-or-unknown-supervisor",
    },
    {
      facts: {
        ...completeFacts(),
        supervision: { ...completeFacts().supervision, kind: "unknown" },
      },
      blocker: "polling-or-unknown-supervisor",
    },
    {
      facts: {
        ...completeFacts(),
        supervision: {
          ...completeFacts().supervision,
          pidGeneration: "coarse-or-unknown",
        },
      },
      blocker: "coarse-or-unknown-pid-generation",
    },
    {
      facts: {
        ...completeFacts(),
        supervision: {
          ...completeFacts().supervision,
          eventStream: "lost-or-unknown",
        },
      },
      blocker: "event-stream-lost-or-unknown",
    },
    {
      facts: {
        ...completeFacts(),
        supervision: {
          ...completeFacts().supervision,
          descendants: "escaped-or-unprovable",
        },
      },
      blocker: "descendant-ownership-unprovable",
    },
  ];

  for (const { facts, blocker } of cases) {
    const assessment = assessG5ExecutionBoundary(facts);
    assertEquals(assessment.trustedExecutorVerification, "blocked", blocker);
    assertEquals(assessment.trustedExecutorVerificationBlockers, [blocker]);
    assertEquals(assessment.cleanupBoundary, "not-established");
    assertEquals(assessment.cleanupBlockers, [
      "external-executor-lifecycle-verification-required",
    ]);
    assertEquals(assessment.g5Result, "not-assessed");
  }
});

Deno.test("G5 boundary blocks absent or blank run and executor-instance identities", () => {
  for (
    const facts of [
      { ...completeFacts(), runId: "" },
      { ...completeFacts(), runId: "  " },
      { ...completeFacts(), executorInstanceId: "" },
      { ...completeFacts(), executorInstanceId: "  " },
    ]
  ) {
    const assessment = assessG5ExecutionBoundary(facts);
    assertEquals(assessment.trustedExecutorVerification, "blocked");
    assertEquals(assessment.trustedExecutorVerificationBlockers, [
      "run-or-executor-instance-identity-absent",
    ]);
    assertEquals(assessment.cleanupBoundary, "not-established");
    assertEquals(assessment.g5Result, "not-assessed");
  }
});

Deno.test("G5 boundary requires an independent verifier before execution", () => {
  const assessment = assessG5ExecutionBoundary(completeFacts());

  assertEquals(assessment.trustedExecutorVerification, "required");
  assertEquals(assessment.trustedExecutorVerificationBlockers, []);
  assertEquals(assessment.cleanupBoundary, "not-established");
  assertEquals(assessment.cleanupBlockers, [
    "external-executor-lifecycle-verification-required",
  ]);
  assertEquals(assessment.g5Result, "not-assessed");
});

Deno.test("G5 boundary reports every independent ownership failure", () => {
  const assessment = assessG5ExecutionBoundary({
    host: "persistent-host",
    runId: RUN_ID,
    supervision: {
      descendants: "escaped-or-unprovable",
      eventStream: "lost-or-unknown",
      kind: "polling-only",
      pidGeneration: "coarse-or-unknown",
    },
    executorInstanceId: EXECUTOR_INSTANCE_ID,
  });

  assertEquals(assessment.trustedExecutorVerification, "blocked");
  assertEquals(assessment.trustedExecutorVerificationBlockers, [
    "persistent-or-unknown-host",
    "polling-or-unknown-supervisor",
    "coarse-or-unknown-pid-generation",
    "event-stream-lost-or-unknown",
    "descendant-ownership-unprovable",
  ]);
  assertEquals(assessment.cleanupBoundary, "not-established");
  assertEquals(assessment.g5Result, "not-assessed");
});

Deno.test("G5 boundary blocks unrecognized runtime facts", () => {
  const assessment = assessG5ExecutionBoundary({
    host: "unrecognized-host",
    runId: RUN_ID,
    supervision: {
      descendants: "unrecognized-descendants",
      eventStream: "unrecognized-event-stream",
      kind: "unrecognized-supervisor",
      pidGeneration: "unrecognized-pid-generation",
    },
    executorInstanceId: EXECUTOR_INSTANCE_ID,
  });

  assertEquals(assessment.trustedExecutorVerification, "blocked");
  assertEquals(assessment.trustedExecutorVerificationBlockers, [
    "malformed-boundary-facts",
  ]);
});

Deno.test("G5 boundary policy accepts no raw destruction attestation or process cleanup input", async () => {
  const source = await Deno.readTextFile(
    new URL("./g5_execution_boundary.ts", import.meta.url),
  );
  for (
    const forbidden of [
      "Deno.Command",
      "Deno.kill",
      "Deno.remove",
      "browser_launcher",
      "owned_browser_process_controller",
      "profilePath",
      ".cleanup(",
      "externalVmDestruction",
      "externally-verified",
      "evidenceSha256",
      "destroyedAt",
      'execution: "eligible"',
      "executionBlockers",
    ]
  ) {
    assertEquals(source.includes(forbidden), false, forbidden);
  }
});
