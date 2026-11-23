// SPDX-License-Identifier: MPL-2.0

import { join } from "@std/path";
import { PROJECT_ROOT } from "./defines.ts";
import {
  signFeaturePackage,
  verifySignedFeaturePackage,
} from "../../browser-features/modules/modules/feature-host/package/FeaturePackageVerifier.ts";
import type { FeaturePackageAuthoringMetadata } from "../../browser-features/modules/modules/feature-host/package/types.ts";
import {
  assertProcessRecoveryPlatform,
  buildTrace,
  killBrowser,
  launchBrowser,
  recordTrace,
  reservePort,
  waitForDriverResult,
  waitForDurableMarker,
  writePrivateProfile,
} from "./hotfix_process_recovery.ts";
import type { ProcessRecoveryTrace } from "./hotfix_process_recovery_types.ts";
import {
  assertM3cAdmissionUnchanged,
  matchesM3cBoundary,
  parseM3cBrowserDriverResult,
  parseM3cDurableMarker,
  resolveM3cBoundaryExpectation,
  selectM3cProcessScenarios,
} from "./m3c_signed_process_recovery_helpers.ts";
import type {
  M3cBoundaryExpectation,
  M3cBrowserDriverResult,
  M3cDurableMarker,
  M3cExpectedState,
  M3cOwnedProcessEvidence,
  M3cPackageFixture,
  M3cProcessScenario,
  M3cProcessScenarioEvidence,
  M3cProcessStep,
  M3cPublicAuthorization,
  M3cResolvedBoundaryExpectation,
} from "./m3c_signed_process_recovery_types.ts";

const RUN_ROOT = join(PROJECT_ROOT, "_dist", "m3c-signed-process-runs");
const FEATURE_HOST_URI =
  "resource://noraneko/modules/feature-host/FeatureHost.sys.mjs";
const M0_MODULE_HOST_URI =
  "resource://noraneko/modules/feature-host/M0ModuleHost.sys.mjs";
const SIGNED_HOST_URI =
  "resource://noraneko/modules/feature-host/M3cSignedExecution.sys.mjs";
const WINDOW_IDENTITY_URI =
  "resource://noraneko/modules/feature-host/WindowIdentity.sys.mjs";

function hex(bytes: Uint8Array): string {
  return Array.from(bytes, (value) => value.toString(16).padStart(2, "0"))
    .join("");
}

function arrayBuffer(bytes: Uint8Array): ArrayBuffer {
  const copy = new Uint8Array(bytes.length);
  copy.set(bytes);
  return copy.buffer;
}

function decodeBase64Url(value: string): Uint8Array {
  const base64 = value.replace(/-/g, "+").replace(/_/g, "/");
  return Uint8Array.from(
    atob(base64.padEnd(Math.ceil(base64.length / 4) * 4, "=")),
    (character) => character.charCodeAt(0),
  );
}

function base64Url(bytes: Uint8Array): string {
  return btoa(String.fromCharCode(...bytes)).replace(/\+/g, "-")
    .replace(/\//g, "_").replace(/=+$/g, "");
}

async function digest(bytes: Uint8Array): Promise<string> {
  const copy = new Uint8Array(bytes.length);
  copy.set(bytes);
  return hex(
    new Uint8Array(await crypto.subtle.digest("SHA-256", arrayBuffer(copy))),
  );
}

async function signedPackage(
  featureId: string,
  version: string,
  releaseSequence: number,
  source: string,
  capabilities: readonly string[],
  authority: CryptoKeyPair,
  keyId: string,
  runtimeBuildId: string,
): Promise<Uint8Array> {
  const metadata: FeaturePackageAuthoringMetadata = {
    featureId,
    version,
    releaseSequence,
    compatibility: {
      hostSchema: 2,
      hostBuildId: "m0-schema-2",
      runtimeBuildIds: [runtimeBuildId],
      capabilities: [...capabilities],
    },
  };
  return await signFeaturePackage(
    metadata,
    source,
    keyId,
    authority.privateKey,
  );
}

export async function createAuthorization(
  featureId: string,
  runtimeBuildId: string,
  capabilities: readonly string[],
): Promise<{
  authorization: M3cPublicAuthorization;
  packages: Record<string, string>;
  packageDigests: Record<string, string>;
}> {
  const pair = await crypto.subtle.generateKey(
    { name: "Ed25519" },
    false,
    ["sign", "verify"],
  ) as CryptoKeyPair;
  const keyId = "ephemeral-m3c-process-key";
  const publicKey = base64Url(
    new Uint8Array(
      await crypto.subtle.exportKey("raw", pair.publicKey),
    ),
  );
  const authorization: M3cPublicAuthorization = {
    trustKind: "core-system-principal",
    policy: {
      featureId,
      hostSchema: 2,
      hostBuildId: "caller-value-is-ignored",
      runtimeBuildId: "caller-value-is-ignored",
      allowedCapabilities: [...capabilities],
      minimumReleaseSequence: 1,
      trustedKeys: [{ keyId, publicKey }],
    },
  };
  const healthy = capabilities.includes("hub-pages")
    ? `(() => { globalThis.__floorpM0 = { activate(context, request) { request(JSON.stringify({ type: "hub.register", descriptor: { localId: "m3c-process", route: ${
      JSON.stringify(`/extensions/${featureId}/m3c-process`)
    }, titleKey: "hotfixHub.sample.title", navigation: { labelKey: "hotfixHub.sample.navigation", order: 10 }, view: { kind: "message", titleKey: "hotfixHub.sample.title", bodyKey: "hotfixHub.sample.body", params: { version: "m3c-process" } } } })); }, attachWindow() {}, health() { return "m3c-process"; }, deactivate() {} }; })();`
    : '(() => { globalThis.__floorpM0 = { activate() {}, attachWindow() {}, health() { return "m3c-process"; }, deactivate() {} }; })();';
  const throwing =
    '(() => { globalThis.__floorpM0 = { activate() {}, attachWindow() {}, health() { return "m3c-process"; }, deactivate() { throw new Error("signed cleanup poison"); } }; })();';
  const hanging =
    '(() => { globalThis.__floorpM0 = { activate() {}, attachWindow() {}, health() { return "m3c-process"; }, deactivate() { return new Promise(() => {}); } }; })();';
  const activationFailure =
    '(() => { globalThis.__floorpM0 = { activate() { throw new Error("signed candidate activation failure"); }, attachWindow() {}, health() { return "m3c-process"; }, deactivate() {} }; })();';
  const equivocal =
    '(() => { globalThis.__floorpM0 = { activate() {}, attachWindow() {}, health() { return "m3c-equivocal"; }, deactivate() {} }; })();';
  const sources = [
    ["v1", 1, healthy],
    ["v2", 2, healthy],
    ["v3", 3, healthy],
    ["v3-fail", 3, activationFailure],
    ["lower", 2, healthy],
    ["equivocal", 3, equivocal],
    ["cleanup-throw", 4, throwing],
    ["cleanup-hang", 5, hanging],
    ["cleanup-next", 6, healthy],
  ] as const;
  const packages: Record<string, string> = {};
  const packageDigests: Record<string, string> = {};
  const fixtures: Record<string, M3cPackageFixture> = {};
  const packagePolicy = {
    ...authorization.policy,
    hostBuildId: "m0-schema-2",
    runtimeBuildId,
  };
  for (const [name, sequence, source] of sources) {
    const bytes = await signedPackage(
      featureId,
      name,
      sequence,
      source,
      capabilities,
      pair,
      keyId,
      runtimeBuildId,
    );
    const verified = await verifySignedFeaturePackage(bytes, packagePolicy);
    fixtures[name] = {
      version: name,
      releaseSequence: sequence,
      packageSha256: verified.packageSha256,
      bytes,
    };
  }
  for (const [name, fixture] of Object.entries(fixtures)) {
    packages[name] = base64Url(fixture.bytes);
    packageDigests[name] = fixture.packageSha256;
  }
  return { authorization, packages, packageDigests };
}

export function m3cBrowserDriverScript(): string {
  return `
const config = arguments[0];
const matches = ${matchesM3cBoundary.toString()};
const assertAdmissionUnchanged = ${assertM3cAdmissionUnchanged.toString()};
const failureDiagnostics = Object.create(null);
const run = async () => {
  Services.prefs.setStringPref("nora.startup.mode", "test");
  if (Services.prefs.getStringPref("nora.startup.mode", "") !== "test") throw new Error("owned validation browser did not enter test startup mode");
  const signed = ChromeUtils.importESModule(${JSON.stringify(SIGNED_HOST_URI)});
  const engine = ChromeUtils.importESModule(${
    JSON.stringify(FEATURE_HOST_URI)
  });
  const m0Module = ChromeUtils.importESModule(${
    JSON.stringify(M0_MODULE_HOST_URI)
  });
  const windowIdentity = ChromeUtils.importESModule(${
    JSON.stringify(WINDOW_IDENTITY_URI)
  });
  const decode = value => { const base64 = value.replace(/-/g, "+").replace(/_/g, "/"); return Uint8Array.from(atob(base64.padEnd(Math.ceil(base64.length / 4) * 4, "=")), c => c.charCodeAt(0)); };
  const bytes = Object.fromEntries(Object.entries(config.packages).map(([key, value]) => [key, decode(value)]));
  const root = PathUtils.join(PathUtils.profileDir, "floorp-hotfix-signed-tests-v1", config.testId, config.featureId);
  const journalPath = PathUtils.join(root, "state.json");
  let candidateReadyObserved = false;
  const observer = async snapshot => {
    if (snapshot.pending?.phase === "CANDIDATE_READY") candidateReadyObserved = true;
    if (!config.boundary || !matches(config.boundary, snapshot)) return;
    const journalBytes = await IOUtils.read(journalPath);
    const journalText = new TextDecoder("utf-8", { fatal: true }).decode(journalBytes);
    const journal = JSON.parse(journalText);
    const hash = Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", journalBytes)), value => value.toString(16).padStart(2, "0")).join("");
    await IOUtils.writeJSON(config.markerPath, {
      schemaVersion: 1, runNonce: config.runNonce, launchId: config.launchId,
      boundary: config.boundary.boundary, testId: config.testId,
      featureId: config.featureId, evaluationCount: engine.m0PayloadEvaluationsForTests(),
      snapshot, journal, journalSha256: hash, journalByteLength: journalBytes.length,
    }, { flush: true });
    await new Promise(() => {});
  };
  const policy = config.authorization;
  const evaluationCountBefore = engine.m0PayloadEvaluationsForTests();
  let host = null;
  let openError = null;
  try {
    host = config.mode === "create"
      ? await signed.createSignedFeatureHostForTests(config.testId, config.featureId, true, policy, { afterDurableWriteForTests: observer })
      : await signed.reopenSignedFeatureHostForTests(config.testId, config.featureId, true, policy, { afterDurableWriteForTests: observer, failAfterRecoveryForTests: config.injectRecoveryFailure === true });
  } catch (error) {
    openError = String(error);
  }
  if (openError && !config.injectRecoveryFailure) throw new Error("signed host open failed: " + openError);
  const outcomes = { openError, actions: [], injections: { candidateReadyObserved, recoveryCompleted: openError?.includes("signed-test-recovery-completed") === true } };
  let cleanupDiagnostics = null;
  let windowObservation = null;
  const probeOwners = async () => {
    Services.prefs.setStringPref("nora.startup.mode", "test");
    if (Services.prefs.getStringPref("nora.startup.mode", "") !== "test") throw new Error("owned validation browser did not enter test startup mode for owner probes");
    let facadeError = "";
    try { await signed.createSignedFeatureHostForTests(config.testId, config.featureId, true, policy); }
    catch (error) { facadeError = String(error); }
    let facadeReopenError = "";
    try { await signed.reopenSignedFeatureHostForTests(config.testId, config.featureId, true, policy); }
    catch (error) { facadeReopenError = String(error); }
    let adapterCalls = 0;
    const fail = () => { adapterCalls++; throw new Error("adapter must not be called"); };
    let innerError = "";
    try {
      await engine.M0FeatureHost.openSignedForTests(root, true, {
        testId: config.testId, definitionFingerprint: "owned-process-probe",
        identityCatalog: { featureId: config.featureId, hasVersion: fail, digestForVersion: fail, hasDigest: fail, compatibilityForVersion: fail },
        packageProvider: { ensure: fail, load: fail },
        recordStore: { read: fail, prepare: fail },
      });
    } catch (error) { innerError = String(error); }
    outcomes.ownerProbe = { facadeError, facadeReopenError, innerError, adapterCalls };
    if (!facadeError.includes("owner-active") || !facadeReopenError.includes("owner-active") || !innerError.includes("signed owner already active") || adapterCalls !== 0) throw new Error("both signed ownership fences were not retained: " + JSON.stringify(outcomes.ownerProbe));
  };
  if (host) {
    const staged = new Map();
    const revision = () => Number(host.diagnostics().revision);
    const admissionState = async () => {
      const journalBytes = await IOUtils.read(journalPath);
      const journal = JSON.parse(new TextDecoder("utf-8", { fatal: true }).decode(journalBytes));
      const journalSha256 = Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", journalBytes)), value => value.toString(16).padStart(2, "0")).join("");
      return { journalSha256, journalByteLength: journalBytes.length, revision: revision(), evaluationCount: engine.m0PayloadEvaluationsForTests(), highWaterFingerprint: JSON.stringify(journal.highWater) };
    };
    for (const action of config.actions) {
      if (action.kind === "stage") {
        const before = await admissionState();
        staged.set(action.packageId, await host.stage(bytes[action.packageId]));
        const after = await admissionState();
        assertAdmissionUnchanged(before, after);
        outcomes.actions.push({ kind: "stage", packageId: action.packageId, before, after });
      } else if (action.kind === "update-rejected") {
        const pkg = staged.get(action.packageId) ?? await host.stage(bytes[action.packageId]);
        const before = await admissionState();
        let failure = "";
        try { await host.update(action.txId, revision(), pkg.packageSha256); }
        catch (error) { failure = String(error); }
        if (!failure.includes(action.code)) throw new Error("signed update admission did not reject with " + action.code + ": " + failure);
        const after = await admissionState();
        assertAdmissionUnchanged(before, after);
        outcomes.actions.push({ kind: "update-rejected", code: action.code, before, after });
      } else if (action.kind === "install") {
        const pkg = staged.get(action.packageId) ?? await host.stage(bytes[action.packageId]);
        const outcome = await host.install(action.txId, revision(), pkg.packageSha256);
        if (outcome.status !== "succeeded") throw new Error("setup install did not succeed: " + outcome.status);
        outcomes.actions.push(outcome);
      } else if (action.kind === "update") {
        const pkg = staged.get(action.packageId) ?? await host.stage(bytes[action.packageId]);
        const before = engine.m0PayloadEvaluationsForTests();
        const outcome = await host.update(action.txId, revision(), pkg.packageSha256);
        outcomes.actions.push({ kind: "update", outcome, evaluationDelta: engine.m0PayloadEvaluationsForTests() - before });
      } else if (action.kind === "enable") {
        const outcome = await host.enable(action.txId, revision());
        if (outcome.status !== "succeeded") throw new Error("setup enable did not succeed: " + outcome.status);
        outcomes.actions.push(outcome);
      } else if (action.kind === "disable") {
        outcomes.actions.push(await host.disable(action.txId, revision()));
      } else if (action.kind === "close") {
        const attempts = action.repeat ?? 1;
        for (let index = 0; index < attempts; index++) {
          try { await host.close(); outcomes.actions.push({ close: "resolved" }); }
          catch (error) { outcomes.actions.push({ close: String(error) }); }
        }
        cleanupDiagnostics = host.cleanupDiagnosticsForTests();
        const closeOutcomes = outcomes.actions.filter(action => Object.hasOwn(action, "close"));
        if (config.expectCleanClose && (closeOutcomes.length !== attempts || closeOutcomes.some(action => action.close !== "resolved") || !cleanupDiagnostics.proven)) throw new Error("normal close did not resolve with proven cleanup");
      } else if (action.kind === "owner-retention-probe") {
        await probeOwners();
      } else if (action.kind === "window-close-poison") {
        const mainWindow = window;
        const child = OpenBrowserWindow();
        const deadline = Date.now() + config.windowTimeoutMs;
        while (!child.gBrowserInit?.delayedStartupFinished && Date.now() < deadline) await new Promise(resolve => setTimeout(resolve, 50));
        if (!child.gBrowserInit?.delayedStartupFinished) throw new Error("owned child window startup timed out");
        const id = windowIdentity.getFeatureWindowIdentity(child);
        while (!host.windowIdsForTests().includes(id) && Date.now() < deadline) await new Promise(resolve => setTimeout(resolve, 25));
        if (!host.windowIdsForTests().includes(id)) throw new Error("owned child window was not admitted");
        let disposerCalled = false;
        await host.registerWindowCleanupForTests(id, () => { disposerCalled = true; return Promise.reject(new Error("owned child disposer poison")); });
        child.close();
        while ((!disposerCalled || !host.cleanupDiagnosticsForTests().evidence.includes("window-detach-report")) && Date.now() < deadline) await new Promise(resolve => setTimeout(resolve, 50));
        const detachReportObserved = host.cleanupDiagnosticsForTests().evidence.includes("window-detach-report");
        if (!disposerCalled || child.closed !== true || mainWindow.closed || !detachReportObserved) throw new Error("real child window disposer report was not observed");
        windowObservation = { windowId: id, disposerCalled, closed: child.closed, mainWindowOpen: !mainWindow.closed, detachReportObserved, cleanup: host.cleanupDiagnosticsForTests() };
      } else if (action.kind === "token-null-detach") {
        const mainWindow = window;
        const owned = OpenBrowserWindow();
        const deadline = Date.now() + config.windowTimeoutMs;
        const phase = {
          phase: "window-startup",
          windowId: null,
          windowStarted: false,
          windowAdmitted: false,
          disposerEntered: false,
          windowClosed: false,
          detachCallCount: 0,
          detachDelegateEntered: false,
          detachReturned: false,
          nativeDetachReport: null,
          signedWindowCleanupPending: false,
          windowDetachPending: false,
          updateSettled: false,
          updateStatus: null,
          evaluationDelta: null,
          runtimeStopFailure: false,
          closeStarted: false,
          shutdownBegan: false,
          closeWaitedForDetach: false,
          closeError: null,
          pendingCleared: false,
          runtimeStopFailureRetained: false,
        };
        failureDiagnostics.tokenNullDetach = phase;
        while (!owned.gBrowserInit?.delayedStartupFinished && Date.now() < deadline) await new Promise(resolve => setTimeout(resolve, 50));
        if (!owned.gBrowserInit?.delayedStartupFinished) throw new Error("detach window startup timed out");
        phase.windowStarted = true;
        const id = windowIdentity.getFeatureWindowIdentity(owned);
        phase.windowId = id;
        while (!host.windowIdsForTests().includes(id) && Date.now() < deadline) await new Promise(resolve => setTimeout(resolve, 25));
        if (!host.windowIdsForTests().includes(id)) throw new Error("detach window was not admitted");
        phase.windowAdmitted = true;
        let releaseDisposerGate;
        const disposerGate = new Promise(resolve => releaseDisposerGate = resolve);
        let releaseDetachGate;
        const detachGate = new Promise(resolve => releaseDetachGate = resolve);
        let disposerEntered = false;
        let detachReturned = false;
        let updatePromise;
        let updateSettled = false;
        let closeSettled = false;
        let closeResult = null;
        let prototypePatched = false;
        const m0Prototype = m0Module.M0ModuleHost.prototype;
        const detachDescriptor = Object.getOwnPropertyDescriptor(m0Prototype, "detachWindow");
        if (!detachDescriptor || typeof detachDescriptor.value !== "function") throw new Error("M0 detachWindow descriptor is unavailable");
        const originalDetachWindow = detachDescriptor.value;
        const before = engine.m0PayloadEvaluationsForTests();
        try {
          await host.registerWindowCleanupForTests(id, async () => {
            disposerEntered = true;
            phase.disposerEntered = true;
            owned.close();
            await disposerGate;
            throw new Error("gated token-null disposer poison");
          });
          Object.defineProperty(m0Prototype, "detachWindow", {
            ...detachDescriptor,
            value: function(windowId) {
              if (windowId !== id) return originalDetachWindow.apply(this, [windowId]);
              phase.detachCallCount++;
              return originalDetachWindow.apply(this, [windowId]).then(async report => {
                phase.detachDelegateEntered = true;
                phase.nativeDetachReport = {
                  errors: Array.isArray(report.errors) ? report.errors.slice(0, 8).map(error => String(error).slice(0, 160)) : [],
                  timedOut: report.timedOut === true,
                  pendingTasks: Number.isSafeInteger(report.pendingTasks) ? report.pendingTasks : 0,
                  pendingResources: Number.isSafeInteger(report.pendingResources) ? report.pendingResources : 0,
                };
                await detachGate;
                detachReturned = true;
                phase.detachReturned = true;
                return report;
              });
            },
          });
          prototypePatched = true;
          const pkg = staged.get(action.packageId) ?? await host.stage(bytes[action.packageId]);
          updatePromise = host.update(action.txId, revision(), pkg.packageSha256).then(
            outcome => ({ outcome }),
            error => ({ error: String(error) }),
          ).then(result => { updateSettled = true; phase.updateSettled = true; return result; });
          while ((!disposerEntered || !owned.closed || !phase.detachDelegateEntered) && Date.now() < deadline) await new Promise(resolve => setTimeout(resolve, 25));
          phase.windowClosed = owned.closed;
          if (!disposerEntered || !owned.closed || !phase.detachDelegateEntered || phase.detachCallCount !== 1) throw new Error("token-null close/dispose/detach phase was not observed");
          const pendingBeforeDisposerRelease = host.cleanupDiagnosticsForTests();
          phase.signedWindowCleanupPending = pendingBeforeDisposerRelease.evidence.includes("signed-window-cleanup-pending");
          phase.windowDetachPending = pendingBeforeDisposerRelease.evidence.includes("window-detach-pending");
          if (!phase.signedWindowCleanupPending || phase.windowDetachPending) throw new Error("token-null pending cleanup evidence did not identify the signed-only task");
          releaseDisposerGate();
          const updateDeadline = Date.now() + config.windowTimeoutMs;
          while (!updateSettled && Date.now() < updateDeadline) await new Promise(resolve => setTimeout(resolve, 25));
          if (!updateSettled) throw new Error("token-null update did not settle after disposer release");
          const updateResult = await updatePromise;
          phase.updateStatus = updateResult.outcome?.status ?? updateResult.error ?? null;
          phase.evaluationDelta = engine.m0PayloadEvaluationsForTests() - before;
          const afterUpdate = host.cleanupDiagnosticsForTests();
          phase.runtimeStopFailure = afterUpdate.evidence.includes("runtime-stop:replacement");
          if (updateResult.error || updateResult.outcome?.status !== "failed_quarantined" || phase.evaluationDelta !== 0 || !phase.runtimeStopFailure) throw new Error("token-null replacement cleanup failure was not retained");
          phase.phase = "close-waits-for-detach";
          phase.closeStarted = true;
          const closePromise = host.close().then(
            () => ({ status: "resolved", error: null }),
            error => ({ status: "rejected", error: String(error) }),
          ).then(result => { closeSettled = true; closeResult = result; return result; });
          let shutdownDiagnostics = host.cleanupDiagnosticsForTests();
          while ((!shutdownDiagnostics.evidence.includes("signed-window-cleanup-pending") || shutdownDiagnostics.evidence.includes("host-not-closed") || shutdownDiagnostics.evidence.includes("window-observer-installed")) && !closeSettled && Date.now() < deadline) {
            await new Promise(resolve => setTimeout(resolve, 25));
            shutdownDiagnostics = host.cleanupDiagnosticsForTests();
          }
          phase.shutdownBegan = !shutdownDiagnostics.evidence.includes("host-not-closed") && !shutdownDiagnostics.evidence.includes("window-observer-installed") && shutdownDiagnostics.evidence.includes("signed-window-cleanup-pending");
          await new Promise(resolve => setTimeout(resolve, 0));
          shutdownDiagnostics = host.cleanupDiagnosticsForTests();
          phase.closeWaitedForDetach = phase.shutdownBegan && !closeSettled && shutdownDiagnostics.evidence.includes("signed-window-cleanup-pending") && !shutdownDiagnostics.evidence.includes("window-detach-pending");
          if (!phase.closeWaitedForDetach) throw new Error("host close did not remain pending on the token-null cleanup promise");
          releaseDetachGate();
          const closeDeadline = Date.now() + config.windowTimeoutMs;
          while (!closeSettled && Date.now() < closeDeadline) await new Promise(resolve => setTimeout(resolve, 25));
          if (!closeSettled || closeResult.status !== "rejected" || !closeResult.error.includes("cleanup-unproven")) throw new Error("close did not settle with cleanup-unproven after detach release");
          phase.closeError = closeResult.error.slice(0, 160);
          const afterClose = host.cleanupDiagnosticsForTests();
          phase.pendingCleared = !afterClose.evidence.includes("signed-window-cleanup-pending");
          phase.runtimeStopFailureRetained = afterClose.evidence.includes("runtime-stop:replacement");
          if (!detachReturned || !phase.pendingCleared || !phase.runtimeStopFailureRetained) throw new Error("token-null cleanup completion or sticky failure evidence was lost");
          outcomes.actions.push({ kind: "update", outcome: updateResult.outcome, evaluationDelta: phase.evaluationDelta });
          outcomes.actions.push({ kind: "token-null-close", close: phase.closeError });
          cleanupDiagnostics = afterClose;
          windowObservation = {
            windowId: id, disposerCalled: disposerEntered, closed: owned.closed,
            mainWindowOpen: !mainWindow.closed, detachCallCount: phase.detachCallCount,
            detachDelegateEntered: phase.detachDelegateEntered, detachReturned,
            nativeDetachReport: phase.nativeDetachReport,
            signedWindowCleanupPending: phase.signedWindowCleanupPending,
            windowDetachPending: phase.windowDetachPending,
            closeWaitedForDetach: phase.closeWaitedForDetach,
            pendingCleared: phase.pendingCleared,
            runtimeStopFailureRetained: phase.runtimeStopFailureRetained,
          };
        } finally {
          releaseDisposerGate();
          releaseDetachGate();
          const settleDeadline = Date.now() + config.windowTimeoutMs;
          while ((!updateSettled || (prototypePatched && !detachReturned)) && Date.now() < settleDeadline) await new Promise(resolve => setTimeout(resolve, 25));
          if (prototypePatched) Object.defineProperty(m0Prototype, "detachWindow", detachDescriptor);
        }
      } else if (action.kind === "registry-release-poison") {
        const registryModule = ChromeUtils.importESModule("resource://noraneko/modules/feature-host/HubRegistry.sys.mjs");
        const registry = registryModule.getSharedHubRegistry();
        const ownClaimDescriptor = Object.getOwnPropertyDescriptor(
          registry,
          "claimHost",
        );
        const original = registry.claimHost;
        const claimsBefore = registry.diagnosticsForTests.claims;
        let scenarioClaims = 0;
        let releaseAttempts = 0;
        registry.claimHost = function(featureId, hostEpoch) {
          const claim = original.call(this, featureId, hostEpoch);
          if (featureId !== config.featureId) return claim;
          scenarioClaims++;
          return { featureId: claim.featureId, hostEpoch: claim.hostEpoch, release() { releaseAttempts++; throw new Error("owned registry release poison"); } };
        };
        try {
          const pkg = staged.get(action.packageId) ?? await host.stage(bytes[action.packageId]);
          const installed = await host.install("registry-poison-install", revision(), pkg.packageSha256);
          const enabled = await host.enable("registry-poison-enable", revision());
          if (installed.status !== "succeeded" || enabled.status !== "succeeded") throw new Error("registry fixture install/enable failed");
          outcomes.actions.push(installed, enabled);
          try { await host.close(); } catch (error) { outcomes.actions.push({ close: String(error) }); }
          cleanupDiagnostics = host.cleanupDiagnosticsForTests();
          const claimsAfter = registry.diagnosticsForTests.claims;
          outcomes.registryDiagnostics = { claims: claimsAfter, scenarioClaims, releaseAttempts };
          if (scenarioClaims !== 1 || releaseAttempts !== 1 || claimsAfter !== claimsBefore + 1) throw new Error("real shared registry claim did not remain owned");
        } finally {
          if (ownClaimDescriptor) {
            Object.defineProperty(registry, "claimHost", ownClaimDescriptor);
          } else {
            delete registry.claimHost;
          }
        }
      }
    }
    cleanupDiagnostics ??= host.cleanupDiagnosticsForTests();
  }
  if (config.injectRecoveryFailure) await probeOwners();
  const journalBytes = await IOUtils.read(journalPath);
  const journalText = new TextDecoder("utf-8", { fatal: true }).decode(journalBytes);
  const journal = JSON.parse(journalText);
  const journalSha256 = Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", journalBytes)), value => value.toString(16).padStart(2, "0")).join("");
  const result = {
    schemaVersion: 1, runNonce: config.runNonce, launchId: config.launchId,
    buildId: Services.appinfo.appBuildID, testId: config.testId,
    featureId: config.featureId, evaluationCount: engine.m0PayloadEvaluationsForTests(),
    evaluationCountBefore,
    diagnostics: host ? host.diagnostics() : {}, cleanupDiagnostics,
    journal, journalSha256, journalByteLength: journalBytes.length, outcomes, windowObservation,
  };
  await IOUtils.writeJSON(config.resultPath, result, { flush: true });
};
void run().catch(async error => {
  await IOUtils.writeJSON(config.errorPath, {
    runNonce: config.runNonce,
    launchId: config.launchId,
    error: String(error),
    diagnostics: failureDiagnostics,
  }, { flush: true });
});
return "started";
`;
}

function transactionId(scenarioId: string, index: number): string {
  return `m3c-${scenarioId}-${index}`;
}

function boundaryExpectation(
  boundary: M3cBoundaryExpectation["boundary"],
  transactionId: string,
  operation: M3cBoundaryExpectation["operation"],
  targetPackageId: string,
  desiredEnabled?: boolean,
  attempted?: boolean,
): M3cBoundaryExpectation {
  return {
    boundary,
    transactionId,
    operation,
    targetPackageId,
    ...(desiredEnabled === undefined ? {} : { desiredEnabled }),
    ...(attempted === undefined ? {} : { attempted }),
  };
}

function assertSafeBoundary(
  expectation: M3cResolvedBoundaryExpectation,
  marker: M3cDurableMarker,
): void {
  if (!matchesM3cBoundary(expectation, marker.snapshot)) {
    throw new Error(
      "durable marker did not match exact transaction/package identity",
    );
  }
  const attempt = marker.snapshot.attempt;
  if (typeof attempt === "object" && attempt !== null) {
    const record = attempt as Record<string, unknown>;
    const tried = [
      ...(Array.isArray(record.attemptedDigests)
        ? record.attemptedDigests
        : []),
      ...(Array.isArray(record.rejectedDigests) ? record.rejectedDigests : []),
    ];
    if (tried.length > 2) {
      throw new Error("durable candidate ledger exceeds two entries");
    }
    if (
      Array.isArray(record.rejectedDigests) &&
      typeof marker.snapshot.rollbackDigest === "string" &&
      record.rejectedDigests.includes(marker.snapshot.rollbackDigest)
    ) throw new Error("rejected target cannot be the healthy rollback target");
  }
}

function assertExpectedState(
  expected: M3cExpectedState | undefined,
  evaluationCount: number,
  journal: Record<string, unknown>,
  packageDigests: Record<string, string>,
): void {
  if (!expected) return;
  const lifecycle = journal.lifecycle;
  if (typeof lifecycle !== "object" || lifecycle === null) {
    throw new Error("signed journal has no lifecycle record");
  }
  const record = lifecycle as Record<string, unknown>;
  const highWater = journal.highWater;
  if (
    expected.evaluationCount !== undefined &&
    evaluationCount !== expected.evaluationCount
  ) {
    throw new Error(
      `evaluation count ${evaluationCount} did not equal ${expected.evaluationCount}`,
    );
  }
  if (expected.selectedPackageId !== undefined) {
    const selectedDigest = expected.selectedPackageId === null
      ? null
      : packageDigests[expected.selectedPackageId];
    if (
      selectedDigest === undefined && expected.selectedPackageId !== null
    ) {
      throw new Error(
        "expected selected package does not exist in signed fixtures",
      );
    }
    if (
      record.selectedVersion !== selectedDigest ||
      record.selectedDigest !== selectedDigest
    ) {
      throw new Error(
        "recovered selected source digest does not match package identity",
      );
    }
  }
  if (
    expected.desiredEnabled !== undefined &&
    record.desiredEnabled !== expected.desiredEnabled
  ) {
    throw new Error("recovered desired-enabled state changed unexpectedly");
  }
  if (
    expected.pending !== undefined &&
    (record.pending !== null) !== expected.pending
  ) {
    throw new Error("recovered pending state does not match expectation");
  }
  if (expected.outcomeStatus !== undefined) {
    const outcome = record.lastOutcome;
    if (
      typeof outcome !== "object" || outcome === null ||
      (outcome as Record<string, unknown>).status !== expected.outcomeStatus
    ) {
      throw new Error(
        "recovered transaction outcome does not match expectation",
      );
    }
  }
  if (
    expected.blockedReason !== undefined &&
    record.blockedReason !== expected.blockedReason
  ) {
    throw new Error("recovered quarantine state does not match expectation");
  }
  if (
    expected.blocked !== undefined &&
    (typeof record.blockedReason === "string" &&
        record.blockedReason.length > 0) !== expected.blocked
  ) {
    throw new Error("recovered blocked state does not match expectation");
  }
  const actualSequence = typeof highWater === "object" && highWater !== null
    ? (highWater as Record<string, unknown>).releaseSequence
    : null;
  const actualPackage = typeof highWater === "object" && highWater !== null
    ? (highWater as Record<string, unknown>).packageSha256
    : null;
  if (
    expected.highWaterSequence !== undefined &&
    actualSequence !== expected.highWaterSequence
  ) {
    throw new Error(
      "signed journal release-sequence high-water changed unexpectedly",
    );
  }
  const expectedPackage = expected.highWaterPackageId === undefined
    ? undefined
    : expected.highWaterPackageId === null
    ? null
    : packageDigests[expected.highWaterPackageId];
  if (
    expected.highWaterPackageId !== undefined &&
    expected.highWaterPackageId !== null && expectedPackage === undefined
  ) {
    throw new Error(
      "expected high-water package does not exist in signed fixtures",
    );
  }
  if (expectedPackage !== undefined && actualPackage !== expectedPackage) {
    throw new Error(
      "signed journal high-water package identity changed unexpectedly",
    );
  }
  const attemptRecord = typeof record.attempt === "object" &&
      record.attempt !== null
    ? record.attempt as Record<string, unknown>
    : null;
  const attempted = Array.isArray(attemptRecord?.attemptedDigests)
    ? attemptRecord.attemptedDigests
    : [];
  const rejected = Array.isArray(attemptRecord?.rejectedDigests)
    ? attemptRecord.rejectedDigests
    : [];
  if (attempted.length + rejected.length > 2) {
    throw new Error("recovery candidate ledger exceeds two entries");
  }
  if (
    typeof record.rollbackDigest === "string" &&
    rejected.includes(record.rollbackDigest)
  ) throw new Error("rejected target is retained as rollback");
  for (
    const [expectedIds, actual, label] of [
      [expected.attemptedPackageIds, attempted, "attempted"],
      [expected.rejectedPackageIds, rejected, "rejected"],
    ] as const
  ) {
    if (expectedIds === undefined) continue;
    const expectedDigests = expectedIds.map((id) => {
      const packageDigest = packageDigests[id];
      if (!packageDigest) {
        throw new Error(
          `expected ${label} package does not exist in signed fixtures`,
        );
      }
      return packageDigest;
    });
    if (JSON.stringify(actual) !== JSON.stringify(expectedDigests)) {
      throw new Error(
        `recovery ${label} ledger does not match package identity`,
      );
    }
  }
}

async function runStep(options: {
  scenario: M3cProcessScenario;
  step: Omit<M3cProcessStep, "boundary"> & {
    boundary?: M3cResolvedBoundaryExpectation;
  };
  testId: string;
  runNonce: string;
  launchId: number;
  profilePath: string;
  runDirectory: string;
  port: number;
  buildId: string;
  authorization: M3cPublicAuthorization;
  packages: Record<string, string>;
  packageDigests: Record<string, string>;
}): Promise<{
  marker: M3cDurableMarker | null;
  result: M3cBrowserDriverResult | null;
  process: M3cOwnedProcessEvidence;
  failure?: string;
  teardownError?: string;
}> {
  const { scenario, step } = options;
  const markerPath = join(
    options.runDirectory,
    `marker-${options.launchId}.json`,
  );
  const resultPath = join(
    options.runDirectory,
    `result-${options.launchId}.json`,
  );
  const errorPath = join(
    options.runDirectory,
    `error-${options.launchId}.json`,
  );
  const launch = await launchBrowser(
    options.profilePath,
    options.port,
    options.launchId,
    options.runDirectory,
    options.buildId,
  );
  let marker: M3cDurableMarker | null = null;
  let result: M3cBrowserDriverResult | null = null;
  let termination: Awaited<ReturnType<typeof killBrowser>> | null = null;
  let failure: string | undefined;
  let teardownError: string | undefined;
  try {
    await launch.client.setContext("chrome");
    const boundary = step.boundary;
    const response = await launch.client.executeScript(
      m3cBrowserDriverScript(),
      [{
        runNonce: options.runNonce,
        launchId: options.launchId,
        testId: options.testId,
        featureId: scenario.featureId,
        profilePath: options.profilePath,
        mode: step.mode,
        actions: step.actions,
        expectCleanClose: step.expected?.cleanupProven === true,
        boundary: boundary ?? null,
        injectRecoveryFailure: step.injectRecoveryFailure === true,
        authorization: options.authorization,
        packages: options.packages,
        markerPath,
        resultPath,
        errorPath,
        windowTimeoutMs: 15_000,
      }],
    );
    if (response !== "started") {
      throw new Error("M3c process driver did not start");
    }
    if (boundary) {
      const standard = await waitForDurableMarker({
        markerPath,
        errorPath,
        resultPath,
        runNonce: options.runNonce,
        launchId: options.launchId,
        boundary: boundary.boundary,
        timeoutMs: 90_000,
      });
      const raw = await Deno.readTextFile(markerPath);
      marker = parseM3cDurableMarker(raw, {
        runNonce: options.runNonce,
        launchId: options.launchId,
        boundary: boundary.boundary,
        testId: options.testId,
        featureId: scenario.featureId,
      });
      if (standard.evaluationCount !== marker.evaluationCount) {
        throw new Error(
          "M3c marker evaluation count disagrees with supervisor marker",
        );
      }
      assertSafeBoundary(boundary, marker);
      assertExpectedState(
        step.expected,
        marker.evaluationCount,
        marker.journal,
        options.packageDigests,
      );
      const statePath = join(
        options.profilePath,
        "floorp-hotfix-signed-tests-v1",
        options.testId,
        scenario.featureId,
        "state.json",
      );
      const diskBytes = await Deno.readFile(statePath);
      if (
        diskBytes.byteLength !== marker.journalByteLength ||
        await digest(diskBytes) !== marker.journalSha256
      ) throw new Error("M3c marker journal does not match owned profile disk");
    } else {
      const raw = await waitForDriverResult(resultPath, errorPath, 180_000);
      result = parseM3cBrowserDriverResult(raw, {
        runNonce: options.runNonce,
        launchId: options.launchId,
        buildId: launch.buildId,
        testId: options.testId,
        featureId: scenario.featureId,
      });
      const statePath = join(
        options.profilePath,
        "floorp-hotfix-signed-tests-v1",
        options.testId,
        scenario.featureId,
        "state.json",
      );
      const diskBytes = await Deno.readFile(statePath);
      if (
        diskBytes.byteLength !== result.journalByteLength ||
        await digest(diskBytes) !== result.journalSha256
      ) {
        throw new Error("M3c result journal does not match owned profile disk");
      }
      assertExpectedState(
        step.expected,
        result.evaluationCount,
        result.journal,
        options.packageDigests,
      );
      if (
        step.expected?.cleanupProven !== undefined &&
        result.cleanupDiagnostics?.proven !== step.expected.cleanupProven
      ) {
        throw new Error("cleanup proof does not match scenario expectation");
      }
      if (step.expected?.cleanupEvidenceIncludes) {
        const evidence = result.cleanupDiagnostics?.evidence ?? [];
        if (
          !step.expected.cleanupEvidenceIncludes.every((item) =>
            evidence.includes(item)
          )
        ) {
          throw new Error("required cleanup evidence is missing");
        }
      }
      if (
        step.expected?.windowClosed !== undefined &&
        result.windowObservation?.closed !== step.expected.windowClosed
      ) {
        throw new Error(
          "owned child window close state does not match expectation",
        );
      }
      if (
        step.expected?.mainWindowOpen !== undefined &&
        result.windowObservation?.mainWindowOpen !==
          step.expected.mainWindowOpen
      ) {
        throw new Error("the primary browser window did not remain open");
      }
      if (
        step.expected?.windowDisposerRan !== undefined &&
        result.windowObservation?.disposerCalled !==
          step.expected.windowDisposerRan
      ) {
        throw new Error(
          "real window disposer callback did not match expectation",
        );
      }
      if (
        step.expected?.detachReportObserved !== undefined &&
        result.windowObservation?.detachReportObserved !==
          step.expected.detachReportObserved
      ) {
        throw new Error(
          "token-null window detach report did not match expectation",
        );
      }
      if (
        step.expected?.signedWindowCleanupPending !== undefined &&
        result.windowObservation?.signedWindowCleanupPending !==
          step.expected.signedWindowCleanupPending
      ) throw new Error("signed window cleanup pending observation mismatch");
      if (
        step.expected?.windowDetachPending !== undefined &&
        result.windowObservation?.windowDetachPending !==
          step.expected.windowDetachPending
      ) throw new Error("window detach token-map observation mismatch");
      if (
        step.expected?.closeWaitedForDetach !== undefined &&
        result.windowObservation?.closeWaitedForDetach !==
          step.expected.closeWaitedForDetach
      ) throw new Error("close did not await token-null detach completion");
      if (step.expected?.updateEvaluationDeltas) {
        const actions = result.outcomes.actions;
        if (!Array.isArray(actions)) {
          throw new Error("browser result lacks operation evidence");
        }
        const deltas = actions.flatMap((action) =>
          typeof action === "object" && action !== null &&
            (action as Record<string, unknown>).kind === "update"
            ? [(action as Record<string, unknown>).evaluationDelta]
            : []
        );
        if (
          JSON.stringify(deltas) !==
            JSON.stringify(step.expected.updateEvaluationDeltas)
        ) {
          throw new Error(
            "update source evaluation deltas do not match scenario expectation",
          );
        }
      }
      if (
        step.expected?.registryClaims !== undefined ||
        step.expected?.registryReleaseAttempts !== undefined
      ) {
        const diagnostics = result.outcomes.registryDiagnostics;
        if (
          typeof diagnostics !== "object" || diagnostics === null ||
          (step.expected.registryClaims !== undefined &&
            (diagnostics as Record<string, unknown>).scenarioClaims !==
              step.expected.registryClaims) ||
          (step.expected.registryReleaseAttempts !== undefined &&
            (diagnostics as Record<string, unknown>).releaseAttempts !==
              step.expected.registryReleaseAttempts)
        ) {
          throw new Error(
            "shared registry claim count does not match scenario expectation",
          );
        }
      }
      if (step.expected?.closeErrorIncludes) {
        const actions = result.outcomes.actions;
        const closeErrors = Array.isArray(actions)
          ? actions.flatMap((action) =>
            typeof action === "object" && action !== null &&
              typeof (action as Record<string, unknown>).close === "string"
              ? [(action as Record<string, unknown>).close as string]
              : []
          )
          : [];
        if (
          !closeErrors.length ||
          !closeErrors.every((message) =>
            message.includes(step.expected!.closeErrorIncludes!)
          )
        ) {
          throw new Error(
            "close did not retain the expected cleanup-unproven failure",
          );
        }
      }
      if (step.injectRecoveryFailure) {
        const openError = result.outcomes.openError;
        if (
          typeof openError !== "string" ||
          !openError.includes("signed-test-recovery-completed")
        ) {
          throw new Error(
            "cleanup-poison recovery must reject initialization, not return a quarantined host",
          );
        }
        const injections = result.outcomes.injections;
        if (
          typeof injections !== "object" || injections === null ||
          (injections as Record<string, unknown>).candidateReadyObserved !==
            true ||
          (injections as Record<string, unknown>).recoveryCompleted !==
            true ||
          result.evaluationCount - result.evaluationCountBefore !== 1
        ) {
          throw new Error(
            "initialization recovery did not complete candidate recovery before failed open",
          );
        }
        const lifecycle = result.journal.lifecycle;
        if (
          typeof lifecycle !== "object" || lifecycle === null ||
          (lifecycle as Record<string, unknown>).blockedReason !== null ||
          (lifecycle as Record<string, unknown>).pending !== null ||
          typeof (lifecycle as Record<string, unknown>).lastOutcome !==
            "object" ||
          (lifecycle as Record<string, unknown>).lastOutcome === null ||
          ((lifecycle as Record<string, unknown>).lastOutcome as Record<
              string,
              unknown
            >).status !== "succeeded"
        ) {
          throw new Error(
            "initialization failure fabricated a durable quarantine state",
          );
        }
        const probe = result.outcomes.ownerProbe;
        if (
          typeof probe !== "object" || probe === null ||
          typeof (probe as Record<string, unknown>).facadeError !== "string" ||
          !String((probe as Record<string, unknown>).facadeError).includes(
            "owner-active",
          ) ||
          !String((probe as Record<string, unknown>).facadeReopenError)
            .includes("owner-active") ||
          !String((probe as Record<string, unknown>).innerError).includes(
            "signed owner already active",
          ) ||
          (probe as Record<string, unknown>).adapterCalls !== 0
        ) {
          throw new Error(
            "initialization cleanup recovery did not retain both ownership fences",
          );
        }
      }
    }
  } catch (error) {
    failure = String(error);
  } finally {
    try {
      termination = await killBrowser(launch);
    } catch (error) {
      teardownError = String(error);
    }
  }
  const process: M3cOwnedProcessEvidence = {
    ...launch.root,
    profilePath: launch.ownership.profilePath,
    killedPids: termination?.processes.map((process) => process.pid) ?? null,
    portClosed: termination?.portClosed ?? null,
    ...(teardownError === undefined ? {} : { teardownError }),
  };
  return {
    marker,
    result,
    process,
    ...(failure === undefined ? {} : { failure }),
    ...(teardownError === undefined ? {} : { teardownError }),
  };
}

export async function runM3cProcessScenario(
  scenario: M3cProcessScenario,
  expectedBuildId: string,
  trace: ProcessRecoveryTrace,
): Promise<void> {
  assertProcessRecoveryPlatform(Deno.build.os);
  const runNonce = crypto.randomUUID();
  const testId = crypto.randomUUID();
  const runDirectory = join(RUN_ROOT, `${scenario.id}-${testId}`);
  const profilePath = join(runDirectory, "profile");
  await Deno.mkdir(RUN_ROOT, { recursive: true, mode: 0o700 });
  await Deno.mkdir(runDirectory, { mode: 0o700 });
  const port = reservePort();
  const osServerPort = reservePort();
  await writePrivateProfile(profilePath, port, osServerPort);
  const capabilities = scenario.capabilities ?? [];
  const { authorization, packages, packageDigests } = await createAuthorization(
    scenario.featureId,
    expectedBuildId,
    capabilities,
  );
  for (const [name, encoded] of Object.entries(packages)) {
    const bytes = decodeBase64Url(encoded);
    await Deno.writeFile(
      join(runDirectory, `signed-package-${name}.json`),
      bytes,
      { mode: 0o600, createNew: true },
    );
  }
  const evidence: M3cProcessScenarioEvidence = {
    id: scenario.id,
    testId,
    featureId: scenario.featureId,
    runDirectory,
    profilePath,
    launches: [],
  };
  let capturedRecoveryTransactionId: string | undefined;
  try {
    for (const [index, step] of scenario.steps.entries()) {
      if (step.removeCachedPackage) {
        const packageDigest = packageDigests[step.removeCachedPackage];
        if (!packageDigest) {
          throw new Error("unknown test-owned package to remove");
        }
        const packagePath = join(
          profilePath,
          "floorp-signed-package-cache-v1",
          "tests",
          testId,
          scenario.featureId,
          packageDigest,
        );
        const entry = await Deno.lstat(packagePath);
        if (!entry.isFile || entry.isSymlink) {
          throw new Error(
            "test-owned package removal target is not a regular file",
          );
        }
        const realProfile = await Deno.realPath(profilePath);
        const realPackage = await Deno.realPath(packagePath);
        if (!realPackage.startsWith(`${realProfile}/`)) {
          throw new Error("test-owned package escaped its scenario profile");
        }
        await Deno.remove(packagePath);
      }
      const resolvedStep: Omit<M3cProcessStep, "boundary"> & {
        boundary?: M3cResolvedBoundaryExpectation;
      } = {
        ...step,
        boundary: step.boundary
          ? resolveM3cBoundaryExpectation(
            step.boundary,
            packageDigests,
            capturedRecoveryTransactionId,
          )
          : undefined,
      };
      const outcome = await runStep({
        scenario,
        step: resolvedStep,
        testId,
        runNonce,
        launchId: index + 1,
        profilePath,
        runDirectory,
        port,
        buildId: expectedBuildId,
        authorization,
        packages,
        packageDigests,
      });
      evidence.launches.push({
        launchId: index + 1,
        mode: step.mode,
        process: outcome.process,
        boundary: outcome.marker?.boundary ?? null,
        evaluationCount: outcome.marker?.evaluationCount ??
          outcome.result?.evaluationCount ?? null,
        journalSha256: outcome.marker?.journalSha256 ??
          outcome.result?.journalSha256 ?? null,
        journal: outcome.marker?.journal ?? outcome.result?.journal ?? null,
        diagnostics: outcome.result?.diagnostics ?? null,
        cleanupDiagnostics: outcome.result?.cleanupDiagnostics ?? null,
        outcomes: outcome.result?.outcomes ?? null,
        ...(outcome.failure === undefined && outcome.teardownError === undefined
          ? {}
          : { failure: outcome.failure ?? outcome.teardownError }),
      });
      if (outcome.failure) throw new Error(outcome.failure);
      if (outcome.teardownError) {
        throw new Error(
          `owned browser teardown failed: ${outcome.teardownError}`,
        );
      }
      if (step.boundary?.transactionId === "capture") {
        const pending = outcome.marker?.snapshot.pending;
        if (
          typeof pending !== "object" || pending === null ||
          typeof (pending as Record<string, unknown>).txId !== "string"
        ) {
          throw new Error(
            "recovery boundary marker lacks its transaction identity",
          );
        }
        capturedRecoveryTransactionId = (pending as Record<string, unknown>)
          .txId as string;
      }
    }
    trace.scenarios.push(evidence);
    await recordTrace(join(RUN_ROOT, `${runNonce}-trace.json`), trace);
  } catch (error) {
    trace.failure = String(error);
    trace.scenarios.push({ ...evidence, failure: String(error) });
    await recordTrace(join(RUN_ROOT, `${runNonce}-trace.json`), trace);
    throw error;
  }
}

export function buildM3cProcessPlan(): M3cProcessScenario[] {
  const activeV1 = (prefix: string): M3cProcessStep["actions"] => [
    { kind: "stage", packageId: "v1" },
    { kind: "install", txId: transactionId(prefix, 1), packageId: "v1" },
    { kind: "enable", txId: transactionId(prefix, 2) },
  ];
  return [
    {
      id: "prepared-install",
      featureId: "m3c-process-install",
      steps: [
        {
          mode: "create",
          actions: [
            { kind: "stage", packageId: "v1" },
            {
              kind: "install",
              txId: transactionId("prepared-install", 1),
              packageId: "v1",
            },
          ],
          boundary: boundaryExpectation(
            "PREPARED",
            transactionId("prepared-install", 1),
            "install",
            "v1",
            false,
          ),
          expected: {
            selectedPackageId: null,
            desiredEnabled: false,
            highWaterSequence: 1,
            highWaterPackageId: "v1",
            evaluationCount: 0,
            pending: true,
          },
        },
        {
          mode: "reopen",
          actions: [],
          expected: {
            selectedPackageId: null,
            desiredEnabled: false,
            highWaterSequence: 1,
            highWaterPackageId: "v1",
            evaluationCount: 0,
            pending: false,
            outcomeStatus: "failed_restored",
          },
        },
      ],
    },
    {
      id: "enabled-update-prepared",
      featureId: "m3c-process-update-prepared",
      steps: [
        {
          mode: "create",
          actions: [
            ...activeV1("update-prepared"),
            { kind: "stage", packageId: "v2" },
            {
              kind: "update",
              txId: transactionId("update-prepared", 3),
              packageId: "v2",
            },
          ],
          boundary: boundaryExpectation(
            "PREPARED",
            transactionId("update-prepared", 3),
            "update",
            "v2",
            true,
            false,
          ),
          expected: {
            selectedPackageId: "v1",
            desiredEnabled: true,
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 1,
            attemptedPackageIds: [],
            rejectedPackageIds: [],
            pending: true,
          },
        },
        {
          mode: "reopen",
          actions: [],
          expected: {
            selectedPackageId: "v1",
            desiredEnabled: true,
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 1,
            pending: false,
            outcomeStatus: "failed_restored",
          },
        },
      ],
    },
    {
      id: "disabled-update-prepared",
      featureId: "m3c-process-disabled-update",
      steps: [
        {
          mode: "create",
          actions: [
            { kind: "stage", packageId: "v1" },
            {
              kind: "install",
              txId: transactionId("disabled-update", 1),
              packageId: "v1",
            },
            { kind: "stage", packageId: "v2" },
            {
              kind: "update",
              txId: transactionId("disabled-update", 2),
              packageId: "v2",
            },
          ],
          boundary: boundaryExpectation(
            "PREPARED",
            transactionId("disabled-update", 2),
            "update",
            "v2",
            false,
          ),
          expected: {
            selectedPackageId: "v1",
            desiredEnabled: false,
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 0,
            pending: true,
          },
        },
        {
          mode: "reopen",
          actions: [],
          expected: {
            selectedPackageId: "v1",
            desiredEnabled: false,
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 0,
            pending: false,
            outcomeStatus: "failed_restored",
          },
        },
      ],
    },
    {
      id: "old-stopped-update",
      featureId: "m3c-process-old-stopped",
      steps: [
        {
          mode: "create",
          actions: [
            ...activeV1("old-stopped"),
            { kind: "stage", packageId: "v2" },
            {
              kind: "update",
              txId: transactionId("old-stopped", 3),
              packageId: "v2",
            },
          ],
          boundary: boundaryExpectation(
            "OLD_STOPPED",
            transactionId("old-stopped", 3),
            "update",
            "v2",
            true,
            false,
          ),
          expected: {
            selectedPackageId: "v1",
            desiredEnabled: true,
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 1,
            attemptedPackageIds: [],
            rejectedPackageIds: [],
            pending: true,
          },
        },
        {
          mode: "reopen",
          actions: [],
          expected: {
            selectedPackageId: "v1",
            desiredEnabled: true,
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 1,
            pending: false,
            outcomeStatus: "failed_restored",
          },
        },
      ],
    },
    {
      id: "old-stopped-attempted-update",
      featureId: "m3c-process-old-stopped-attempted",
      steps: [
        {
          mode: "create",
          actions: [
            ...activeV1("old-stopped-attempted"),
            { kind: "stage", packageId: "v2" },
            {
              kind: "update",
              txId: transactionId("old-stopped-attempted", 3),
              packageId: "v2",
            },
          ],
          boundary: boundaryExpectation(
            "OLD_STOPPED",
            transactionId("old-stopped-attempted", 3),
            "update",
            "v2",
            true,
            true,
          ),
          expected: {
            selectedPackageId: "v1",
            desiredEnabled: true,
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 1,
            attemptedPackageIds: ["v2"],
            rejectedPackageIds: [],
            pending: true,
          },
        },
        {
          mode: "reopen",
          actions: [],
          expected: {
            selectedPackageId: "v1",
            desiredEnabled: true,
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 1,
            pending: false,
            outcomeStatus: "failed_restored",
          },
        },
      ],
    },
    {
      id: "candidate-ready-update",
      featureId: "m3c-process-candidate-ready",
      steps: [
        {
          mode: "create",
          actions: [
            ...activeV1("candidate-ready"),
            { kind: "stage", packageId: "v2" },
            {
              kind: "update",
              txId: transactionId("candidate-ready", 3),
              packageId: "v2",
            },
          ],
          boundary: boundaryExpectation(
            "CANDIDATE_READY",
            transactionId("candidate-ready", 3),
            "update",
            "v2",
            true,
          ),
          expected: {
            selectedPackageId: "v1",
            desiredEnabled: true,
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 2,
            attemptedPackageIds: ["v2"],
            rejectedPackageIds: [],
            pending: true,
          },
        },
        {
          mode: "reopen",
          actions: [],
          expected: {
            selectedPackageId: "v1",
            desiredEnabled: true,
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 1,
            pending: false,
            outcomeStatus: "failed_restored",
          },
        },
      ],
    },
    {
      id: "committed-pending-publish",
      featureId: "m3c-process-committed",
      steps: [
        {
          mode: "create",
          actions: [
            ...activeV1("committed"),
            { kind: "stage", packageId: "v2" },
            {
              kind: "update",
              txId: transactionId("committed", 3),
              packageId: "v2",
            },
          ],
          boundary: boundaryExpectation(
            "committed_pending_publish",
            transactionId("committed", 3),
            "update",
            "v2",
            true,
          ),
          expected: {
            selectedPackageId: "v2",
            desiredEnabled: true,
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 2,
            attemptedPackageIds: ["v2"],
            rejectedPackageIds: [],
            pending: false,
            outcomeStatus: "committed_pending_publish",
          },
        },
        {
          mode: "reopen",
          actions: [],
          expected: {
            selectedPackageId: "v1",
            desiredEnabled: true,
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 1,
            pending: false,
            outcomeStatus: "failed_restored",
          },
        },
      ],
    },
    {
      id: "fallback-repeated-interruption",
      featureId: "m3c-process-fallback",
      steps: [
        {
          mode: "create",
          actions: [
            ...activeV1("fallback"),
            { kind: "stage", packageId: "v2" },
            {
              kind: "update",
              txId: transactionId("fallback", 3),
              packageId: "v2",
            },
          ],
          expected: {
            selectedPackageId: "v2",
            desiredEnabled: true,
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 2,
            pending: false,
            outcomeStatus: "succeeded",
            updateEvaluationDeltas: [1],
          },
        },
        {
          mode: "reopen",
          actions: [],
          removeCachedPackage: "v2",
          boundary: boundaryExpectation(
            "fallback_pending_before_mark",
            "capture",
            "update",
            "v1",
            undefined,
            false,
          ),
          expected: {
            selectedPackageId: "v2",
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 0,
            attemptedPackageIds: [],
            rejectedPackageIds: ["v2"],
            pending: true,
          },
        },
        {
          mode: "reopen",
          actions: [],
          boundary: boundaryExpectation(
            "fallback_attempt_marked",
            "captured",
            "update",
            "v1",
            undefined,
            true,
          ),
          expected: {
            selectedPackageId: "v2",
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 0,
            attemptedPackageIds: ["v1"],
            rejectedPackageIds: ["v2"],
            pending: true,
          },
        },
        {
          mode: "reopen",
          actions: [],
          expected: {
            selectedPackageId: "v2",
            desiredEnabled: true,
            highWaterSequence: 2,
            highWaterPackageId: "v2",
            evaluationCount: 0,
            attemptedPackageIds: ["v1"],
            rejectedPackageIds: ["v2"],
            pending: false,
            outcomeStatus: "failed_quarantined",
            blocked: true,
          },
        },
      ],
    },
    {
      id: "disabled-selected-package-recovery",
      featureId: "m3c-process-disabled-selected",
      steps: [
        {
          mode: "create",
          actions: [
            { kind: "stage", packageId: "v1" },
            {
              kind: "install",
              txId: transactionId("disabled-selected", 1),
              packageId: "v1",
            },
            { kind: "close" },
          ],
          expected: {
            selectedPackageId: "v1",
            desiredEnabled: false,
            highWaterSequence: 1,
            highWaterPackageId: "v1",
            evaluationCount: 0,
            pending: false,
            cleanupProven: true,
          },
        },
        {
          mode: "reopen",
          actions: [],
          expected: {
            selectedPackageId: "v1",
            desiredEnabled: false,
            highWaterSequence: 1,
            highWaterPackageId: "v1",
            evaluationCount: 0,
            pending: false,
          },
        },
      ],
    },
    {
      id: "failed-sequence-admission-floor",
      featureId: "m3c-process-sequence-floor",
      steps: [{
        mode: "create",
        actions: [
          ...activeV1("sequence-floor"),
          { kind: "stage", packageId: "v2" },
          {
            kind: "update",
            txId: transactionId("sequence-floor", 3),
            packageId: "v2",
          },
          { kind: "stage", packageId: "v3-fail" },
          {
            kind: "update",
            txId: transactionId("sequence-floor", 4),
            packageId: "v3-fail",
          },
          {
            kind: "stage",
            packageId: "lower",
          },
          {
            kind: "update-rejected",
            txId: transactionId("sequence-floor", 5),
            packageId: "lower",
            code: "sequence-too-low",
          },
          {
            kind: "stage",
            packageId: "equivocal",
          },
          {
            kind: "update-rejected",
            txId: transactionId("sequence-floor", 6),
            packageId: "equivocal",
            code: "sequence-equivocation",
          },
        ],
        expected: {
          selectedPackageId: "v2",
          desiredEnabled: true,
          highWaterSequence: 3,
          highWaterPackageId: "v3-fail",
          evaluationCount: 4,
          pending: false,
          outcomeStatus: "failed_restored",
          updateEvaluationDeltas: [1, 2],
        },
      }],
    },
    {
      id: "cleanup-poison-disable",
      featureId: "m3c-process-cleanup-disable",
      steps: [{
        mode: "create",
        actions: [
          { kind: "stage", packageId: "cleanup-throw" },
          {
            kind: "install",
            txId: transactionId("cleanup-disable", 1),
            packageId: "cleanup-throw",
          },
          { kind: "enable", txId: transactionId("cleanup-disable", 2) },
          { kind: "disable", txId: transactionId("cleanup-disable", 3) },
          { kind: "close", repeat: 2 },
          { kind: "owner-retention-probe" },
        ],
        expected: {
          selectedPackageId: "cleanup-throw",
          highWaterSequence: 4,
          highWaterPackageId: "cleanup-throw",
          evaluationCount: 1,
          cleanupProven: false,
          closeErrorIncludes: "cleanup-unproven",
        },
      }],
    },
    {
      id: "cleanup-poison-replacement",
      featureId: "m3c-process-cleanup-replace",
      steps: [{
        mode: "create",
        actions: [
          { kind: "stage", packageId: "cleanup-throw" },
          {
            kind: "install",
            txId: transactionId("cleanup-replace", 1),
            packageId: "cleanup-throw",
          },
          { kind: "enable", txId: transactionId("cleanup-replace", 2) },
          { kind: "stage", packageId: "cleanup-next" },
          {
            kind: "update",
            txId: transactionId("cleanup-replace", 3),
            packageId: "cleanup-next",
          },
          { kind: "close", repeat: 2 },
          { kind: "owner-retention-probe" },
        ],
        expected: {
          selectedPackageId: "cleanup-throw",
          highWaterSequence: 6,
          highWaterPackageId: "cleanup-next",
          evaluationCount: 1,
          outcomeStatus: "failed_quarantined",
          blocked: true,
          cleanupProven: false,
          updateEvaluationDeltas: [0],
          closeErrorIncludes: "cleanup-unproven",
        },
      }],
    },
    {
      id: "cleanup-poison-close-timeout",
      featureId: "m3c-process-cleanup-timeout",
      steps: [{
        mode: "create",
        actions: [
          { kind: "stage", packageId: "cleanup-hang" },
          {
            kind: "install",
            txId: transactionId("cleanup-timeout", 1),
            packageId: "cleanup-hang",
          },
          { kind: "enable", txId: transactionId("cleanup-timeout", 2) },
          { kind: "close", repeat: 2 },
          { kind: "owner-retention-probe" },
        ],
        expected: {
          selectedPackageId: "cleanup-hang",
          highWaterSequence: 5,
          highWaterPackageId: "cleanup-hang",
          evaluationCount: 1,
          cleanupProven: false,
          closeErrorIncludes: "cleanup-unproven",
        },
      }],
    },
    {
      id: "cleanup-poison-runtime-close",
      featureId: "m3c-process-cleanup-close",
      steps: [{
        mode: "create",
        actions: [
          { kind: "stage", packageId: "cleanup-throw" },
          {
            kind: "install",
            txId: transactionId("cleanup-close", 1),
            packageId: "cleanup-throw",
          },
          { kind: "enable", txId: transactionId("cleanup-close", 2) },
          { kind: "close", repeat: 2 },
          { kind: "owner-retention-probe" },
        ],
        expected: {
          selectedPackageId: "cleanup-throw",
          highWaterSequence: 4,
          highWaterPackageId: "cleanup-throw",
          evaluationCount: 1,
          cleanupProven: false,
          closeErrorIncludes: "cleanup-unproven",
        },
      }],
    },
    {
      id: "cleanup-poison-real-window-close",
      featureId: "m3c-process-window-close",
      steps: [{
        mode: "create",
        actions: [
          ...activeV1("window-close"),
          { kind: "window-close-poison" },
          { kind: "close", repeat: 2 },
          { kind: "owner-retention-probe" },
        ],
        expected: {
          selectedPackageId: "v1",
          highWaterSequence: 1,
          highWaterPackageId: "v1",
          evaluationCount: 1,
          cleanupProven: false,
          cleanupEvidenceIncludes: ["window-detach-report"],
          windowClosed: true,
          mainWindowOpen: true,
          windowDisposerRan: true,
          detachReportObserved: true,
          closeErrorIncludes: "cleanup-unproven",
        },
      }],
    },
    {
      id: "cleanup-poison-token-null-detach",
      featureId: "m3c-process-token-null",
      steps: [{
        mode: "create",
        actions: [
          ...activeV1("token-null"),
          {
            kind: "token-null-detach",
            txId: transactionId("token-null", 3),
            packageId: "cleanup-next",
          },
          { kind: "close", repeat: 2 },
          { kind: "owner-retention-probe" },
        ],
        expected: {
          selectedPackageId: "v1",
          highWaterSequence: 6,
          highWaterPackageId: "cleanup-next",
          evaluationCount: 1,
          pending: false,
          outcomeStatus: "failed_quarantined",
          blocked: true,
          cleanupProven: false,
          cleanupEvidenceIncludes: ["runtime-stop:replacement"],
          windowClosed: true,
          mainWindowOpen: true,
          windowDisposerRan: true,
          signedWindowCleanupPending: true,
          windowDetachPending: false,
          closeWaitedForDetach: true,
          updateEvaluationDeltas: [0],
          closeErrorIncludes: "cleanup-unproven",
        },
      }],
    },
    {
      id: "cleanup-poison-registry-release",
      featureId: "m3c-process-registry",
      capabilities: ["hub-pages"],
      steps: [{
        mode: "create",
        actions: [
          { kind: "registry-release-poison", packageId: "v1" },
          { kind: "owner-retention-probe" },
        ],
        expected: {
          selectedPackageId: "v1",
          highWaterSequence: 1,
          highWaterPackageId: "v1",
          evaluationCount: 1,
          cleanupProven: false,
          cleanupEvidenceIncludes: ["hub-host-release"],
          registryClaims: 1,
          registryReleaseAttempts: 1,
          closeErrorIncludes: "cleanup-unproven",
        },
      }],
    },
    {
      id: "cleanup-poison-initialization-recovery",
      featureId: "m3c-process-init-cleanup",
      steps: [
        {
          mode: "create",
          actions: [
            { kind: "stage", packageId: "cleanup-throw" },
            {
              kind: "install",
              txId: transactionId("init-cleanup", 1),
              packageId: "cleanup-throw",
            },
            { kind: "enable", txId: transactionId("init-cleanup", 2) },
          ],
          expected: {
            selectedPackageId: "cleanup-throw",
            desiredEnabled: true,
            highWaterSequence: 4,
            highWaterPackageId: "cleanup-throw",
            evaluationCount: 1,
            pending: false,
          },
        },
        {
          mode: "reopen",
          actions: [],
          injectRecoveryFailure: true,
          expected: {
            selectedPackageId: "cleanup-throw",
            desiredEnabled: true,
            highWaterSequence: 4,
            highWaterPackageId: "cleanup-throw",
            evaluationCount: 1,
            pending: false,
            blocked: false,
            outcomeStatus: "succeeded",
          },
        },
      ],
    },
  ];
}

export async function runM3cProcessRecoveryDriver(
  selectedIds: readonly string[] = [],
): Promise<void> {
  assertProcessRecoveryPlatform(Deno.build.os);
  const plan = selectM3cProcessScenarios(buildM3cProcessPlan(), selectedIds);
  const runNonce = crypto.randomUUID();
  const port = reservePort();
  const osServerPort = reservePort();
  const trace = await buildTrace(runNonce, port, osServerPort);
  for (const scenario of plan) {
    await runM3cProcessScenario(scenario, trace.runtimeExpectedBuildId, trace);
  }
  console.log(`M3c signed process driver completed ${plan.length} scenarios`);
}

if (import.meta.main) {
  const selectedIds = Deno.args.map((argument) => {
    const prefix = "--scenario=";
    if (!argument.startsWith(prefix) || argument.length === prefix.length) {
      throw new Error(`invalid process-driver argument: ${argument}`);
    }
    return argument.slice(prefix.length);
  });
  runM3cProcessRecoveryDriver(selectedIds).catch((error: unknown) => {
    console.error("M3c signed process driver failed", error);
    Deno.exitCode = 1;
  });
}
