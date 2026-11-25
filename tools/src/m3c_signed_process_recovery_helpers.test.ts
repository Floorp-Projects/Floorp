// SPDX-License-Identifier: MPL-2.0

import {
  assertM3cAdmissionUnchanged,
  launchM3cBrowserWithEvidence,
  matchesM3cBoundary,
  parseM3cBrowserDriverResult,
  parseM3cDurableMarker,
  resolveM3cBoundaryExpectation,
  selectM3cProcessScenarios,
} from "./m3c_signed_process_recovery_helpers.ts";
import {
  buildM3cProcessPlan,
  createAuthorization,
  m3cBrowserDriverScript,
} from "./m3c_signed_process_recovery.ts";
import { verifySignedFeaturePackage } from "../../browser-features/modules/modules/feature-host/package/FeaturePackageVerifier.ts";
import { waitForDurableMarker } from "./hotfix_process_recovery.ts";
import type {
  M3cBoundaryExpectation,
  M3cProcessLaunchFailureEvidence,
  M3cResolvedBoundaryExpectation,
} from "./m3c_signed_process_recovery_types.ts";

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

function reject(action: () => unknown): boolean {
  try {
    action();
    return false;
  } catch {
    return true;
  }
}

const uuid = "11111111-2222-4333-8444-555555555555";

Deno.test("M3c boundary resolves fixture ID to internal envelope hash", () => {
  const digest = "a".repeat(64);
  const expectation: M3cBoundaryExpectation = {
    boundary: "PREPARED",
    transactionId: "tx-1",
    operation: "update",
    targetPackageId: "display-v2",
    desiredEnabled: true,
  };
  const resolved = resolveM3cBoundaryExpectation(expectation, {
    "display-v2": digest,
  });
  const snapshot = {
    desiredEnabled: true,
    pending: {
      op: "update",
      txId: "tx-1",
      phase: "PREPARED",
      targetVersion: digest,
      targetDigest: digest,
    },
  };
  assert(
    resolved.targetVersion === digest,
    "internal version token is the package hash",
  );
  assert(
    resolved.targetDigest === digest,
    "digest identity is the same package hash",
  );
  assert(
    matchesM3cBoundary(resolved, snapshot),
    "hash identities match the durable record",
  );
  assert(
    !matchesM3cBoundary(resolved, {
      ...snapshot,
      pending: { ...snapshot.pending, targetVersion: "display-v2" },
    }),
    "manifest display label is not accepted as an internal token",
  );
  assert(
    reject(() => resolveM3cBoundaryExpectation(expectation, {})),
    "unknown fixture ID is rejected",
  );
});

Deno.test("M3c launch rejection records evidence and remains a failure", async () => {
  const launchError = new Error("injected launch rejection");
  const failures: M3cProcessLaunchFailureEvidence[] = [];
  let launchCalls = 0;
  let observedError: unknown;
  try {
    await launchM3cBrowserWithEvidence(
      () => {
        launchCalls += 1;
        return Promise.reject(launchError);
      },
      {
        launchId: 4,
        mode: "reopen",
        stdoutLogPath: "browser-4.stdout.log",
        stderrLogPath: "browser-4.stderr.log",
      },
      (evidence) => failures.push(evidence),
    );
  } catch (error) {
    observedError = error;
  }
  const evidence = failures[0];
  assert(launchCalls === 1, "injected launch path is called once");
  assert(observedError === launchError, "launch rejection is propagated");
  assert(
    failures.length === 1 && evidence !== undefined,
    "failure is recorded",
  );
  assert(
    evidence.launchStatus === "rejected" && evidence.launchId === 4 &&
      evidence.mode === "reopen" &&
      evidence.failure.includes("injected launch rejection"),
    "launch identity and failure are preserved",
  );
  assert(
    evidence.stdoutLogPath === "browser-4.stdout.log" &&
      evidence.stderrLogPath === "browser-4.stderr.log",
    "known log locations are preserved",
  );
  assert(
    evidence.process === null &&
      evidence.ownershipEvidence === "unavailable" &&
      evidence.teardownEvidence === "unavailable",
    "unavailable ownership and teardown are not claimed",
  );
});

Deno.test("M3c OLD_STOPPED matcher distinguishes pre-evaluation admission", () => {
  const digest = "b".repeat(64);
  const expectation: M3cResolvedBoundaryExpectation = {
    boundary: "OLD_STOPPED",
    transactionId: "tx-2",
    operation: "update",
    targetPackageId: "v2",
    targetVersion: digest,
    targetDigest: digest,
    attempted: false,
  };
  const snapshot = {
    pending: {
      op: "update",
      txId: "tx-2",
      phase: "OLD_STOPPED",
      targetVersion: digest,
      targetDigest: digest,
    },
    attempt: { attemptedDigests: [] },
  };
  assert(
    matchesM3cBoundary(expectation, snapshot),
    "unattempted target matches OLD_STOPPED",
  );
  assert(
    !matchesM3cBoundary({ ...expectation, attempted: true }, snapshot),
    "attempted target does not match before evaluation",
  );
  assert(
    matchesM3cBoundary({ ...expectation, attempted: true }, {
      ...snapshot,
      attempt: { attemptedDigests: [digest] },
    }),
    "persisted attempt is distinguishable",
  );
});

Deno.test("M3c fallback matcher binds recovery epoch and attempt bit", () => {
  const fallbackDigest = "c".repeat(64);
  const rejectedDigest = "d".repeat(64);
  const base: M3cBoundaryExpectation = {
    boundary: "fallback_pending_before_mark",
    transactionId: "captured",
    operation: "update",
    targetPackageId: "v1",
    attempted: false,
  };
  const resolved = resolveM3cBoundaryExpectation(
    base,
    { v1: fallbackDigest },
    uuid,
  );
  const snapshot = {
    pending: {
      op: "startup_recovery",
      txId: uuid,
      phase: "OLD_STOPPED",
      targetVersion: fallbackDigest,
      targetDigest: fallbackDigest,
      request: { op: "startup_recovery", recoveryEpoch: uuid },
    },
    attempt: {
      originTxId: uuid,
      recoveryEpoch: uuid,
      request: { op: "startup_recovery", recoveryEpoch: uuid },
      attemptedDigests: [],
      rejectedDigests: [rejectedDigest],
    },
  };
  assert(
    matchesM3cBoundary(resolved, snapshot),
    "fallback before attempt mark matches",
  );
  const marked = resolveM3cBoundaryExpectation(
    {
      ...base,
      boundary: "fallback_attempt_marked",
      attempted: true,
    },
    { v1: fallbackDigest },
    uuid,
  );
  assert(
    matchesM3cBoundary(marked, {
      ...snapshot,
      attempt: { ...snapshot.attempt, attemptedDigests: [fallbackDigest] },
    }),
    "fallback after attempt mark matches",
  );
  assert(
    !matchesM3cBoundary(marked, snapshot),
    "pre-mark ledger cannot match post-mark boundary",
  );
  assert(
    reject(() =>
      resolveM3cBoundaryExpectation(base, { v1: fallbackDigest }, "not-a-uuid")
    ),
    "malformed recovery epoch is rejected",
  );
});

Deno.test("M3c admission comparator covers bytes revision high-water and evaluations", () => {
  const before = {
    journalSha256: "e".repeat(64),
    journalByteLength: 128,
    revision: 3,
    evaluationCount: 2,
    highWaterFingerprint: '{"releaseSequence":3}',
  };
  assertM3cAdmissionUnchanged(before, { ...before });
  for (
    const changed of [
      { ...before, journalSha256: "f".repeat(64) },
      { ...before, revision: 4 },
      { ...before, evaluationCount: 3 },
      { ...before, highWaterFingerprint: "{}" },
    ]
  ) {
    assert(
      reject(() => assertM3cAdmissionUnchanged(before, changed)),
      "admission mutation rejected",
    );
  }
});

Deno.test("M3c result parser binds scenario, journal identity and launch", () => {
  const result = {
    schemaVersion: 1,
    runNonce: "nonce",
    launchId: 2,
    buildId: "20261112140843",
    testId: uuid,
    featureId: "m3c-process-test",
    evaluationCount: 2,
    evaluationCountBefore: 1,
    diagnostics: {},
    cleanupDiagnostics: null,
    journal: {
      featureId: "m3c-process-test",
      lifecycle: { featureId: "m3c-process-test" },
    },
    journalSha256: "a".repeat(64),
    journalByteLength: 128,
    outcomes: {},
    windowObservation: null,
  };
  const expected = {
    runNonce: "nonce",
    launchId: 2,
    buildId: result.buildId,
    testId: uuid,
    featureId: "m3c-process-test",
  };
  assert(
    parseM3cBrowserDriverResult(JSON.stringify(result), expected).launchId ===
      2,
    "valid result parsed",
  );
  for (
    const altered of [
      { ...result, runNonce: "old" },
      { ...result, testId: "old" },
      { ...result, featureId: "other" },
      { ...result, journal: { ...result.journal, featureId: "other" } },
      { ...result, journalSha256: "bad" },
    ]
  ) {
    assert(
      reject(() =>
        parseM3cBrowserDriverResult(JSON.stringify(altered), expected)
      ),
      "stale or mismatched result rejected",
    );
  }
});

Deno.test("M3c durable marker binds scenario and journal bytes", () => {
  const lifecycle = {
    featureId: "m3c-process-test",
    stateRevision: 4,
    pending: { txId: "tx-1" },
  };
  const marker = {
    schemaVersion: 1,
    runNonce: "run-1",
    launchId: 3,
    boundary: "PREPARED",
    evaluationCount: 1,
    snapshot: lifecycle,
    testId: uuid,
    featureId: "m3c-process-test",
    journal: { featureId: "m3c-process-test", lifecycle },
    journalSha256: "a".repeat(64),
    journalByteLength: 100,
  };
  const expected = {
    runNonce: "run-1",
    launchId: 3,
    boundary: "PREPARED" as const,
    testId: uuid,
    featureId: "m3c-process-test",
  };
  assert(
    parseM3cDurableMarker(JSON.stringify(marker), expected).journalSha256 ===
      marker.journalSha256,
    "marker journal hash retained",
  );
  assert(
    reject(() =>
      parseM3cDurableMarker(
        JSON.stringify({ ...marker, featureId: "other" }),
        expected,
      )
    ),
    "marker feature mismatch rejected",
  );
});

Deno.test("generated browser driver compiles as plain JavaScript", () => {
  const source = m3cBrowserDriverScript();
  // deno-lint-ignore no-eval
  new Function(source);
  assert(
    !source.includes("complete("),
    "Marionette executeScript has no async callback convention",
  );
  assert(
    !/\bas\s+number\b/.test(source),
    "generated body contains no TypeScript assertion",
  );
  const startupMode = source.indexOf(
    'Services.prefs.setStringPref("nora.startup.mode", "test");',
  );
  const firstModuleImport = source.indexOf("ChromeUtils.importESModule");
  assert(
    startupMode >= 0 && startupMode < firstModuleImport,
    "owned browser selects test mode before loading the signed host",
  );
  const ownerProbeStart = source.indexOf("const probeOwners = async () => {");
  const ownerProbeMode = source.indexOf(
    'Services.prefs.setStringPref("nora.startup.mode", "test");',
    ownerProbeStart,
  );
  const firstOwnerCheck = source.indexOf(
    "await signed.createSignedFeatureHostForTests(",
    ownerProbeStart,
  );
  assert(
    ownerProbeStart >= 0 && ownerProbeMode > ownerProbeStart &&
      firstOwnerCheck > ownerProbeMode,
    "owner probe restores test mode after browser-window startup markers",
  );
  assert(
    source.includes("m0Module.M0ModuleHost.prototype") &&
      source.includes(
        'Object.defineProperty(m0Prototype, "detachWindow", detachDescriptor)',
      ) &&
      source.includes("diagnostics: failureDiagnostics"),
    "token-null detach gate is scoped, restored and diagnosable",
  );
});

Deno.test("owned-process scenario selection preserves the complete default plan", () => {
  const plan = buildM3cProcessPlan();
  assert(
    selectM3cProcessScenarios(plan, []).length === 18,
    "empty selection runs all scenarios",
  );
  const selected = selectM3cProcessScenarios(plan, [
    "cleanup-poison-token-null-detach",
    "cleanup-poison-registry-release",
    "cleanup-poison-initialization-recovery",
  ]);
  assert(
    selected.map((scenario) => scenario.id).join(",") ===
      "cleanup-poison-token-null-detach,cleanup-poison-registry-release,cleanup-poison-initialization-recovery",
    "requested scenarios retain plan order",
  );
  assert(
    reject(() => selectM3cProcessScenarios(plan, ["unknown"])),
    "unknown scenarios are rejected",
  );
  assert(
    reject(() =>
      selectM3cProcessScenarios(plan, [
        "cleanup-poison-token-null-detach",
        "cleanup-poison-token-null-detach",
      ])
    ),
    "duplicate scenarios are rejected",
  );
});

Deno.test("M3a signer fixtures round-trip through the package verifier", async () => {
  const runtimeBuildId = "20261112140843";
  const { authorization, packages, packageDigests } = await createAuthorization(
    "m3c-process-smoke",
    runtimeBuildId,
    [],
  );
  const encoded = packages.v1;
  assert(encoded !== undefined, "signed package exists");
  const base64 = encoded.replace(/-/g, "+").replace(/_/g, "/");
  const bytes = Uint8Array.from(
    atob(base64.padEnd(Math.ceil(base64.length / 4) * 4, "=")),
    (character) => character.charCodeAt(0),
  );
  const verified = await verifySignedFeaturePackage(bytes, {
    ...authorization.policy,
    hostBuildId: "m0-schema-2",
    runtimeBuildId,
  });
  assert(
    verified.manifest.version === "v1",
    "display version remains manifest data",
  );
  assert(
    verified.packageSha256 === packageDigests.v1,
    "engine identity is verifier envelope hash",
  );
  assert(
    !("privateKey" in authorization),
    "public authorization contains no private key",
  );
});

Deno.test("owned-process plan marks admission stages and OLD_STOPPED before evaluation", () => {
  const plan = buildM3cProcessPlan();
  const oldStoppedScenario = plan.find((scenario) =>
    scenario.id === "old-stopped-update"
  );
  const oldStoppedAttemptedScenario = plan.find((scenario) =>
    scenario.id === "old-stopped-attempted-update"
  );
  const oldStopped = oldStoppedScenario?.steps[0];
  const oldStoppedAttempted = oldStoppedAttemptedScenario?.steps[0];
  assert(plan.length === 18, "owned-process plan contains 18 scenarios");
  assert(
    oldStopped?.boundary?.boundary === "OLD_STOPPED" &&
      oldStopped.boundary.attempted === false,
    "OLD_STOPPED captures pre-evaluation state",
  );
  assert(
    oldStoppedAttempted?.boundary?.boundary === "OLD_STOPPED" &&
      oldStoppedAttempted.boundary.attempted === true &&
      oldStoppedScenario?.featureId !== oldStoppedAttemptedScenario?.featureId,
    "a separate OLD_STOPPED scenario captures the attempted state",
  );
  assert(
    oldStoppedAttempted.expected?.evaluationCount === 1 &&
      JSON.stringify(oldStoppedAttempted.expected.attemptedPackageIds) ===
        JSON.stringify(["v2"]) &&
      JSON.stringify(oldStoppedAttempted.expected.rejectedPackageIds) === "[]",
    "attempt marking precedes v2 evaluation and has an exact one-entry ledger",
  );
  assert(
    oldStopped.expected?.evaluationCount === 1 &&
      JSON.stringify(oldStopped.expected.attemptedPackageIds) === "[]" &&
      JSON.stringify(oldStopped.expected.rejectedPackageIds) === "[]",
    "no candidate evaluation or ledger entry precedes termination",
  );
  const admission = plan.find((scenario) =>
    scenario.id === "failed-sequence-admission-floor"
  )?.steps[0]?.actions;
  assert(
    admission?.some((action) =>
      action.kind === "update-rejected" && action.code === "sequence-too-low"
    ),
    "lower package rejects at update admission",
  );
  assert(
    admission?.some((action) =>
      action.kind === "update-rejected" &&
      action.code === "sequence-equivocation"
    ),
    "equivocal package rejects at update admission",
  );
});

Deno.test("durable boundary wait rejects result completion without its marker", async () => {
  const directory = await Deno.makeTempDir();
  try {
    const resultPath = `${directory}/result.json`;
    await Deno.writeTextFile(resultPath, "{}");
    let message = "";
    try {
      await waitForDurableMarker({
        markerPath: `${directory}/marker.json`,
        errorPath: `${directory}/error.json`,
        resultPath,
        runNonce: "nonce",
        launchId: 1,
        boundary: "PREPARED",
        timeoutMs: 5_000,
      });
    } catch (error) {
      message = String(error);
    }
    assert(
      message.includes("completed without expected durable marker"),
      "completed driver rejects a missing boundary immediately",
    );
  } finally {
    await Deno.remove(directory, { recursive: true });
  }
});
