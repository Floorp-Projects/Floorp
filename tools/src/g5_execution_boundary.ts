// SPDX-License-Identifier: MPL-2.0

export type G5ExecutionBlocker =
  | "persistent-or-unknown-host"
  | "polling-or-unknown-supervisor"
  | "coarse-or-unknown-pid-generation"
  | "event-stream-lost-or-unknown"
  | "descendant-ownership-unprovable"
  | "run-or-executor-instance-identity-absent"
  | "malformed-boundary-facts";

export interface G5BoundaryFacts {
  // These are candidate executor classes only. This policy does not establish
  // their lifecycle or cleanup properties; an independent verifier must do so.
  host:
    | "disposable-macos-vm"
    | "github-hosted-macos"
    | "persistent-host"
    | "unknown";
  runId: string;
  supervision: {
    descendants: "causally-complete" | "escaped-or-unprovable";
    eventStream: "complete" | "lost-or-unknown";
    kind: "launch-bound-event-supervisor" | "polling-only" | "unknown";
    pidGeneration: "coarse-or-unknown" | "high-resolution";
  };
  // This is a non-secret correlation value for a single ephemeral executor
  // instance. GitHub-hosted runners need not expose a provider VM identifier.
  executorInstanceId: string;
}

export interface G5BoundaryAssessment {
  cleanupBlockers: readonly [
    "external-executor-lifecycle-verification-required",
  ];
  cleanupBoundary: "not-established";
  g5Result: "not-assessed";
  trustedExecutorVerification: "blocked" | "required";
  trustedExecutorVerificationBlockers: readonly G5ExecutionBlocker[];
}

const cleanupBlockers = Object.freeze(
  [
    "external-executor-lifecycle-verification-required",
  ] as const,
);

function isNonblankString(value: unknown): value is string {
  return typeof value === "string" && /[^\s]/u.test(value);
}

function isString(value: unknown): value is string {
  return typeof value === "string";
}

function isHost(value: unknown): value is G5BoundaryFacts["host"] {
  return value === "disposable-macos-vm" ||
    value === "github-hosted-macos" ||
    value === "persistent-host" ||
    value === "unknown";
}

function isSupervisorKind(
  value: unknown,
): value is G5BoundaryFacts["supervision"]["kind"] {
  return value === "launch-bound-event-supervisor" ||
    value === "polling-only" || value === "unknown";
}

function isPIDGeneration(
  value: unknown,
): value is G5BoundaryFacts["supervision"]["pidGeneration"] {
  return value === "coarse-or-unknown" || value === "high-resolution";
}

function isEventStream(
  value: unknown,
): value is G5BoundaryFacts["supervision"]["eventStream"] {
  return value === "complete" || value === "lost-or-unknown";
}

function isDescendantOwnership(
  value: unknown,
): value is G5BoundaryFacts["supervision"]["descendants"] {
  return value === "causally-complete" || value === "escaped-or-unprovable";
}

function exactDataProperties(
  value: unknown,
  expectedKeys: readonly string[],
): Record<string, unknown> | undefined {
  if (
    typeof value !== "object" || value === null || Array.isArray(value)
  ) {
    return undefined;
  }
  const record = value as Record<string, unknown>;
  const keys = Object.keys(record);
  if (
    keys.length !== expectedKeys.length ||
    !keys.every((key) => expectedKeys.includes(key))
  ) {
    return undefined;
  }
  const copied: Record<string, unknown> = {};
  for (const key of expectedKeys) {
    if (!Object.hasOwn(record, key)) return undefined;
    copied[key] = record[key];
  }
  return copied;
}

function parseFacts(value: unknown): G5BoundaryFacts | undefined {
  const facts = exactDataProperties(value, [
    "host",
    "runId",
    "supervision",
    "executorInstanceId",
  ]);
  if (facts === undefined) return undefined;
  const supervision = exactDataProperties(facts.supervision, [
    "descendants",
    "eventStream",
    "kind",
    "pidGeneration",
  ]);
  if (
    supervision === undefined || !isHost(facts.host) ||
    !isString(facts.runId) ||
    !isString(facts.executorInstanceId) ||
    !isDescendantOwnership(supervision.descendants) ||
    !isEventStream(supervision.eventStream) ||
    !isSupervisorKind(supervision.kind) ||
    !isPIDGeneration(supervision.pidGeneration)
  ) {
    return undefined;
  }
  return {
    host: facts.host,
    runId: facts.runId,
    supervision: {
      descendants: supervision.descendants,
      eventStream: supervision.eventStream,
      kind: supervision.kind,
      pidGeneration: supervision.pidGeneration,
    },
    executorInstanceId: facts.executorInstanceId,
  };
}

function parseFactsJson(value: string): G5BoundaryFacts | undefined {
  let decoded: unknown;
  try {
    decoded = JSON.parse(value);
  } catch {
    return undefined;
  }
  const facts = parseFacts(decoded);
  if (facts === undefined || value !== canonicalFactsJson(facts)) {
    return undefined;
  }
  return facts;
}

/**
 * Accept only the single canonical spelling generated for trusted boundary
 * facts. This rejects duplicate member names, alternate escapes, whitespace,
 * and key-order ambiguity before the assessment can become `required`.
 */
function canonicalFactsJson(facts: G5BoundaryFacts): string {
  return JSON.stringify({
    host: facts.host,
    runId: facts.runId,
    supervision: {
      descendants: facts.supervision.descendants,
      eventStream: facts.supervision.eventStream,
      kind: facts.supervision.kind,
      pidGeneration: facts.supervision.pidGeneration,
    },
    executorInstanceId: facts.executorInstanceId,
  });
}

function evaluateExecutionBlockers(
  facts: G5BoundaryFacts,
): readonly G5ExecutionBlocker[] {
  if (
    !isNonblankString(facts.runId) ||
    !isNonblankString(facts.executorInstanceId)
  ) {
    return ["run-or-executor-instance-identity-absent"];
  }
  const blockers: G5ExecutionBlocker[] = [];
  if (
    facts.host !== "disposable-macos-vm" &&
    facts.host !== "github-hosted-macos"
  ) {
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

function assessment(
  blockers: readonly G5ExecutionBlocker[],
): G5BoundaryAssessment {
  return Object.freeze({
    cleanupBlockers,
    cleanupBoundary: "not-established" as const,
    g5Result: "not-assessed" as const,
    trustedExecutorVerification: blockers.length === 0
      ? "required" as const
      : "blocked" as const,
    trustedExecutorVerificationBlockers: Object.freeze([...blockers]),
  });
}

/**
 * Pure, pre-execution policy for a protected G5 run.
 *
 * It deliberately does not accept or validate external lifecycle evidence.
 * A separately trusted verifier must bind the executor lifecycle to this run
 * and instance before any final cleanup or G5 evidence claim can be made.
 *
 * `required` is not an execution authorization. It only means the supplied
 * facts have no local policy blockers and must still be independently verified
 * by the trusted executor before it launches anything.
 *
 * The input is JSON text rather than an arbitrary JavaScript object. The
 * policy must not trust caller-controlled getters or Proxy traps at this
 * boundary; non-string inputs and malformed JSON fail closed.
 */
export function assessG5ExecutionBoundary(
  factsJson: string,
): G5BoundaryAssessment {
  const parsed = typeof factsJson === "string"
    ? parseFactsJson(factsJson)
    : undefined;
  return assessment(
    parsed === undefined
      ? ["malformed-boundary-facts"]
      : evaluateExecutionBlockers(parsed),
  );
}
