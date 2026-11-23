// SPDX-License-Identifier: MPL-2.0

import type {
  M3cAdmissionState,
  M3cBoundaryExpectation,
  M3cBrowserDriverResult,
  M3cDurableMarker,
  M3cResolvedBoundaryExpectation,
} from "./m3c_signed_process_recovery_types.ts";

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

export function selectM3cProcessScenarios<T extends { id: string }>(
  plan: readonly T[],
  selectedIds: readonly string[],
): T[] {
  if (selectedIds.length === 0) return [...plan];
  const selected = new Set(selectedIds);
  if (selected.size !== selectedIds.length) {
    throw new Error("duplicate M3c process scenario selection");
  }
  const available = new Set(plan.map((scenario) => scenario.id));
  const unknown = selectedIds.filter((id) => !available.has(id));
  if (unknown.length > 0) {
    throw new Error(`unknown M3c process scenario: ${unknown.join(", ")}`);
  }
  return plan.filter((scenario) => selected.has(scenario.id));
}

export function canonicalJson(value: unknown): string {
  if (Array.isArray(value)) {
    return `[${value.map((entry) => canonicalJson(entry)).join(",")}]`;
  }
  if (isRecord(value)) {
    return `{${
      Object.keys(value).sort().map((key) =>
        `${JSON.stringify(key)}:${canonicalJson(value[key])}`
      ).join(",")
    }}`;
  }
  return JSON.stringify(value) ?? "null";
}

export function resolveM3cBoundaryExpectation(
  expectation: M3cBoundaryExpectation,
  packageDigests: Record<string, string>,
  recoveryTransactionId?: string,
): M3cResolvedBoundaryExpectation {
  const targetDigest = packageDigests[expectation.targetPackageId];
  if (!targetDigest || !/^[0-9a-f]{64}$/.test(targetDigest)) {
    throw new Error("boundary target does not identify a signed fixture");
  }
  const transactionId = expectation.transactionId === "captured"
    ? recoveryTransactionId
    : expectation.transactionId;
  if (!transactionId) throw new Error("recovery transaction was not captured");
  if (
    expectation.transactionId === "captured" &&
    !/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/.test(
      transactionId,
    )
  ) throw new Error("captured recovery transaction is malformed");
  return {
    ...expectation,
    transactionId,
    targetVersion: targetDigest,
    targetDigest,
  };
}

export function matchesM3cBoundary(
  expectation: M3cResolvedBoundaryExpectation,
  snapshot: Record<string, unknown>,
): boolean {
  const isRecord = (value: unknown): value is Record<string, unknown> =>
    typeof value === "object" && value !== null && !Array.isArray(value);
  const pending = isRecord(snapshot.pending) ? snapshot.pending : null;
  const attempt = isRecord(snapshot.attempt) ? snapshot.attempt : null;
  if (expectation.boundary === "committed_pending_publish") {
    const outcome = isRecord(snapshot.lastOutcome)
      ? snapshot.lastOutcome
      : null;
    const request = isRecord(outcome?.request) ? outcome.request : null;
    return outcome?.status === expectation.boundary &&
      outcome.txId === expectation.transactionId &&
      request?.op === expectation.operation &&
      request.target === expectation.targetVersion &&
      snapshot.selectedVersion === expectation.targetVersion &&
      snapshot.selectedDigest === expectation.targetDigest;
  }
  if (!pending) return false;
  const transactionMatches = expectation.transactionId === "capture"
    ? typeof pending.txId === "string" &&
      /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/.test(
        pending.txId,
      ) && attempt?.originTxId === pending.txId
    : pending.txId === expectation.transactionId;
  if (!transactionMatches) return false;
  if (
    expectation.boundary === "fallback_pending_before_mark" ||
    expectation.boundary === "fallback_attempt_marked"
  ) {
    const attempted = Array.isArray(attempt?.attemptedDigests)
      ? attempt.attemptedDigests
      : [];
    const marked = attempted.includes(expectation.targetDigest);
    const request = isRecord(attempt?.request) ? attempt.request : null;
    const pendingRequest = isRecord(pending.request) ? pending.request : null;
    return pending.op === "startup_recovery" &&
      request?.op === "startup_recovery" &&
      pendingRequest?.op === "startup_recovery" &&
      pendingRequest.recoveryEpoch === pending.txId &&
      attempt?.recoveryEpoch === pending.txId &&
      request?.recoveryEpoch === pending.txId &&
      attempt?.originTxId === pending.txId &&
      pending.phase === "OLD_STOPPED" &&
      pending.targetVersion === expectation.targetVersion &&
      pending.targetDigest === expectation.targetDigest &&
      (expectation.attempted === undefined ||
        marked === expectation.attempted) &&
      (expectation.boundary === "fallback_attempt_marked") === marked;
  }
  return pending.op === expectation.operation &&
    pending.phase === expectation.boundary &&
    pending.targetVersion === expectation.targetVersion &&
    pending.targetDigest === expectation.targetDigest &&
    (expectation.attempted === undefined ||
      (Array.isArray(attempt?.attemptedDigests) &&
          attempt.attemptedDigests.includes(expectation.targetDigest)) ===
        expectation.attempted) &&
    (expectation.desiredEnabled === undefined ||
      snapshot.desiredEnabled === expectation.desiredEnabled);
}

export function assertM3cAdmissionUnchanged(
  before: M3cAdmissionState,
  after: M3cAdmissionState,
): void {
  if (
    before.journalSha256 !== after.journalSha256 ||
    before.journalByteLength !== after.journalByteLength ||
    before.revision !== after.revision ||
    before.evaluationCount !== after.evaluationCount ||
    before.highWaterFingerprint !== after.highWaterFingerprint
  ) throw new Error("inert package admission changed durable state");
}

export function parseM3cDurableMarker(
  raw: string,
  expected: {
    runNonce: string;
    launchId: number;
    boundary: M3cDurableMarker["boundary"];
    testId: string;
    featureId: string;
  },
): M3cDurableMarker {
  const value: unknown = JSON.parse(raw);
  if (
    !isRecord(value) || !isRecord(value.snapshot) ||
    !isRecord(value.journal)
  ) {
    throw new Error("invalid M3c durable marker");
  }
  const lifecycle = value.journal.lifecycle;
  if (!isRecord(lifecycle)) throw new Error("marker journal lacks lifecycle");
  if (
    value.schemaVersion !== 1 || value.runNonce !== expected.runNonce ||
    value.launchId !== expected.launchId ||
    value.boundary !== expected.boundary || value.testId !== expected.testId ||
    value.featureId !== expected.featureId ||
    !Number.isSafeInteger(value.evaluationCount) ||
    (value.evaluationCount as number) < 0 ||
    typeof value.journalSha256 !== "string" ||
    !/^[0-9a-f]{64}$/.test(value.journalSha256) ||
    !Number.isSafeInteger(value.journalByteLength) ||
    (value.journalByteLength as number) < 1 ||
    (value.journalByteLength as number) > 262_144 ||
    value.snapshot.featureId !== expected.featureId ||
    value.journal.featureId !== expected.featureId ||
    canonicalJson(lifecycle) !== canonicalJson(value.snapshot)
  ) throw new Error("M3c durable marker identity or journal mismatch");
  return value as unknown as M3cDurableMarker;
}

export function parseM3cBrowserDriverResult(
  raw: string,
  expected: {
    runNonce: string;
    launchId: number;
    buildId: string;
    testId: string;
    featureId: string;
  },
): M3cBrowserDriverResult {
  const value: unknown = JSON.parse(raw);
  if (
    !isRecord(value) || !isRecord(value.diagnostics) ||
    !isRecord(value.journal) || !isRecord(value.outcomes) ||
    !(value.cleanupDiagnostics === null ||
      isRecord(value.cleanupDiagnostics)) ||
    !(value.windowObservation === null || isRecord(value.windowObservation))
  ) {
    throw new Error("invalid M3c browser result");
  }
  if (
    isRecord(value.cleanupDiagnostics) &&
    (typeof value.cleanupDiagnostics.proven !== "boolean" ||
      !Array.isArray(value.cleanupDiagnostics.evidence) ||
      value.cleanupDiagnostics.evidence.length > 16 ||
      value.cleanupDiagnostics.evidence.some((entry) =>
        typeof entry !== "string" || entry.length > 96
      ))
  ) throw new Error("invalid detached cleanup diagnostics");
  const lifecycle = isRecord(value.journal.lifecycle)
    ? value.journal.lifecycle
    : null;
  if (
    value.schemaVersion !== 1 || value.runNonce !== expected.runNonce ||
    value.launchId !== expected.launchId ||
    value.buildId !== expected.buildId ||
    value.testId !== expected.testId ||
    value.featureId !== expected.featureId ||
    value.journal.featureId !== expected.featureId ||
    lifecycle?.featureId !== expected.featureId ||
    !/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/.test(
      value.testId,
    ) || typeof value.featureId !== "string" ||
    typeof value.journalSha256 !== "string" ||
    !/^[0-9a-f]{64}$/.test(value.journalSha256) ||
    !Number.isSafeInteger(value.journalByteLength) ||
    (value.journalByteLength as number) < 1 ||
    (value.journalByteLength as number) > 262_144 ||
    !Number.isSafeInteger(value.evaluationCountBefore) ||
    (value.evaluationCountBefore as number) < 0 ||
    !Number.isSafeInteger(value.evaluationCount) ||
    (value.evaluationCount as number) < 0
  ) throw new Error("M3c browser result identity or bounds mismatch");
  return value as unknown as M3cBrowserDriverResult;
}
