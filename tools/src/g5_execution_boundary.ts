// SPDX-License-Identifier: MPL-2.0

export type G5ExecutionBlocker =
  | "persistent-or-unknown-host"
  | "polling-or-unknown-supervisor"
  | "coarse-or-unknown-pid-generation"
  | "event-stream-lost-or-unknown"
  | "descendant-ownership-unprovable"
  | "run-or-vm-identity-absent";

export interface G5BoundaryFacts {
  host: "disposable-macos-vm" | "persistent-host" | "unknown";
  runId: string;
  supervision: {
    descendants: "causally-complete" | "escaped-or-unprovable";
    eventStream: "complete" | "lost-or-unknown";
    kind: "launch-bound-event-supervisor" | "polling-only" | "unknown";
    pidGeneration: "coarse-or-unknown" | "high-resolution";
  };
  vmId: string;
}

export interface G5BoundaryAssessment {
  cleanupBlockers: readonly ["external-vm-lifecycle-verification-required"];
  cleanupBoundary: "not-established";
  g5Result: "not-assessed";
  trustedExecutorVerification: "blocked" | "required";
  trustedExecutorVerificationBlockers: readonly G5ExecutionBlocker[];
}

function nonEmpty(value: string): boolean {
  return value.trim().length > 0;
}

function evaluateExecutionBlockers(
  facts: G5BoundaryFacts,
): readonly G5ExecutionBlocker[] {
  if (!nonEmpty(facts.runId) || !nonEmpty(facts.vmId)) {
    return ["run-or-vm-identity-absent"];
  }

  const blockers: G5ExecutionBlocker[] = [];
  if (facts.host !== "disposable-macos-vm") {
    blockers.push("persistent-or-unknown-host");
  }
  if (facts.supervision.kind !== "launch-bound-event-supervisor") {
    blockers.push("polling-or-unknown-supervisor");
  }
  if (facts.supervision.pidGeneration !== "high-resolution") {
    blockers.push("coarse-or-unknown-pid-generation");
  }
  if (facts.supervision.eventStream !== "complete") {
    blockers.push("event-stream-lost-or-unknown");
  }
  if (facts.supervision.descendants !== "causally-complete") {
    blockers.push("descendant-ownership-unprovable");
  }
  return blockers;
}

/**
 * Pure, pre-execution policy for a protected G5 run.
 *
 * It deliberately does not accept or validate external lifecycle evidence.
 * A separately trusted verifier must bind VM destruction to this run and VM
 * before any final cleanup or G5 evidence claim can be made.
 *
 * `required` is not an execution authorization. It only means the supplied
 * facts have no local policy blockers and must still be independently verified
 * by the trusted executor before it launches anything.
 */
export function assessG5ExecutionBoundary(
  facts: G5BoundaryFacts,
): G5BoundaryAssessment {
  const trustedExecutorVerificationBlockers = evaluateExecutionBlockers(facts);
  return {
    cleanupBlockers: ["external-vm-lifecycle-verification-required"],
    cleanupBoundary: "not-established",
    g5Result: "not-assessed",
    trustedExecutorVerification:
      trustedExecutorVerificationBlockers.length === 0 ? "required" : "blocked",
    trustedExecutorVerificationBlockers,
  };
}
