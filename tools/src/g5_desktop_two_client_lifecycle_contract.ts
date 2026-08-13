// SPDX-License-Identifier: MPL-2.0

/**
 * An inert, data-only proof format for a pair of already-completed Desktop
 * lifecycle observations. This module has no effect beyond validation.
 */
export const FLOORP_G5_TWO_CLIENT_LIFECYCLE_SCHEMA =
  "floorp-g5-desktop-two-client-lifecycle-v1";

export interface G5DesktopTwoClientLifecycleAssessment {
  readonly blockers:
    | readonly []
    | readonly ["malformed-two-client-lifecycle-evidence"];
  readonly execution_authorization: "not-granted";
  readonly g5_result: "not-assessed";
  readonly lifecycle_validation: "accepted" | "rejected";
}

interface ParsedCaptureProof {
  readonly clientInstanceId: string;
  readonly executorInstanceId: string;
  readonly marionettePort: number;
  readonly pairId: string;
  readonly proofId: string;
  readonly rootPid: number;
  readonly rootProcessGeneration: string;
  readonly runId: string;
}

interface ParsedTerminationProof extends ParsedCaptureProof {
  readonly captureProofId: string;
  readonly operation: "abort" | "stop";
}

interface ParsedClientLifecycle {
  readonly captureProof: ParsedCaptureProof;
  readonly terminationProof: ParsedTerminationProof;
}

interface ParsedEvidence {
  readonly clients: readonly [ParsedClientLifecycle, ParsedClientLifecycle];
  readonly executorInstanceId: string;
  readonly pairId: string;
  readonly runId: string;
}

const EVIDENCE_KEYS = [
  "clients",
  "executorInstanceId",
  "pairId",
  "runId",
  "schemaVersion",
] as const;
const CLIENT_KEYS = ["captureProof", "terminationProof"] as const;
const CAPTURE_PROOF_KEYS = [
  "clientInstanceId",
  "descendantOwnership",
  "eventStream",
  "executorInstanceId",
  "marionettePort",
  "operation",
  "pairId",
  "pidGeneration",
  "proofId",
  "rootPid",
  "rootProcessGeneration",
  "runId",
] as const;
const TERMINATION_PROOF_KEYS = [
  "captureProofId",
  "clientInstanceId",
  "descendantOwnership",
  "eventStream",
  "executorInstanceId",
  "marionettePort",
  "marionettePortState",
  "operation",
  "operationResult",
  "ownedTree",
  "pairId",
  "pidGeneration",
  "proofId",
  "rootPid",
  "rootProcessGeneration",
  "runId",
] as const;
const OPAQUE_IDENTIFIER = /^[a-z0-9][a-z0-9._:-]{0,127}$/u;

function exactDataProperties(
  value: unknown,
  expectedKeys: readonly string[],
): Record<string, unknown> | undefined {
  if (
    typeof value !== "object" || value === null || Array.isArray(value) ||
    Object.getPrototypeOf(value) !== Object.prototype
  ) {
    return undefined;
  }
  try {
    const record = value as Record<string, unknown>;
    const keys = Reflect.ownKeys(record);
    if (
      keys.length !== expectedKeys.length ||
      !keys.every((key) =>
        typeof key === "string" && expectedKeys.includes(key)
      )
    ) {
      return undefined;
    }
    const copied: Record<string, unknown> = {};
    for (const key of expectedKeys) {
      const descriptor = Object.getOwnPropertyDescriptor(record, key);
      if (descriptor === undefined || !("value" in descriptor)) {
        return undefined;
      }
      copied[key] = descriptor.value;
    }
    return copied;
  } catch {
    return undefined;
  }
}

function isOpaqueIdentifier(value: unknown): value is string {
  return typeof value === "string" && OPAQUE_IDENTIFIER.test(value);
}

function isRootPid(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) && value > 0;
}

function isMarionettePort(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) &&
    value >= 1_024 && value <= 65_535;
}

function isHighResolutionGeneration(
  value: unknown,
  rootPid: number,
): value is string {
  return typeof value === "string" &&
    new RegExp(`^pid-${rootPid}-generation-[0-9]{9,}$`, "u").test(value);
}

function parseCaptureProof(
  value: unknown,
): ParsedCaptureProof | undefined {
  const proof = exactDataProperties(value, CAPTURE_PROOF_KEYS);
  if (
    proof === undefined || proof.operation !== "capture" ||
    proof.descendantOwnership !== "causally-complete" ||
    proof.eventStream !== "complete" ||
    proof.pidGeneration !== "high-resolution" ||
    !isOpaqueIdentifier(proof.clientInstanceId) ||
    !isOpaqueIdentifier(proof.executorInstanceId) ||
    !isOpaqueIdentifier(proof.pairId) || !isOpaqueIdentifier(proof.proofId) ||
    !isOpaqueIdentifier(proof.runId) || !isRootPid(proof.rootPid) ||
    !isMarionettePort(proof.marionettePort) ||
    !isHighResolutionGeneration(proof.rootProcessGeneration, proof.rootPid)
  ) {
    return undefined;
  }
  return {
    clientInstanceId: proof.clientInstanceId,
    executorInstanceId: proof.executorInstanceId,
    marionettePort: proof.marionettePort,
    pairId: proof.pairId,
    proofId: proof.proofId,
    rootPid: proof.rootPid,
    rootProcessGeneration: proof.rootProcessGeneration,
    runId: proof.runId,
  };
}

function parseTerminationProof(
  value: unknown,
): ParsedTerminationProof | undefined {
  const proof = exactDataProperties(value, TERMINATION_PROOF_KEYS);
  if (
    proof === undefined ||
    (proof.operation !== "abort" && proof.operation !== "stop") ||
    proof.operationResult !==
      (proof.operation === "abort" ? "aborted" : "stopped") ||
    proof.ownedTree !== "absent" ||
    proof.marionettePortState !== "absent" ||
    proof.descendantOwnership !== "causally-complete" ||
    proof.eventStream !== "complete" ||
    proof.pidGeneration !== "high-resolution" ||
    !isOpaqueIdentifier(proof.captureProofId) ||
    !isOpaqueIdentifier(proof.clientInstanceId) ||
    !isOpaqueIdentifier(proof.executorInstanceId) ||
    !isOpaqueIdentifier(proof.pairId) || !isOpaqueIdentifier(proof.proofId) ||
    !isOpaqueIdentifier(proof.runId) || !isRootPid(proof.rootPid) ||
    !isMarionettePort(proof.marionettePort) ||
    !isHighResolutionGeneration(proof.rootProcessGeneration, proof.rootPid)
  ) {
    return undefined;
  }
  return {
    captureProofId: proof.captureProofId,
    clientInstanceId: proof.clientInstanceId,
    executorInstanceId: proof.executorInstanceId,
    marionettePort: proof.marionettePort,
    pairId: proof.pairId,
    operation: proof.operation,
    proofId: proof.proofId,
    rootPid: proof.rootPid,
    rootProcessGeneration: proof.rootProcessGeneration,
    runId: proof.runId,
  };
}

function parseClientLifecycle(
  value: unknown,
): ParsedClientLifecycle | undefined {
  const client = exactDataProperties(value, CLIENT_KEYS);
  if (client === undefined) return undefined;
  const captureProof = parseCaptureProof(client.captureProof);
  const terminationProof = parseTerminationProof(client.terminationProof);
  if (
    captureProof === undefined || terminationProof === undefined ||
    terminationProof.captureProofId !== captureProof.proofId ||
    terminationProof.clientInstanceId !== captureProof.clientInstanceId ||
    terminationProof.executorInstanceId !== captureProof.executorInstanceId ||
    terminationProof.marionettePort !== captureProof.marionettePort ||
    terminationProof.pairId !== captureProof.pairId ||
    terminationProof.rootPid !== captureProof.rootPid ||
    terminationProof.rootProcessGeneration !==
      captureProof.rootProcessGeneration ||
    terminationProof.runId !== captureProof.runId ||
    terminationProof.proofId === captureProof.proofId
  ) {
    return undefined;
  }
  return { captureProof, terminationProof };
}

function parseEvidence(value: unknown): ParsedEvidence | undefined {
  const evidence = exactDataProperties(value, EVIDENCE_KEYS);
  if (
    evidence === undefined ||
    evidence.schemaVersion !== FLOORP_G5_TWO_CLIENT_LIFECYCLE_SCHEMA ||
    !isOpaqueIdentifier(evidence.executorInstanceId) ||
    !isOpaqueIdentifier(evidence.pairId) ||
    !isOpaqueIdentifier(evidence.runId) ||
    !Array.isArray(evidence.clients) || evidence.clients.length !== 2
  ) {
    return undefined;
  }
  const first = parseClientLifecycle(evidence.clients[0]);
  const second = parseClientLifecycle(evidence.clients[1]);
  if (first === undefined || second === undefined) return undefined;

  const firstCapture = first.captureProof;
  const secondCapture = second.captureProof;
  if (
    firstCapture.clientInstanceId >= secondCapture.clientInstanceId ||
    firstCapture.clientInstanceId === secondCapture.clientInstanceId ||
    firstCapture.rootPid === secondCapture.rootPid ||
    firstCapture.marionettePort === secondCapture.marionettePort ||
    firstCapture.rootProcessGeneration === secondCapture.rootProcessGeneration
  ) {
    return undefined;
  }

  const proofIds = new Set([
    first.captureProof.proofId,
    first.terminationProof.proofId,
    second.captureProof.proofId,
    second.terminationProof.proofId,
  ]);
  if (proofIds.size !== 4) return undefined;

  for (const client of [first, second]) {
    for (const proof of [client.captureProof, client.terminationProof]) {
      if (
        proof.executorInstanceId !== evidence.executorInstanceId ||
        proof.pairId !== evidence.pairId || proof.runId !== evidence.runId
      ) {
        return undefined;
      }
    }
  }
  return {
    clients: [first, second],
    executorInstanceId: evidence.executorInstanceId,
    pairId: evidence.pairId,
    runId: evidence.runId,
  };
}

function canonicalCaptureProof(
  proof: ParsedCaptureProof,
): Record<string, unknown> {
  return {
    clientInstanceId: proof.clientInstanceId,
    descendantOwnership: "causally-complete",
    eventStream: "complete",
    executorInstanceId: proof.executorInstanceId,
    marionettePort: proof.marionettePort,
    operation: "capture",
    pairId: proof.pairId,
    pidGeneration: "high-resolution",
    proofId: proof.proofId,
    rootPid: proof.rootPid,
    rootProcessGeneration: proof.rootProcessGeneration,
    runId: proof.runId,
  };
}

function canonicalTerminationProof(
  proof: ParsedTerminationProof,
): Record<string, unknown> {
  return {
    captureProofId: proof.captureProofId,
    clientInstanceId: proof.clientInstanceId,
    descendantOwnership: "causally-complete",
    eventStream: "complete",
    executorInstanceId: proof.executorInstanceId,
    marionettePort: proof.marionettePort,
    marionettePortState: "absent",
    operation: proof.operation,
    operationResult: proof.operation === "abort" ? "aborted" : "stopped",
    ownedTree: "absent",
    pairId: proof.pairId,
    pidGeneration: "high-resolution",
    proofId: proof.proofId,
    rootPid: proof.rootPid,
    rootProcessGeneration: proof.rootProcessGeneration,
    runId: proof.runId,
  };
}

function canonicalEvidenceJson(evidence: ParsedEvidence): string {
  return JSON.stringify({
    clients: evidence.clients.map((client) => ({
      captureProof: canonicalCaptureProof(client.captureProof),
      terminationProof: canonicalTerminationProof(client.terminationProof),
    })),
    executorInstanceId: evidence.executorInstanceId,
    pairId: evidence.pairId,
    runId: evidence.runId,
    schemaVersion: FLOORP_G5_TWO_CLIENT_LIFECYCLE_SCHEMA,
  });
}

function accepted(): G5DesktopTwoClientLifecycleAssessment {
  return Object.freeze({
    blockers: Object.freeze([]) as readonly [],
    execution_authorization: "not-granted" as const,
    g5_result: "not-assessed" as const,
    lifecycle_validation: "accepted" as const,
  });
}

function rejected(): G5DesktopTwoClientLifecycleAssessment {
  return Object.freeze({
    blockers: Object.freeze([
      "malformed-two-client-lifecycle-evidence",
    ]) as readonly ["malformed-two-client-lifecycle-evidence"],
    execution_authorization: "not-granted" as const,
    g5_result: "not-assessed" as const,
    lifecycle_validation: "rejected" as const,
  });
}

/**
 * Accepts only canonical, data-only lifecycle evidence for two distinct
 * clients. Acceptance does not authorize or perform any operation.
 */
export function assessG5DesktopTwoClientLifecycleEvidence(
  evidenceJson: unknown,
): G5DesktopTwoClientLifecycleAssessment {
  if (typeof evidenceJson !== "string") return rejected();
  try {
    const parsed = parseEvidence(JSON.parse(evidenceJson));
    return parsed === undefined ||
        evidenceJson !== canonicalEvidenceJson(parsed)
      ? rejected()
      : accepted();
  } catch {
    return rejected();
  }
}
