// SPDX-License-Identifier: MPL-2.0

import {
  assessG5DesktopTwoClientLifecycleEvidence,
  FLOORP_G5_TWO_CLIENT_LIFECYCLE_SCHEMA,
} from "./g5_desktop_two_client_lifecycle_contract.ts";

export const G5_DESKTOP_TWO_CLIENT_OFFLINE_FAKE_ONLY =
  "offline-fictional-fixture-v2" as const;

export interface G5DesktopOfflineFakeResult {
  readonly evidenceJson: string;
  readonly execution_authorization: "not-granted";
  readonly g5_result: "not-assessed";
}

export interface G5DesktopOfflineFakeTwoClientExecutor {
  consumeFixture(): G5DesktopOfflineFakeResult;
}

interface ParsedClientFixture {
  readonly captureProofId: string;
  readonly clientInstanceId: string;
  readonly port: number;
  readonly profileToken: string;
  readonly rootPid: number;
  readonly rootProcessGeneration: string;
  readonly terminationProofId: string;
}

interface ParsedFixture {
  readonly clients: readonly [ParsedClientFixture, ParsedClientFixture];
  readonly executorInstanceId: string;
  readonly pairId: string;
  readonly runId: string;
}

const FIXTURE_SCHEMA = "floorp-g5-desktop-two-client-offline-fixture-v2";
const OPAQUE_IDENTIFIER = /^[a-z0-9][a-z0-9._:-]{0,127}$/iu;
const HIGH_RESOLUTION_GENERATION = /^pid-([1-9][0-9]*)-generation-[0-9]{9,}$/u;

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
  if (keys.length !== expectedKeys.length) return undefined;
  for (const key of keys) {
    if (!expectedKeys.includes(key)) return undefined;
  }
  const copied: Record<string, unknown> = {};
  for (const key of expectedKeys) {
    if (!Object.hasOwn(record, key)) return undefined;
    copied[key] = record[key];
  }
  return copied;
}

function isOpaqueIdentifier(value: unknown): value is string {
  return typeof value === "string" && OPAQUE_IDENTIFIER.test(value);
}

function isRootPid(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) && value > 0;
}

function isPort(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) &&
    value >= 1_024 && value <= 65_535;
}

function isHighResolutionGeneration(
  value: unknown,
  rootPid: number,
): value is string {
  if (typeof value !== "string") return false;
  const match = HIGH_RESOLUTION_GENERATION.exec(value);
  return match !== null && match[1] === String(rootPid);
}

function parseClientFixture(value: unknown): ParsedClientFixture | undefined {
  const client = exactDataProperties(value, [
    "captureProofId",
    "clientInstanceId",
    "fictional",
    "port",
    "profileToken",
    "rootPid",
    "rootProcessGeneration",
    "terminationProofId",
  ]);
  if (
    client === undefined || client.fictional !== true ||
    !isOpaqueIdentifier(client.captureProofId) ||
    !isOpaqueIdentifier(client.clientInstanceId) ||
    !isPort(client.port) || !isOpaqueIdentifier(client.profileToken) ||
    !isRootPid(client.rootPid) ||
    !isHighResolutionGeneration(
      client.rootProcessGeneration,
      client.rootPid,
    ) || !isOpaqueIdentifier(client.terminationProofId)
  ) {
    return undefined;
  }
  return {
    captureProofId: client.captureProofId,
    clientInstanceId: client.clientInstanceId,
    port: client.port,
    profileToken: client.profileToken,
    rootPid: client.rootPid,
    rootProcessGeneration: client.rootProcessGeneration,
    terminationProofId: client.terminationProofId,
  };
}

function parseFixture(value: unknown): ParsedFixture | undefined {
  const fixture = exactDataProperties(value, [
    "clients",
    "executorInstanceId",
    "mode",
    "pairId",
    "runId",
    "schemaVersion",
  ]);
  if (
    fixture === undefined ||
    fixture.mode !== G5_DESKTOP_TWO_CLIENT_OFFLINE_FAKE_ONLY ||
    fixture.schemaVersion !== FIXTURE_SCHEMA ||
    !isOpaqueIdentifier(fixture.executorInstanceId) ||
    !isOpaqueIdentifier(fixture.pairId) || !isOpaqueIdentifier(fixture.runId) ||
    !Array.isArray(fixture.clients) || fixture.clients.length !== 2
  ) {
    return undefined;
  }
  const first = parseClientFixture(fixture.clients[0]);
  const second = parseClientFixture(fixture.clients[1]);
  if (first === undefined || second === undefined) return undefined;
  const strings = [
    fixture.executorInstanceId,
    fixture.pairId,
    fixture.runId,
    first.captureProofId,
    first.clientInstanceId,
    first.profileToken,
    first.rootProcessGeneration,
    first.terminationProofId,
    second.captureProofId,
    second.clientInstanceId,
    second.profileToken,
    second.rootProcessGeneration,
    second.terminationProofId,
  ];
  const ports = [first.port, second.port];
  const rootPids = [first.rootPid, second.rootPid];
  if (
    first.clientInstanceId >= second.clientInstanceId ||
    new Set(strings).size !== strings.length ||
    new Set(ports).size !== ports.length ||
    new Set(rootPids).size !== rootPids.length
  ) {
    return undefined;
  }
  return {
    clients: [first, second],
    executorInstanceId: fixture.executorInstanceId,
    pairId: fixture.pairId,
    runId: fixture.runId,
  };
}

function canonicalClientFixture(
  client: ParsedClientFixture,
): Record<string, unknown> {
  return {
    captureProofId: client.captureProofId,
    clientInstanceId: client.clientInstanceId,
    fictional: true,
    port: client.port,
    profileToken: client.profileToken,
    rootPid: client.rootPid,
    rootProcessGeneration: client.rootProcessGeneration,
    terminationProofId: client.terminationProofId,
  };
}

function canonicalFixtureJson(fixture: ParsedFixture): string {
  return JSON.stringify({
    clients: [
      canonicalClientFixture(fixture.clients[0]),
      canonicalClientFixture(fixture.clients[1]),
    ],
    executorInstanceId: fixture.executorInstanceId,
    mode: G5_DESKTOP_TWO_CLIENT_OFFLINE_FAKE_ONLY,
    pairId: fixture.pairId,
    runId: fixture.runId,
    schemaVersion: FIXTURE_SCHEMA,
  });
}

function parseCanonicalFixtureJson(value: unknown): ParsedFixture | undefined {
  if (typeof value !== "string") return undefined;
  try {
    const fixture = parseFixture(JSON.parse(value));
    return fixture === undefined || value !== canonicalFixtureJson(fixture)
      ? undefined
      : fixture;
  } catch {
    return undefined;
  }
}

function canonicalClientLifecycle(
  fixture: ParsedFixture,
  client: ParsedClientFixture,
): Record<string, unknown> {
  return {
    captureProof: {
      clientInstanceId: client.clientInstanceId,
      descendantOwnership: "causally-complete",
      eventStream: "complete",
      executorInstanceId: fixture.executorInstanceId,
      marionettePort: client.port,
      operation: "capture",
      pairId: fixture.pairId,
      pidGeneration: "high-resolution",
      proofId: client.captureProofId,
      rootPid: client.rootPid,
      rootProcessGeneration: client.rootProcessGeneration,
      runId: fixture.runId,
    },
    terminationProof: {
      captureProofId: client.captureProofId,
      clientInstanceId: client.clientInstanceId,
      descendantOwnership: "causally-complete",
      eventStream: "complete",
      executorInstanceId: fixture.executorInstanceId,
      marionettePort: client.port,
      marionettePortState: "absent",
      operation: "stop",
      operationResult: "stopped",
      ownedTree: "absent",
      pairId: fixture.pairId,
      pidGeneration: "high-resolution",
      proofId: client.terminationProofId,
      rootPid: client.rootPid,
      rootProcessGeneration: client.rootProcessGeneration,
      runId: fixture.runId,
    },
  };
}

function canonicalLifecycleEvidenceJson(fixture: ParsedFixture): string {
  return JSON.stringify({
    clients: [
      canonicalClientLifecycle(fixture, fixture.clients[0]),
      canonicalClientLifecycle(fixture, fixture.clients[1]),
    ],
    executorInstanceId: fixture.executorInstanceId,
    pairId: fixture.pairId,
    runId: fixture.runId,
    schemaVersion: FLOORP_G5_TWO_CLIENT_LIFECYCLE_SCHEMA,
  });
}

/**
 * Accepts canonical JSON only, validates a fictional two-client fixture at
 * construction, and exposes one irreversible data-only consumption step.
 */
export function createOfflineFakeG5DesktopTwoClientExecutor(
  fixtureJson: unknown,
): G5DesktopOfflineFakeTwoClientExecutor {
  const fixture = parseCanonicalFixtureJson(fixtureJson);
  if (fixture === undefined) {
    throw new Error("G5 offline fixture input was rejected");
  }
  const evidenceJson = canonicalLifecycleEvidenceJson(fixture);
  const lifecycle = assessG5DesktopTwoClientLifecycleEvidence(evidenceJson);
  if (
    lifecycle.lifecycle_validation !== "accepted" ||
    lifecycle.execution_authorization !== "not-granted" ||
    lifecycle.g5_result !== "not-assessed"
  ) {
    throw new Error("G5 offline fixture lifecycle data was rejected");
  }
  let consumed = false;
  return Object.freeze({
    consumeFixture(): G5DesktopOfflineFakeResult {
      if (consumed) throw new Error("G5 offline fixture is single-use");
      consumed = true;
      return Object.freeze({
        evidenceJson,
        execution_authorization: "not-granted" as const,
        g5_result: "not-assessed" as const,
      });
    },
  });
}
