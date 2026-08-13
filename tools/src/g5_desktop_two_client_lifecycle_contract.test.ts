// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals } from "@std/assert";
import {
  assessG5DesktopTwoClientLifecycleEvidence,
  FLOORP_G5_TWO_CLIENT_LIFECYCLE_SCHEMA,
} from "./g5_desktop_two_client_lifecycle_contract.ts";

const RUN_ID = "g5-run-20260814-001";
const EXECUTOR_INSTANCE_ID = "g5-executor-20260814-001";
const PAIR_ID = "g5-pair-20260814-001";

type MutableRecord = Record<string, unknown>;

function lifecycle(
  clientInstanceId: string,
  rootPid: number,
  marionettePort: number,
  rootProcessGeneration: string,
  captureProofId: string,
  terminationProofId: string,
): MutableRecord {
  return {
    captureProof: {
      clientInstanceId,
      descendantOwnership: "causally-complete",
      eventStream: "complete",
      executorInstanceId: EXECUTOR_INSTANCE_ID,
      marionettePort,
      operation: "capture",
      pairId: PAIR_ID,
      pidGeneration: "high-resolution",
      proofId: captureProofId,
      rootPid,
      rootProcessGeneration,
      runId: RUN_ID,
    },
    terminationProof: {
      captureProofId,
      clientInstanceId,
      descendantOwnership: "causally-complete",
      eventStream: "complete",
      executorInstanceId: EXECUTOR_INSTANCE_ID,
      marionettePort,
      marionettePortState: "absent",
      operation: "stop",
      operationResult: "stopped",
      ownedTree: "absent",
      pairId: PAIR_ID,
      pidGeneration: "high-resolution",
      proofId: terminationProofId,
      rootPid,
      rootProcessGeneration,
      runId: RUN_ID,
    },
  };
}

function validEvidence(): MutableRecord {
  return {
    clients: [
      lifecycle(
        "g5-client-a",
        4_201,
        28_291,
        "pid-4201-generation-987654321",
        "g5-capture-proof-a",
        "g5-termination-proof-a",
      ),
      lifecycle(
        "g5-client-b",
        4_202,
        28_292,
        "pid-4202-generation-987654322",
        "g5-capture-proof-b",
        "g5-termination-proof-b",
      ),
    ],
    executorInstanceId: EXECUTOR_INSTANCE_ID,
    pairId: PAIR_ID,
    runId: RUN_ID,
    schemaVersion: FLOORP_G5_TWO_CLIENT_LIFECYCLE_SCHEMA,
  };
}

function clientAt(evidence: MutableRecord, index: number): MutableRecord {
  return (evidence.clients as MutableRecord[])[index];
}

function captureAt(evidence: MutableRecord, index: number): MutableRecord {
  return clientAt(evidence, index).captureProof as MutableRecord;
}

function terminationAt(evidence: MutableRecord, index: number): MutableRecord {
  return clientAt(evidence, index).terminationProof as MutableRecord;
}

function assess(evidence: MutableRecord) {
  return assessG5DesktopTwoClientLifecycleEvidence(JSON.stringify(evidence));
}

function assertRejected(evidence: MutableRecord) {
  const assessment = assess(evidence);
  assertEquals(assessment.lifecycle_validation, "rejected");
  assertEquals(assessment.blockers, [
    "malformed-two-client-lifecycle-evidence",
  ]);
  assertEquals(assessment.execution_authorization, "not-granted");
  assertEquals(assessment.g5_result, "not-assessed");
}

Deno.test("G5 two-client lifecycle accepts complete distinct pair evidence without authorizing execution", () => {
  const assessment = assess(validEvidence());

  assertEquals(assessment.lifecycle_validation, "accepted");
  assertEquals(assessment.blockers, []);
  assertEquals(assessment.execution_authorization, "not-granted");
  assertEquals(assessment.g5_result, "not-assessed");
  assert(Object.isFrozen(assessment));
  assert(Object.isFrozen(assessment.blockers));
});

Deno.test("G5 two-client lifecycle rejects duplicate client identities, PIDs, ports, and generations", () => {
  const duplicateClientId = validEvidence();
  captureAt(duplicateClientId, 1).clientInstanceId = "g5-client-a";
  terminationAt(duplicateClientId, 1).clientInstanceId = "g5-client-a";

  const duplicateRootPid = validEvidence();
  captureAt(duplicateRootPid, 1).rootPid = 4_201;
  terminationAt(duplicateRootPid, 1).rootPid = 4_201;
  captureAt(duplicateRootPid, 1).rootProcessGeneration =
    "pid-4201-generation-987654321";
  terminationAt(duplicateRootPid, 1).rootProcessGeneration =
    "pid-4201-generation-987654321";

  const duplicatePort = validEvidence();
  captureAt(duplicatePort, 1).marionettePort = 28_291;
  terminationAt(duplicatePort, 1).marionettePort = 28_291;

  for (const evidence of [duplicateClientId, duplicateRootPid, duplicatePort]) {
    assertRejected(evidence);
  }
});

Deno.test("G5 two-client lifecycle rejects cross-pair binding and proof reuse", () => {
  const foreignPair = validEvidence();
  terminationAt(foreignPair, 1).pairId = "g5-pair-foreign";

  const foreignCaptureReference = validEvidence();
  terminationAt(foreignCaptureReference, 1).captureProofId =
    "g5-capture-proof-a";

  const reusedProof = validEvidence();
  terminationAt(reusedProof, 1).proofId = "g5-capture-proof-b";

  const reusedTerminationProof = validEvidence();
  terminationAt(reusedTerminationProof, 1).proofId = "g5-termination-proof-a";

  for (
    const evidence of [
      foreignPair,
      foreignCaptureReference,
      reusedProof,
      reusedTerminationProof,
    ]
  ) {
    assertRejected(evidence);
  }
});

Deno.test("G5 two-client lifecycle rejects malformed and incomplete termination proof", () => {
  const residualTree = validEvidence();
  terminationAt(residualTree, 0).ownedTree = "present";

  const residualPort = validEvidence();
  terminationAt(residualPort, 0).marionettePortState = "present";

  const missingEventStream = validEvidence();
  delete terminationAt(missingEventStream, 0).eventStream;

  const extraKey = validEvidence();
  terminationAt(extraKey, 0).recoveryCommand = "forbidden";

  const mismatchedGeneration = validEvidence();
  terminationAt(mismatchedGeneration, 0).rootProcessGeneration =
    "pid-4201-generation-987654399";

  const mismatchedResult = validEvidence();
  terminationAt(mismatchedResult, 0).operation = "abort";

  for (
    const evidence of [
      residualTree,
      residualPort,
      missingEventStream,
      extraKey,
      mismatchedGeneration,
      mismatchedResult,
    ]
  ) {
    assertRejected(evidence);
  }
});

Deno.test("G5 two-client lifecycle accepts a complete abort proof only with its exact result", () => {
  const evidence = validEvidence();
  terminationAt(evidence, 1).operation = "abort";
  terminationAt(evidence, 1).operationResult = "aborted";

  const assessment = assess(evidence);

  assertEquals(assessment.lifecycle_validation, "accepted");
  assertEquals(assessment.execution_authorization, "not-granted");
  assertEquals(assessment.g5_result, "not-assessed");
});

Deno.test("G5 two-client lifecycle requires canonical JSON and a canonical client ordering", () => {
  const canonical = JSON.stringify(validEvidence());
  const duplicateRunId = canonical.replace(
    `"runId":"${RUN_ID}"`,
    `"runId":"other","runId":"${RUN_ID}"`,
  );
  const alternateEscape = canonical.replace(
    '"operation":"capture"',
    '"operation":"cap\\u0074ure"',
  );
  const reorderedClients = validEvidence();
  (reorderedClients.clients as MutableRecord[]).reverse();

  for (
    const evidenceJson of [
      canonical.replace('"clients":', '"clients" :'),
      duplicateRunId,
      alternateEscape,
      JSON.stringify(reorderedClients),
    ]
  ) {
    const assessment = assessG5DesktopTwoClientLifecycleEvidence(evidenceJson);
    assertEquals(assessment.lifecycle_validation, "rejected");
    assertEquals(assessment.execution_authorization, "not-granted");
    assertEquals(assessment.g5_result, "not-assessed");
  }
});

Deno.test("G5 two-client lifecycle rejects non-string, getter, and Proxy inputs without observing them", () => {
  let getterTouched = false;
  const getterInput = Object.create(null) as MutableRecord;
  Object.defineProperty(getterInput, "evidence", {
    enumerable: true,
    get() {
      getterTouched = true;
      return JSON.stringify(validEvidence());
    },
  });
  assertEquals(
    assessG5DesktopTwoClientLifecycleEvidence(getterInput).lifecycle_validation,
    "rejected",
  );
  assertEquals(getterTouched, false);

  let proxyTouched = false;
  const { proxy, revoke } = Proxy.revocable(validEvidence(), {
    get() {
      proxyTouched = true;
      return undefined;
    },
    ownKeys() {
      proxyTouched = true;
      return [];
    },
  });
  revoke();
  assertEquals(
    assessG5DesktopTwoClientLifecycleEvidence(proxy).lifecycle_validation,
    "rejected",
  );
  assertEquals(proxyTouched, false);
});

Deno.test("G5 two-client lifecycle contract is pure and has no execution-capable surface", async () => {
  const source = await Deno.readTextFile(
    new URL("./g5_desktop_two_client_lifecycle_contract.ts", import.meta.url),
  );
  for (
    const forbidden of [
      "Deno.Command",
      ".spawn(",
      "Deno.env",
      "Deno.connect",
      "Deno.listen",
      "fetch(",
      "WebSocket",
      "child_process",
      "Xcode",
      "FxA",
      "Sync",
      "credential",
      "test-accounts",
    ]
  ) {
    assertEquals(source.includes(forbidden), false, forbidden);
  }
});
