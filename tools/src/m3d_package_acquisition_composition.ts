// SPDX-License-Identifier: MPL-2.0
/// <reference lib="deno.ns" />

import type {
  M3dCompositionContext,
  M3dCompositionNativeScripts,
  M3dCompositionObservations,
} from "./m3d_package_acquisition_composition.types.ts";
import type { M3dPackageHttpsSnapshot } from "./m3d_package_https_fixture.types.ts";

const SIGNED_URI =
  "resource://noraneko/modules/feature-host/M3cSignedExecution.sys.mjs";
const FEATURE_HOST_URI =
  "resource://noraneko/modules/feature-host/FeatureHost.sys.mjs";
const DOWNLOADER_URI =
  "resource://noraneko/modules/feature-host/package/FeaturePackageDownloader.sys.mjs";
const SCRIPT_HELPERS = `
  const signed = ChromeUtils.importESModule(${JSON.stringify(SIGNED_URI)});
  const featureHost = ChromeUtils.importESModule(
    ${JSON.stringify(FEATURE_HOST_URI)}
  );
  const downloader = ChromeUtils.importESModule(
    ${JSON.stringify(DOWNLOADER_URI)}
  );
  const STARTUP_PREF = "nora.startup.mode";
  const withTestStartupMode = async operation => {
    const had = Services.prefs.prefHasUserValue(STARTUP_PREF);
    const previous = Services.prefs.getStringPref(STARTUP_PREF, "");
    try {
      Services.prefs.setStringPref(STARTUP_PREF, "test");
      return await operation();
    } finally {
      if (had) Services.prefs.setStringPref(STARTUP_PREF, previous);
      else Services.prefs.clearUserPref(STARTUP_PREF);
    }
  };
  const createFacade = (testId, featureId, policy) =>
    withTestStartupMode(() => signed.createSignedFeatureHostForTests(
      testId,
      featureId,
      true,
      {trustKind: "core-system-principal", policy},
      {excludeWindowsForTests: true}
    ));
  const windowState = window => {
    if (!window["__floorpM3dCompositionPending"]) {
      window["__floorpM3dCompositionPending"] = Object.create(null);
    }
    return window["__floorpM3dCompositionPending"];
  };
  const stateFor = (window, testId) => {
    const registry = window["__floorpM3dCompositionPending"];
    const state = registry && registry[testId];
    return state && state.testId === testId ? state : null;
  };
  const restoreProbe = state => {
    if (state.probeHadUserValue) {
      Services.prefs.setBoolPref(state.probePref, state.probePreviousValue);
    } else {
      Services.prefs.clearUserPref(state.probePref);
    }
  };
  const hex = bytes => Array.from(
    bytes,
    value => value.toString(16).padStart(2, "0")
  ).join("");
  const digest = async bytes => hex(
    new Uint8Array(await crypto.subtle.digest("SHA-256", bytes))
  );
  const decode = value => Uint8Array.from(
    atob(value),
    character => character.charCodeAt(0)
  );
  const downloaderDiagnostics = () => {
    const had = Services.prefs.prefHasUserValue(STARTUP_PREF);
    const previous = Services.prefs.getStringPref(STARTUP_PREF, "");
    try {
      Services.prefs.setStringPref(STARTUP_PREF, "test");
      return downloader.featurePackageDownloadDiagnosticsForTests();
    } finally {
      if (had) Services.prefs.setStringPref(STARTUP_PREF, previous);
      else Services.prefs.clearUserPref(STARTUP_PREF);
    }
  };
  const countNames = [
    "inFlight",
    "reading",
    "verifying",
    "cleanupPending",
    "blocked"
  ];
  const journalRoot = testId => PathUtils.join(
    PathUtils.profileDir,
    "floorp-hotfix-signed-tests-v1",
    testId
  );
  const cacheRoot = testId => PathUtils.join(
    PathUtils.profileDir,
    "floorp-signed-package-cache-v1",
    "tests",
    testId
  );
  const journalPath = (testId, featureId) => PathUtils.join(
    journalRoot(testId),
    featureId,
    "state.json"
  );
  const journalSnapshot = async (testId, featureId) => {
    const path = journalPath(testId, featureId);
    if (!(await IOUtils.exists(path))) {
      return {
        exists: false,
        byteLength: 0,
        sha256: "",
        revision: -1,
        packages: -1,
        highWaterIsNull: false,
        highWaterSequence: -1,
        highWaterHash: ""
      };
    }
    const bytes = await IOUtils.read(path);
    let parsed = null;
    try {
      parsed = JSON.parse(new TextDecoder().decode(bytes));
    } catch {
      parsed = null;
    }
    const record = typeof parsed === "object" && parsed !== null ? parsed : {};
    const lifecycle = typeof record.lifecycle === "object" &&
        record.lifecycle !== null
      ? record.lifecycle
      : {};
    const highWater = typeof record.highWater === "object" &&
        record.highWater !== null
      ? record.highWater
      : {};
    return {
      exists: true,
      byteLength: bytes.byteLength,
      sha256: await digest(bytes),
      revision: Number.isSafeInteger(lifecycle.stateRevision)
        ? lifecycle.stateRevision
        : -1,
      packages: Array.isArray(record.packages) ? record.packages.length : -1,
      highWaterIsNull: record.highWater === null,
      highWaterSequence: Number.isSafeInteger(highWater.releaseSequence)
        ? highWater.releaseSequence
        : -1,
      highWaterHash: typeof highWater.packageSha256 === "string"
        ? highWater.packageSha256
        : ""
    };
  };
  const cacheSnapshot = async (testId, featureId) => {
    const root = PathUtils.join(cacheRoot(testId), featureId);
    if (!(await IOUtils.exists(root))) {
      return {exists: false, files: [], sha256: ""};
    }
    const files = [];
    const collect = async (directory, prefix) => {
      const children = (await IOUtils.getChildren(directory)).slice().sort();
      for (const child of children) {
        const name = prefix
          ? prefix + "/" + PathUtils.filename(child)
          : PathUtils.filename(child);
        const stat = await IOUtils.stat(child);
        if (stat.type === "directory") {
          await collect(child, name);
        } else if (stat.type === "regular") {
          files.push({
            name,
            sha256: await digest(await IOUtils.read(child))
          });
        } else {
          files.push({name, type: stat.type});
        }
      }
    };
    await collect(root, "");
    files.sort((left, right) =>
      left.name < right.name ? -1 : left.name > right.name ? 1 : 0
    );
    return {
      exists: true,
      files,
      sha256: await digest(new TextEncoder().encode(JSON.stringify(files)))
    };
  };
  const facadeSnapshot = async (facade, testId, featureId) => {
    const journal = await journalSnapshot(testId, featureId);
    const cache = await cacheSnapshot(testId, featureId);
    const metadata = JSON.parse(JSON.stringify(facade.metadata()));
    const diagnostics = JSON.parse(JSON.stringify(facade.diagnostics()));
    return {
      journal,
      cache,
      metadata,
      diagnostics,
      metadataCount: metadata.length,
      revision: Number.isSafeInteger(diagnostics.revision)
        ? diagnostics.revision
        : -1,
      desiredEnabled: diagnostics.desiredEnabled === true,
      selectedPackageSha256:
        typeof diagnostics.selectedPackageSha256 === "string"
          ? diagnostics.selectedPackageSha256
          : "",
      selectedHealth: typeof diagnostics.selectedHealth === "string"
        ? diagnostics.selectedHealth
        : "",
      evaluations: featureHost.m0PayloadEvaluationsForTests()
    };
  };
  const snapshotKey = snapshot => JSON.stringify([
    snapshot.journal,
    snapshot.cache,
    snapshot.metadata,
    snapshot.diagnostics,
    snapshot.revision,
    snapshot.desiredEnabled,
    snapshot.selectedPackageSha256,
    snapshot.selectedHealth,
    snapshot.evaluations
  ]);
  const removeOwnedRoots = async testId => {
    await IOUtils.remove(journalRoot(testId), {
      recursive: true,
      ignoreAbsent: true
    });
    await IOUtils.remove(cacheRoot(testId), {
      recursive: true,
      ignoreAbsent: true
    });
    return !(await IOUtils.exists(journalRoot(testId))) &&
      !(await IOUtils.exists(cacheRoot(testId)));
  };
  const ownedRootsAbsent = async testId =>
    !(await IOUtils.exists(journalRoot(testId))) &&
    !(await IOUtils.exists(cacheRoot(testId)));
  const facadeErrorCode = error => {
    const code = error === null || typeof error !== "object"
      ? null
      : error.code;
    return ["dev-test-only","invalid-test-id","invalid-feature-id","experimental-opt-in-required","owner-active","root-exists","journal-missing","unsafe-layout","invalid-authorization","invalid-policy","invalid-test-hook","invalid-transaction-id","invalid-package-hash","invalid-expected-revision","invalid-cleanup","cleanup-unproven","stale-owner","invalid-journal","journal-too-large","catalog-full","package-conflict","sequence-too-low","sequence-equivocation","package-unavailable","invalid-envelope","invalid-metadata","invalid-source","invalid-signing-key","unsupported-crypto","unsupported-version","untrusted-key","invalid-signature","source-digest-mismatch","feature-mismatch","host-incompatible","runtime-incompatible","capability-denied","size-limit","invalid-window-id","operation-failed","busy"].includes(code) ? code : null;
  };
  const downloaderErrorCode = error => {
    const code = error === null || typeof error !== "object"
      ? null
      : error.code;
    return ["aborted", "timeout", "network-failure",
      "unacceptable-response", "size-limit", "package-rejected",
      "expected-package-mismatch", "cleanup-unproven", "busy",
      "unsupported-platform", "invalid-request", "invalid-endpoint",
      "invalid-policy"].includes(code) ? code : null;
  };
`;

const LIFECYCLE_SCRIPT = `
${SCRIPT_HELPERS}
  const testId = input.testId;
  const featureId = input.featureId;
  const probePref = input.probePref;
  const hadProbe = Services.prefs.prefHasUserValue(probePref);
  const previousProbe = Services.prefs.getBoolPref(probePref, false);
  const out = {facadeCreated: false, scriptFailure: false};
  let facade = null;
  let baselineEvaluations = -1;
  let baseline = null;
  let afterDownloads = null;
  let firstBytes = null;
  let secondBytes = null;
  let registeredState = null;
  let phase = "prepare";
  try {
    const window = Services.wm.getMostRecentWindow("navigator:browser");
    let state = stateFor(window, testId);
    registeredState = state;
    if (input.phase === "transactions") {
      if (!state || !state.downloadComplete) {
        throw new Error("composition-state-unavailable");
      }
      facade = state.facade;
      baselineEvaluations = state.baselineEvaluations;
      baseline = state.baseline;
      afterDownloads = state.afterDownloads;
      firstBytes = state.firstBytes;
      secondBytes = state.secondBytes;
      input.first = state.firstEntry;
      input.second = state.secondEntry;
      state.lifecycleScriptPending = true;
      Object.assign(out, state.inertResult);
      phase = "stage-first";
    } else {
      Services.prefs.clearUserPref(probePref);
      baselineEvaluations = featureHost.m0PayloadEvaluationsForTests();
      out.baselineEvaluations = baselineEvaluations;
      phase = "create";
      facade = await createFacade(testId, featureId, input.policy);
      out.facadeCreated = true;
      state = {
        kind: "lifecycle",
        testId,
        featureId,
        probePref,
        probeHadUserValue: hadProbe,
        probePreviousValue: previousProbe,
        facade,
        baselineEvaluations,
        baseline: null,
        firstEntry: input.first,
        secondEntry: input.second,
        firstBytes: null,
        secondBytes: null,
        activeController: null,
        downloadPending: false,
        creationPending: false,
        lifecycleScriptPending: true,
        cleanupRequested: false,
        downloadComplete: false,
        inertResult: null
      };
      windowState(window)[testId] = state;
      registeredState = state;
      phase = "baseline-snapshot";
      baseline = await facadeSnapshot(facade, testId, featureId);
      state.baseline = baseline;
      if (state.cleanupRequested) {
        state.lifecycleScriptPending = false;
        return out;
      }
    out.baselineJournalExists = baseline.journal.exists;
    out.baselineStateValid = baseline.journal.exists &&
      baseline.journal.revision === 0 &&
      baseline.journal.packages === 0 &&
      baseline.journal.highWaterIsNull === true &&
      baseline.journal.highWaterSequence === -1 &&
      baseline.journal.highWaterHash === "" &&
      baseline.metadata.length === 0 &&
      baseline.diagnostics.revision === 0;
    out.baselineJournalSha256 = baseline.journal.sha256;
    out.baselineJournalBytes = baseline.journal.byteLength;
    out.baselineCacheSha256 = baseline.cache.sha256;
    const acquire = async entry => {
      const controller = new window.AbortController();
      state.activeController = controller;
      state.downloadPending = true;
      try {
        return await downloader.downloadFeaturePackageBytes({
          origin: input.origin,
          packageSha256: entry.packageSha256,
          policy: input.policy,
          signal: controller.signal
        });
      } finally {
        state.downloadPending = false;
        state.activeController = null;
      }
    };
    phase = "download-first";
    firstBytes = await acquire(input.first);
    state.firstBytes = firstBytes;
    if (state.cleanupRequested) {
      state.lifecycleScriptPending = false;
      return out;
    }
    phase = "download-second";
    secondBytes = await acquire(input.second);
    state.secondBytes = secondBytes;
    if (state.cleanupRequested) {
      state.lifecycleScriptPending = false;
      return out;
    }
    const expectedFirst = decode(input.first.bytes);
    const expectedSecond = decode(input.second.bytes);
    out.firstBytesExact = firstBytes.byteLength === expectedFirst.byteLength &&
      firstBytes.every((value, index) => value === expectedFirst[index]);
    out.secondBytesExact =
      secondBytes.byteLength === expectedSecond.byteLength &&
      secondBytes.every((value, index) => value === expectedSecond[index]);
    out.firstHashMatches =
      await digest(firstBytes) === input.first.packageSha256;
    out.secondHashMatches =
      await digest(secondBytes) === input.second.packageSha256;
    afterDownloads = await facadeSnapshot(facade, testId, featureId);
    if (state.cleanupRequested) {
      state.lifecycleScriptPending = false;
      return out;
    }
    state.afterDownloads = afterDownloads;
    out.snapshotUnchangedAfterDownloads =
      snapshotKey(afterDownloads) === snapshotKey(baseline);
    out.downloadJournalSha256 = afterDownloads.journal.sha256;
    out.downloadJournalBytes = afterDownloads.journal.byteLength;
    out.downloadCacheSha256 = afterDownloads.cache.sha256;
    out.noEvaluationAfterDownloads =
      afterDownloads.evaluations === baselineEvaluations;
    out.sentinelFalseAfterDownloads =
      Services.prefs.getBoolPref(probePref, false) === false;
    const diagnostics = downloaderDiagnostics();
    out.diagnosticsZeroAfterDownloads = diagnostics !== null &&
      countNames.every(name => diagnostics[name] === 0);
    state.downloadComplete = true;
    state.inertResult = JSON.parse(JSON.stringify(out));
    state.lifecycleScriptPending = false;
    return out;
    }
    phase = "stage-first";
    const stagedOne = await facade.stage(firstBytes);
    out.stageOneSucceeded =
      stagedOne.packageSha256 === input.first.packageSha256;
    const afterStageOne = await facadeSnapshot(facade, testId, featureId);
    out.stageOneJournalUnchanged =
      JSON.stringify(afterStageOne.journal) ===
        JSON.stringify(baseline.journal);
    out.stageOneNoEvaluation =
      afterStageOne.evaluations === baselineEvaluations;
    out.stageOneRevisionUnchanged =
      afterStageOne.revision === afterDownloads.revision;
    phase = "install";
    const installed = await facade.install(
      "m3d-composition-install",
      afterStageOne.revision,
      input.first.packageSha256
    );
    out.installStatus = [
      "pending",
      "committed_pending_publish",
      "succeeded",
      "failed_restored",
      "failed_quarantined",
      "storage_failed",
    ].includes(installed.status) ? installed.status : "unknown";
    out.installSucceeded = out.installStatus === "succeeded";
    const afterInstall = await facadeSnapshot(facade, testId, featureId);
    out.installRevision = afterInstall.revision;
    out.installRevisionTwo = afterInstall.revision === 2;
    out.installSelectedFirst =
      afterInstall.selectedPackageSha256 === input.first.packageSha256;
    out.installDisabled = afterInstall.desiredEnabled === false;
    out.installHighWaterSequence = afterInstall.journal.highWaterSequence;
    out.installHighWaterFirst = afterInstall.journal.highWaterSequence === 1 &&
      afterInstall.journal.highWaterHash === input.first.packageSha256;
    out.installNoEvaluation =
      afterInstall.evaluations === baselineEvaluations;
    phase = "enable";
    const enabled = await facade.enable(
      "m3d-composition-enable",
      afterInstall.revision
    );
    out.enableStatus = [
      "pending",
      "committed_pending_publish",
      "succeeded",
      "failed_restored",
      "failed_quarantined",
      "storage_failed",
    ].includes(enabled.status) ? enabled.status : "unknown";
    out.enableSucceeded = out.enableStatus === "succeeded";
    const afterEnable = await facadeSnapshot(facade, testId, featureId);
    out.enableEvaluatedOnce =
      afterEnable.evaluations === baselineEvaluations + 1;
    out.enableSelectedFirst =
      afterEnable.selectedPackageSha256 === input.first.packageSha256;
    out.enableDesiredEnabled = afterEnable.desiredEnabled === true;
    out.enableHealthVerified = afterEnable.selectedHealth === "verified";
    phase = "stage-second";
    const stagedTwo = await facade.stage(secondBytes);
    out.stageTwoSucceeded =
      stagedTwo.packageSha256 === input.second.packageSha256;
    const afterStageTwo = await facadeSnapshot(facade, testId, featureId);
    out.stageTwoJournalUnchanged =
      JSON.stringify(afterStageTwo.journal) ===
        JSON.stringify(afterEnable.journal);
    out.stageTwoNoEvaluation =
      afterStageTwo.evaluations === afterEnable.evaluations;
    out.stageTwoRevisionUnchanged =
      afterStageTwo.revision === afterEnable.revision;
    phase = "update";
    const updated = await facade.update(
      "m3d-composition-update",
      afterStageTwo.revision,
      input.second.packageSha256
    );
    out.updateStatus = [
      "pending",
      "committed_pending_publish",
      "succeeded",
      "failed_restored",
      "failed_quarantined",
      "storage_failed",
    ].includes(updated.status) ? updated.status : "unknown";
    out.updateSucceeded = out.updateStatus === "succeeded";
    const afterUpdate = await facadeSnapshot(facade, testId, featureId);
    out.updateEvaluatedOnce =
      afterUpdate.evaluations === afterEnable.evaluations + 1;
    out.updateSelectedSecond =
      afterUpdate.selectedPackageSha256 === input.second.packageSha256;
    out.updateDesiredEnabled = afterUpdate.desiredEnabled === true;
    out.updateHealthVerified = afterUpdate.selectedHealth === "verified";
    out.updateHighWaterSequence = afterUpdate.journal.highWaterSequence;
    out.updateHighWaterSecond = afterUpdate.journal.highWaterSequence === 2 &&
      afterUpdate.journal.highWaterHash === input.second.packageSha256;
    phase = "rollback";
    const rolled = await facade.rollback(
      "m3d-composition-rollback",
      afterUpdate.revision
    );
    out.rollbackStatus = [
      "pending",
      "committed_pending_publish",
      "succeeded",
      "failed_restored",
      "failed_quarantined",
      "storage_failed",
    ].includes(rolled.status) ? rolled.status : "unknown";
    out.rollbackSucceeded = out.rollbackStatus === "succeeded";
    const afterRollback = await facadeSnapshot(facade, testId, featureId);
    out.rollbackEvaluatedOnce =
      afterRollback.evaluations === afterUpdate.evaluations + 1;
    out.rollbackSelectedFirst =
      afterRollback.selectedPackageSha256 === input.first.packageSha256;
    out.rollbackHealthVerified =
      afterRollback.selectedHealth === "verified";
    out.rollbackDesiredEnabled = afterRollback.desiredEnabled === true;
    out.rollbackHighWaterStaysSecond =
      afterRollback.journal.highWaterSequence === 2 &&
      afterRollback.journal.highWaterHash === input.second.packageSha256;
    phase = "disable";
    const disabled = await facade.disable(
      "m3d-composition-disable",
      afterRollback.revision
    );
    out.disableStatus = [
      "pending",
      "committed_pending_publish",
      "succeeded",
      "failed_restored",
      "failed_quarantined",
      "storage_failed",
    ].includes(disabled.status) ? disabled.status : "unknown";
    out.disableSucceeded = out.disableStatus === "succeeded";
    const afterDisable = await facadeSnapshot(facade, testId, featureId);
    out.disableNoEvaluation =
      afterDisable.evaluations === afterRollback.evaluations;
    out.disableDesiredFalse = afterDisable.desiredEnabled === false;
    out.disableSelectedFirst =
      afterDisable.selectedPackageSha256 === input.first.packageSha256;
    out.disableHighWaterStaysSecond =
      afterDisable.journal.highWaterSequence === 2 &&
      afterDisable.journal.highWaterHash === input.second.packageSha256;
    const progression = [
      afterInstall,
      afterEnable,
      afterUpdate,
      afterRollback,
      afterDisable
    ];
    out.revisionMatchesJournal = progression.every(
      snapshot => snapshot.revision === snapshot.journal.revision
    );
    out.revisionIncreased = progression.every(
      (snapshot, index) =>
        index === 0 || snapshot.revision > progression[index - 1].revision
    );
    out.finalRevision = afterDisable.revision;
    out.finalEvaluations = afterDisable.evaluations;
    phase = "final-snapshot";
    out.sentinelFalseAtEnd =
      Services.prefs.getBoolPref(probePref, false) === false;
  } catch (error) {
    out.scriptFailure = true;
    out.failurePhase = phase;
    out.failureCategory = facadeErrorCode(error) ?? "unexpected";
    if (registeredState) registeredState.lifecycleScriptPending = false;
    const window = Services.wm.getMostRecentWindow("navigator:browser");
    if (stateFor(window, testId) === null) {
      if (hadProbe) Services.prefs.setBoolPref(probePref, previousProbe);
      else Services.prefs.clearUserPref(probePref);
    }
  }
  if (registeredState) registeredState.lifecycleScriptPending = false;
  return out;
`;

const CLOSE_START_SCRIPT = `
${SCRIPT_HELPERS}
  const window = Services.wm.getMostRecentWindow("navigator:browser");
  const testId = input.testId;
  const featureId = input.featureId;
  const probePref = input.probePref;
  const existingRegistry = window["__floorpM3dCompositionPending"];
  if (existingRegistry && existingRegistry[testId]) {
    return {started: false, stateConflict: true, scriptFailure: false};
  }
  const out = {started: false, stateConflict: false, facadeCreated: false,
    scriptFailure: false};
  const probeHadUserValue = Services.prefs.prefHasUserValue(probePref);
  const probePreviousValue = Services.prefs.getBoolPref(probePref, false);
  const state = {
    kind: "close",
    testId,
    featureId,
    probePref,
    probeHadUserValue,
    probePreviousValue,
    facade: null,
    abortController: new window.AbortController(),
    creationPending: true,
    startSettled: false,
    cleanupRequested: false,
    facadeCreated: false,
    downloadStarted: false,
    outcome: null,
    bytes: null,
    errorCode: null,
    closeResolved: false,
    baselineEvaluations: -1,
    baselineMetadata: null,
    baselineDiagnostics: null,
    baselineRevision: -1,
    baselineMetadataCount: -1
  };
  windowState(window)[testId] = state;
  try {
    Services.prefs.clearUserPref(probePref);
    const baselineEvaluations = featureHost.m0PayloadEvaluationsForTests();
    const facade = await createFacade(testId, featureId, input.policy);
    state.facade = facade;
    state.facadeCreated = true;
    state.creationPending = false;
    out.facadeCreated = true;
    state.baselineEvaluations = baselineEvaluations;
    if (state.cleanupRequested) {
      state.startSettled = true;
      return out;
    }
    const baseline = await facadeSnapshot(facade, testId, featureId);
    state.baselineMetadata = baseline.metadata;
    state.baselineDiagnostics = baseline.diagnostics;
    state.baselineRevision = baseline.revision;
    state.baselineMetadataCount = baseline.metadataCount;
    out.baselineJournalExists = baseline.journal.exists;
    out.baselineEvaluations = baselineEvaluations;
    if (state.cleanupRequested) {
      state.startSettled = true;
      return out;
    }
    const promise = downloader.downloadFeaturePackageBytes({
      origin: input.origin,
      packageSha256: input.packageSha256,
      policy: input.policy,
      signal: state.abortController.signal
    });
    state.promise = promise;
    state.downloadStarted = true;
    state.promise.then(
      bytes => {
        state.bytes = bytes;
        state.outcome = "resolved";
      },
      error => {
        state.errorCode = downloaderErrorCode(error);
        state.outcome = "rejected";
      }
    );
    out.started = true;
    state.startSettled = true;
  } catch (error) {
    out.scriptFailure = true;
    out.failureCategory = facadeErrorCode(error) ?? "unexpected";
    state.creationPending = false;
    state.startSettled = true;
    state.startFailureCategory = out.failureCategory;
  }
  return out;
`;

const CLOSE_READING_SCRIPT = `
${SCRIPT_HELPERS}
  const window = Services.wm.getMostRecentWindow("navigator:browser");
  const state = stateFor(window, input.testId);
  if (!state) {
    return {stateFound: false, readingObserved: false,
      outcomeSettled: false, scriptFailure: false};
  }
  const deadline = ChromeUtils.now() + input.timeoutMs;
  let diagnostics = downloaderDiagnostics();
  while (
    diagnostics && diagnostics.reading === 0 && state.outcome === null &&
    ChromeUtils.now() < deadline
  ) {
    await new Promise(resolve => window.setTimeout(resolve, 10));
    diagnostics = downloaderDiagnostics();
  }
  return {
    stateFound: true,
    readingObserved: diagnostics !== null && diagnostics.reading > 0,
    outcomeSettled: state.outcome !== null,
    scriptFailure: false
  };
`;

const CLOSE_FACADE_SCRIPT = `
${SCRIPT_HELPERS}
  const window = Services.wm.getMostRecentWindow("navigator:browser");
  const state = stateFor(window, input.testId);
  if (!state) {
    return {stateFound: false, closeResolved: false, cleanupProven: false,
      rootsRemoved: false, rootsAbsent: false,
      outcomeSettledBeforeClose: false, scriptFailure: false};
  }
  const out = {
    stateFound: true,
    closeResolved: false,
    cleanupProven: false,
    rootsRemoved: false,
    rootsAbsent: false,
    outcomeSettledBeforeClose: state.outcome !== null,
    scriptFailure: false
  };
  const registry = windowState(window);
  try {
    await state.facade.close();
    out.closeResolved = true;
    state.closeResolved = true;
  } catch (error) {
    out.closeErrorCode = facadeErrorCode(error);
  }
  try {
    out.cleanupProven =
      state.facade.cleanupDiagnosticsForTests().proven === true;
  } catch {
    out.cleanupProven = false;
  }
  if (out.closeResolved && out.cleanupProven) {
    try {
      out.rootsRemoved = await removeOwnedRoots(state.testId);
    } catch {
      out.rootsRemoved = false;
    }
    try {
      out.rootsAbsent = await ownedRootsAbsent(state.testId);
    } catch {
      out.rootsAbsent = false;
    }
  }
  return out;
`;

const CLOSE_FINISH_SCRIPT = `
${SCRIPT_HELPERS}
  const window = Services.wm.getMostRecentWindow("navigator:browser");
  const state = stateFor(window, input.testId);
  if (!state) {
    return {stateFound: false, outcome: "missing", scriptFailure: false};
  }
  const out = {stateFound: true, scriptFailure: false};
  let phase = "wait-outcome";
  try {
    const deadline = ChromeUtils.now() + input.timeoutMs;
    while (state.outcome === null && ChromeUtils.now() < deadline) {
      await new Promise(resolve => window.setTimeout(resolve, 10));
    }
    out.outcome = state.outcome === "resolved" || state.outcome === "rejected"
      ? state.outcome
      : "pending";
    out.errorCode = state.errorCode;
    const expected = decode(input.expectedBytes);
    const bytes = state.bytes;
    out.bytesExact = out.outcome === "resolved" && bytes !== null &&
      bytes.byteLength === expected.byteLength &&
      bytes.every((value, index) => value === expected[index]);
    out.hashMatches = out.outcome === "resolved" && bytes !== null &&
      await digest(bytes) === input.packageSha256;
    out.stageRejectedCode = null;
    if (out.outcome === "resolved" && bytes !== null) {
      phase = "stage-after-close";
      try {
        await state.facade.stage(bytes);
        out.stageRejected = false;
      } catch (error) {
        out.stageRejected = true;
        out.stageRejectedCode = facadeErrorCode(error);
      }
    } else {
      out.stageRejected = false;
    }
    out.rootsAbsent = await ownedRootsAbsent(state.testId);
    let metadata = null;
    let diagnosticsSnapshot = null;
    try {
      metadata = JSON.parse(JSON.stringify(state.facade.metadata()));
      diagnosticsSnapshot =
        JSON.parse(JSON.stringify(state.facade.diagnostics()));
    } catch {
      metadata = null;
      diagnosticsSnapshot = null;
    }
    out.metadataUnchanged =
      JSON.stringify(metadata) === JSON.stringify(state.baselineMetadata);
    out.diagnosticsUnchanged =
      JSON.stringify(diagnosticsSnapshot) ===
        JSON.stringify(state.baselineDiagnostics);
    out.revisionUnchanged =
      diagnosticsSnapshot !== null &&
      diagnosticsSnapshot.revision === state.baselineRevision;
    out.evaluationsUnchanged =
      featureHost.m0PayloadEvaluationsForTests() === state.baselineEvaluations;
    out.sentinelFalse =
      Services.prefs.getBoolPref(input.probePref, false) === false;
    const diagnostics = downloaderDiagnostics();
    out.downloaderDiagnosticsZero = diagnostics !== null &&
      countNames.every(name => diagnostics[name] === 0);
  } catch (error) {
    out.scriptFailure = true;
    out.failurePhase = phase;
    out.failureCategory = facadeErrorCode(error) ?? "unexpected";
  }
  return out;
`;

const CLOSE_CLEANUP_SCRIPT = `
${SCRIPT_HELPERS}
  const window = Services.wm.getMostRecentWindow("navigator:browser");
  const state = stateFor(window, input.testId);
  if (!state) {
    return {stateFound: false, stateDeleted: false,
      pendingWorkSettled: false, downloaderDiagnosticsZero: false,
      closeResolved: false, cleanupProven: false, rootsRemoved: false,
      rootsAbsent: false, probeRestored: true, scriptFailure: false};
  }
  const registry = windowState(window);
  const out = {
    stateFound: true,
    stateDeleted: false,
    pendingWorkSettled: false,
    downloaderDiagnosticsZero: false,
    closeResolved: false,
    cleanupProven: false,
    rootsRemoved: false,
    rootsAbsent: false,
    probeRestored: false,
    scriptFailure: false
  };
  try {
    const workPending = () => state.kind === "lifecycle"
      ? state.creationPending || state.lifecycleScriptPending ||
        state.downloadPending
      : !state.startSettled ||
        state.downloadStarted && state.outcome === null;
    if (!input.successfulPath) {
      state.cleanupRequested = true;
      if (state.kind === "lifecycle" && state.downloadPending) {
        state.activeController?.abort();
      } else if (
        state.kind === "close" && state.downloadStarted &&
        state.outcome === null
      ) {
        state.abortController.abort();
      }
    }
    const deadline = ChromeUtils.now() + input.timeoutMs;
    while (workPending() && ChromeUtils.now() < deadline) {
      await new Promise(resolve => window.setTimeout(resolve, 10));
    }
    out.pendingWorkSettled = !workPending();
    if (state.facade) {
      try {
        await state.facade.close();
        state.closeResolved = true;
        out.closeResolved = true;
      } catch (error) {
        out.closeErrorCode = facadeErrorCode(error);
      }
    } else if (!state.facadeCreated && !state.creationPending) {
      out.closeResolved = true;
    }
    if (state.facade) {
      try {
        out.cleanupProven =
          state.facade.cleanupDiagnosticsForTests().proven === true;
      } catch {
        out.cleanupProven = false;
      }
    } else {
      out.cleanupProven = !state.facadeCreated && !state.creationPending;
    }
    let diagnostics = downloaderDiagnostics();
    while (
      diagnostics && !countNames.every(name => diagnostics[name] === 0) &&
      ChromeUtils.now() < deadline
    ) {
      await new Promise(resolve => window.setTimeout(resolve, 10));
      diagnostics = downloaderDiagnostics();
    }
    out.downloaderDiagnosticsZero = diagnostics !== null &&
      countNames.every(name => diagnostics[name] === 0);
    if (
      out.closeResolved && out.cleanupProven && out.pendingWorkSettled &&
      out.downloaderDiagnosticsZero
    ) {
      out.rootsRemoved = await removeOwnedRoots(input.testId);
      out.rootsAbsent = await ownedRootsAbsent(input.testId);
    } else {
      out.rootsAbsent = await ownedRootsAbsent(input.testId);
    }
  } catch (error) {
    out.scriptFailure = true;
    out.failureCategory = facadeErrorCode(error) ?? "unexpected";
  } finally {
    try {
      restoreProbe(state);
      out.probeRestored = true;
    } catch {
      out.probeRestored = false;
    }
  }
  if (
    !out.scriptFailure && out.closeResolved && out.cleanupProven &&
    out.pendingWorkSettled && out.downloaderDiagnosticsZero &&
    out.rootsAbsent && out.probeRestored &&
    registry[input.testId] === state
  ) {
    delete registry[input.testId];
    if (Object.keys(registry).length === 0) {
      delete window["__floorpM3dCompositionPending"];
    }
    out.stateDeleted = stateFor(window, input.testId) === null;
  }
  return out;
`;

function booleansTrue(
  value: Record<string, unknown>,
  keys: readonly string[],
): boolean {
  return keys.every((key) => value[key] === true);
}

function numberOf(value: Record<string, unknown>, key: string): number {
  const entry = value[key];
  return typeof entry === "number" && Number.isFinite(entry) ? entry : -1;
}

function stringOf(value: Record<string, unknown>, key: string): string {
  const entry = value[key];
  return typeof entry === "string" ? entry : "";
}

function toBase64(bytes: Uint8Array): string {
  let text = "";
  for (const value of bytes) text += String.fromCharCode(value);
  return btoa(text);
}

async function runLifecycleCases(
  context: M3dCompositionContext,
  testId: string,
): Promise<void> {
  const [first, second] = context.packages;
  const baseline = context.fixture.snapshot();
  let result: Record<string, unknown> = {};
  let downloadResult: Record<string, unknown> = {};
  let cleanup: Record<string, unknown> = {};
  let downloadRpcFailure = false;
  let transactionRpcFailure = false;
  let transactionAttempted = false;
  let diagnosticsZeroBeforeStage = false;
  let fixtureIdleBeforeStage = false;
  let diagnosticsZeroAfterTransactions = false;
  let fixtureIdleAfterTransactions = false;
  let trustedRequests = 0;
  let successfulPath = false;
  const transactionFields = [
    "stageOneSucceeded",
    "stageOneJournalUnchanged",
    "stageOneNoEvaluation",
    "stageOneRevisionUnchanged",
    "installSucceeded",
    "installRevisionTwo",
    "installSelectedFirst",
    "installDisabled",
    "installHighWaterFirst",
    "installNoEvaluation",
    "enableSucceeded",
    "enableEvaluatedOnce",
    "enableSelectedFirst",
    "enableDesiredEnabled",
    "enableHealthVerified",
    "stageTwoSucceeded",
    "stageTwoJournalUnchanged",
    "stageTwoNoEvaluation",
    "stageTwoRevisionUnchanged",
    "updateSucceeded",
    "updateEvaluatedOnce",
    "updateSelectedSecond",
    "updateHighWaterSecond",
    "updateDesiredEnabled",
    "updateHealthVerified",
    "rollbackSucceeded",
    "rollbackEvaluatedOnce",
    "rollbackSelectedFirst",
    "rollbackHealthVerified",
    "rollbackDesiredEnabled",
    "rollbackHighWaterStaysSecond",
    "disableSucceeded",
    "disableNoEvaluation",
    "disableDesiredFalse",
    "disableSelectedFirst",
    "disableHighWaterStaysSecond",
    "revisionMatchesJournal",
    "revisionIncreased",
    "sentinelFalseAtEnd",
  ] as const;
  try {
    context.fixture.setMode(context.validMode);
    try {
      result = await context.browserOperation(
        LIFECYCLE_SCRIPT,
        {
          phase: "download",
          testId,
          featureId: context.featureId,
          policy: context.policy,
          origin: context.fixture.trustedOrigin,
          probePref: context.sourceProbePref,
          first: {
            packageSha256: first.packageSha256,
            version: first.version,
            bytes: toBase64(first.bytes),
          },
          second: {
            packageSha256: second.packageSha256,
            version: second.version,
            bytes: toBase64(second.bytes),
          },
        },
        120_000,
      );
      downloadResult = result;
    } catch {
      downloadRpcFailure = true;
      downloadResult = result;
    }
    diagnosticsZeroBeforeStage = await context.waitForNativeDiagnosticsZero(
      8_000,
    );
    fixtureIdleBeforeStage = await context.waitForFixtureIdle(8_000);
    const afterDownloads = context.fixture.snapshot();
    trustedRequests = afterDownloads.observations
      .slice(baseline.observations.length)
      .filter((entry) =>
        entry.origin === "trusted" && entry.mode === context.validMode
      ).length;
    const downloadReady = !downloadRpcFailure &&
      downloadResult.scriptFailure !== true &&
      downloadResult.diagnosticsZeroAfterDownloads === true &&
      diagnosticsZeroBeforeStage && fixtureIdleBeforeStage &&
      trustedRequests === 2 &&
      booleansTrue(downloadResult, [
        "facadeCreated",
        "baselineStateValid",
        "firstBytesExact",
        "secondBytesExact",
        "firstHashMatches",
        "secondHashMatches",
        "snapshotUnchangedAfterDownloads",
        "noEvaluationAfterDownloads",
        "sentinelFalseAfterDownloads",
      ]);
    if (downloadReady) {
      transactionAttempted = true;
      try {
        result = await context.browserOperation(
          LIFECYCLE_SCRIPT,
          {
            phase: "transactions",
            testId,
            featureId: context.featureId,
            probePref: context.sourceProbePref,
          },
          120_000,
        );
      } catch {
        transactionRpcFailure = true;
        result = { ...result, scriptFailure: true };
      }
      diagnosticsZeroAfterTransactions = await context
        .waitForNativeDiagnosticsZero(8_000);
      fixtureIdleAfterTransactions = await context.waitForFixtureIdle(8_000);
      successfulPath = !transactionRpcFailure &&
        result.scriptFailure !== true &&
        booleansTrue(result, transactionFields) &&
        diagnosticsZeroAfterTransactions && fixtureIdleAfterTransactions;
    }
  } finally {
    try {
      cleanup = await context.browserOperation(
        CLOSE_CLEANUP_SCRIPT,
        { testId, successfulPath, timeoutMs: 12_000 },
        30_000,
      );
    } catch {
      cleanup = {
        stateFound: false,
        stateDeleted: false,
        pendingWorkSettled: false,
        downloaderDiagnosticsZero: false,
        closeResolved: false,
        cleanupProven: false,
        rootsRemoved: false,
        rootsAbsent: false,
        probeRestored: false,
        scriptFailure: true,
      };
    }
    if (!diagnosticsZeroAfterTransactions) {
      diagnosticsZeroAfterTransactions = await context
        .waitForNativeDiagnosticsZero(8_000);
    }
    if (!fixtureIdleAfterTransactions) {
      fixtureIdleAfterTransactions = await context.waitForFixtureIdle(8_000);
    }
  }
  const cleanupPassed = booleansTrue(cleanup, [
    "stateFound",
    "stateDeleted",
    "pendingWorkSettled",
    "downloaderDiagnosticsZero",
    "closeResolved",
    "cleanupProven",
    "rootsRemoved",
    "rootsAbsent",
    "probeRestored",
  ]) && cleanup.scriptFailure !== true;
  const downloadPassed = !downloadRpcFailure &&
    downloadResult.scriptFailure !== true &&
    downloadResult.diagnosticsZeroAfterDownloads === true &&
    diagnosticsZeroBeforeStage && fixtureIdleBeforeStage &&
    trustedRequests === 2 &&
    booleansTrue(downloadResult, [
      "facadeCreated",
      "baselineStateValid",
      "firstBytesExact",
      "secondBytesExact",
      "firstHashMatches",
      "secondHashMatches",
      "snapshotUnchangedAfterDownloads",
      "noEvaluationAfterDownloads",
      "sentinelFalseAfterDownloads",
    ]);
  const inertPassed = downloadPassed && cleanupPassed &&
    diagnosticsZeroAfterTransactions && fixtureIdleAfterTransactions;
  const inertObservations: M3dCompositionObservations = {
    scriptFailure: downloadRpcFailure || downloadResult.scriptFailure === true,
    transactionRpcFailure,
    failurePhase: stringOf(downloadResult, "failurePhase"),
    failureCategory: stringOf(downloadResult, "failureCategory"),
    facadeCreated: downloadResult.facadeCreated === true,
    baselineJournalExists: downloadResult.baselineJournalExists === true,
    baselineStateValid: downloadResult.baselineStateValid === true,
    baselineEvaluations: numberOf(downloadResult, "baselineEvaluations"),
    baselineJournalSha256: stringOf(
      downloadResult,
      "baselineJournalSha256",
    ),
    baselineJournalBytes: numberOf(downloadResult, "baselineJournalBytes"),
    baselineCacheSha256: stringOf(downloadResult, "baselineCacheSha256"),
    downloadJournalSha256: stringOf(downloadResult, "downloadJournalSha256"),
    downloadJournalBytes: numberOf(downloadResult, "downloadJournalBytes"),
    downloadCacheSha256: stringOf(downloadResult, "downloadCacheSha256"),
    firstBytesExact: downloadResult.firstBytesExact === true,
    secondBytesExact: downloadResult.secondBytesExact === true,
    firstHashMatches: downloadResult.firstHashMatches === true,
    secondHashMatches: downloadResult.secondHashMatches === true,
    snapshotUnchangedAfterDownloads:
      downloadResult.snapshotUnchangedAfterDownloads === true,
    noEvaluationAfterDownloads:
      downloadResult.noEvaluationAfterDownloads === true,
    sentinelFalseAfterDownloads:
      downloadResult.sentinelFalseAfterDownloads === true,
    diagnosticsZeroInsideNative:
      downloadResult.diagnosticsZeroAfterDownloads === true,
    diagnosticsZeroBeforeStage,
    fixtureIdleBeforeStage,
    trustedRequests,
    cleanupPassed,
    closeResolved: cleanup.closeResolved === true,
    cleanupProven: cleanup.cleanupProven === true,
    rootsRemoved: cleanup.rootsRemoved === true,
    rootsAbsent: cleanup.rootsAbsent === true,
    stateDeleted: cleanup.stateDeleted === true,
    probeRestored: cleanup.probeRestored === true,
    diagnosticsZeroAfterTransactions,
    fixtureIdleAfterTransactions,
  };
  context.record("download-lifecycle-inert", inertPassed, inertObservations);
  if (transactionAttempted) {
    const transactionsPassed = inertPassed &&
      !transactionRpcFailure && result.scriptFailure !== true &&
      booleansTrue(result, transactionFields);
    context.record("download-lifecycle-transactions", transactionsPassed, {
      scriptFailure: transactionRpcFailure || result.scriptFailure === true,
      failurePhase: stringOf(result, "failurePhase"),
      failureCategory: stringOf(result, "failureCategory"),
      inertPrerequisitePassed: downloadPassed,
      stageOneSucceeded: result.stageOneSucceeded === true,
      stageOneJournalUnchanged: result.stageOneJournalUnchanged === true,
      stageOneNoEvaluation: result.stageOneNoEvaluation === true,
      stageOneRevisionUnchanged: result.stageOneRevisionUnchanged === true,
      installStatus: stringOf(result, "installStatus"),
      installRevision: numberOf(result, "installRevision"),
      installRevisionTwo: result.installRevisionTwo === true,
      installSelectedFirst: result.installSelectedFirst === true,
      installDisabled: result.installDisabled === true,
      installHighWaterSequence: numberOf(result, "installHighWaterSequence"),
      installHighWaterFirst: result.installHighWaterFirst === true,
      installNoEvaluation: result.installNoEvaluation === true,
      enableStatus: stringOf(result, "enableStatus"),
      enableEvaluatedOnce: result.enableEvaluatedOnce === true,
      enableSelectedFirst: result.enableSelectedFirst === true,
      enableDesiredEnabled: result.enableDesiredEnabled === true,
      enableHealthVerified: result.enableHealthVerified === true,
      stageTwoSucceeded: result.stageTwoSucceeded === true,
      stageTwoJournalUnchanged: result.stageTwoJournalUnchanged === true,
      stageTwoNoEvaluation: result.stageTwoNoEvaluation === true,
      stageTwoRevisionUnchanged: result.stageTwoRevisionUnchanged === true,
      updateStatus: stringOf(result, "updateStatus"),
      updateEvaluatedOnce: result.updateEvaluatedOnce === true,
      updateSelectedSecond: result.updateSelectedSecond === true,
      updateHighWaterSequence: numberOf(result, "updateHighWaterSequence"),
      updateHighWaterSecond: result.updateHighWaterSecond === true,
      updateDesiredEnabled: result.updateDesiredEnabled === true,
      updateHealthVerified: result.updateHealthVerified === true,
      rollbackStatus: stringOf(result, "rollbackStatus"),
      rollbackEvaluatedOnce: result.rollbackEvaluatedOnce === true,
      rollbackSelectedFirst: result.rollbackSelectedFirst === true,
      rollbackHealthVerified: result.rollbackHealthVerified === true,
      rollbackDesiredEnabled: result.rollbackDesiredEnabled === true,
      rollbackHighWaterStaysSecond:
        result.rollbackHighWaterStaysSecond === true,
      disableStatus: stringOf(result, "disableStatus"),
      disableNoEvaluation: result.disableNoEvaluation === true,
      disableDesiredFalse: result.disableDesiredFalse === true,
      disableSelectedFirst: result.disableSelectedFirst === true,
      disableHighWaterStaysSecond: result.disableHighWaterStaysSecond === true,
      revisionMatchesJournal: result.revisionMatchesJournal === true,
      revisionIncreased: result.revisionIncreased === true,
      sentinelFalseAtEnd: result.sentinelFalseAtEnd === true,
      finalRevision: numberOf(result, "finalRevision"),
      finalEvaluations: numberOf(result, "finalEvaluations"),
      diagnosticsZeroAfterTransactions,
      fixtureIdleAfterTransactions,
      cleanupPassed,
      stateDeleted: cleanup.stateDeleted === true,
    });
  }
}

async function runCloseDuringDownload(
  context: M3dCompositionContext,
  testId: string,
): Promise<void> {
  const [first] = context.packages;
  const baseline = context.fixture.snapshot();
  context.fixture.setMode(context.stalledMode);
  const operationInput = {
    testId,
    featureId: context.featureId,
    policy: context.policy,
    probePref: context.sourceProbePref,
    origin: context.fixture.trustedOrigin,
    packageSha256: first.packageSha256,
  };
  let start: Record<string, unknown> = {};
  let stall: M3dPackageHttpsSnapshot | null = null;
  let reading: Record<string, unknown> = {};
  let closed: Record<string, unknown> = {};
  let released = false;
  let finished: Record<string, unknown> = {};
  let cleanup: Record<string, unknown> = {};
  let diagnosticsZero = false;
  let fixtureIdle = false;
  let successfulPath = false;
  try {
    context.fixture.setMode(context.stalledMode);
    try {
      start = await context.browserOperation(
        CLOSE_START_SCRIPT,
        operationInput,
        60_000,
      );
    } catch {
      start = { started: false, startRpcFailed: true };
    }
    if (start.started === true) {
      stall = await context.waitForActiveStall(baseline, 15_000);
    }
    if (
      stall !== null && stall.activeStreams > 0 &&
      stall.activeConnections > 0
    ) {
      try {
        reading = await context.browserOperation(
          CLOSE_READING_SCRIPT,
          { testId, timeoutMs: 10_000 },
          20_000,
        );
      } catch {
        reading = {
          stateFound: false,
          readingObserved: false,
          outcomeSettled: true,
          scriptFailure: true,
        };
      }
    }
    const readingPending = booleansTrue(reading, [
      "stateFound",
      "readingObserved",
    ]) && reading.outcomeSettled === false &&
      reading.scriptFailure !== true;
    if (start.started === true && readingPending) {
      try {
        closed = await context.browserOperation(
          CLOSE_FACADE_SCRIPT,
          { testId },
          60_000,
        );
      } catch {
        closed = {
          stateFound: false,
          closeResolved: false,
          cleanupProven: false,
          rootsRemoved: false,
          rootsAbsent: false,
          outcomeSettledBeforeClose: true,
          scriptFailure: true,
        };
      }
    }
    const closeReady = booleansTrue(closed, [
      "stateFound",
      "closeResolved",
      "cleanupProven",
      "rootsRemoved",
      "rootsAbsent",
    ]) && closed.outcomeSettledBeforeClose === false &&
      closed.scriptFailure !== true;
    if (closeReady) {
      try {
        await context.fixture.releaseStalls();
        released = true;
      } catch {
        released = false;
      }
      if (released) {
        try {
          finished = await context.browserOperation(
            CLOSE_FINISH_SCRIPT,
            {
              testId,
              timeoutMs: 30_000,
              expectedBytes: toBase64(first.bytes),
              packageSha256: first.packageSha256,
              probePref: context.sourceProbePref,
            },
            60_000,
          );
        } catch {
          finished = {
            stateFound: false,
            outcome: "missing",
            scriptFailure: true,
          };
        }
      }
    }
    diagnosticsZero = await context.waitForNativeDiagnosticsZero(12_000);
    fixtureIdle = await context.waitForFixtureIdle(12_000);
    successfulPath = start.started === true && start.scriptFailure !== true &&
      stall !== null && stall.activeStreams > 0 &&
      stall.activeConnections > 0 && readingPending && closeReady && released &&
      finished.outcome === "resolved" &&
      finished.scriptFailure !== true &&
      booleansTrue(finished, [
        "bytesExact",
        "hashMatches",
        "stageRejected",
        "rootsAbsent",
        "metadataUnchanged",
        "diagnosticsUnchanged",
        "revisionUnchanged",
        "evaluationsUnchanged",
        "sentinelFalse",
        "downloaderDiagnosticsZero",
      ]) && finished.stageRejectedCode === "operation-failed" &&
      diagnosticsZero && fixtureIdle;
  } finally {
    try {
      cleanup = await context.browserOperation(
        CLOSE_CLEANUP_SCRIPT,
        { testId, successfulPath, timeoutMs: 12_000 },
        30_000,
      );
    } catch {
      cleanup = {
        stateFound: false,
        stateDeleted: false,
        pendingWorkSettled: false,
        downloaderDiagnosticsZero: false,
        closeResolved: false,
        cleanupProven: false,
        rootsRemoved: false,
        rootsAbsent: false,
        probeRestored: false,
        scriptFailure: true,
      };
    }
    if (!diagnosticsZero) {
      diagnosticsZero = await context.waitForNativeDiagnosticsZero(8_000);
    }
    if (!fixtureIdle) {
      fixtureIdle = await context.waitForFixtureIdle(8_000);
    }
  }
  const passed = successfulPath &&
    booleansTrue(start, ["facadeCreated"]) &&
    booleansTrue(cleanup, [
      "stateFound",
      "stateDeleted",
      "pendingWorkSettled",
      "downloaderDiagnosticsZero",
      "closeResolved",
      "cleanupProven",
      "rootsRemoved",
      "rootsAbsent",
      "probeRestored",
    ]) && cleanup.scriptFailure !== true &&
    diagnosticsZero && fixtureIdle;
  context.record("close-during-download", passed, {
    startObserved: start.started === true,
    startRpcFailed: start.startRpcFailed === true,
    startScriptFailure: start.scriptFailure === true,
    startFailureCategory: stringOf(start, "failureCategory"),
    stateConflict: start.stateConflict === true,
    facadeCreated: start.facadeCreated === true,
    baselineEvaluations: numberOf(start, "baselineEvaluations"),
    stallObserved: stall !== null,
    stallActiveStreams: stall?.activeStreams ?? -1,
    stallActiveConnections: stall?.activeConnections ?? -1,
    readingObserved: reading.readingObserved === true,
    readingScriptFailure: reading.scriptFailure === true,
    outcomePendingAtRead: reading.outcomeSettled === false,
    outcomePendingAtClose: closed.outcomeSettledBeforeClose === false,
    closeResolved: closed.closeResolved === true,
    closeErrorCode: stringOf(closed, "closeErrorCode"),
    closeScriptFailure: closed.scriptFailure === true,
    cleanupProvenAtClose: closed.cleanupProven === true,
    rootsRemovedAtClose: closed.rootsRemoved === true,
    rootsAbsentAtClose: closed.rootsAbsent === true,
    stallReleased: released,
    outcome: stringOf(finished, "outcome"),
    finishScriptFailure: finished.scriptFailure === true,
    finishFailurePhase: stringOf(finished, "failurePhase"),
    finishFailureCategory: stringOf(finished, "failureCategory"),
    errorCode: stringOf(finished, "errorCode"),
    bytesExact: finished.bytesExact === true,
    hashMatches: finished.hashMatches === true,
    stageRejected: finished.stageRejected === true,
    stageRejectedCode: stringOf(finished, "stageRejectedCode"),
    rootsAbsentAfterAcquisition: finished.rootsAbsent === true,
    metadataUnchanged: finished.metadataUnchanged === true,
    diagnosticsUnchanged: finished.diagnosticsUnchanged === true,
    revisionUnchanged: finished.revisionUnchanged === true,
    evaluationsUnchanged: finished.evaluationsUnchanged === true,
    sentinelFalse: finished.sentinelFalse === true,
    downloaderDiagnosticsZero: finished.downloaderDiagnosticsZero === true,
    stateDeleted: cleanup.stateDeleted === true,
    cleanupScriptFailure: cleanup.scriptFailure === true,
    pendingWorkSettled: cleanup.pendingWorkSettled === true,
    cleanupDiagnosticsZero: cleanup.downloaderDiagnosticsZero === true,
    cleanupCloseResolved: cleanup.closeResolved === true,
    cleanupProven: cleanup.cleanupProven === true,
    rootsRemovedByCleanup: cleanup.rootsRemoved === true,
    rootsAbsentAfterCleanup: cleanup.rootsAbsent === true,
    probeRestored: cleanup.probeRestored === true,
    diagnosticsZero,
    fixtureIdle,
  });
}

export async function runM3dCompositionCases(
  context: M3dCompositionContext,
): Promise<void> {
  await runLifecycleCases(context, crypto.randomUUID());
  await runCloseDuringDownload(context, crypto.randomUUID());
}

export function m3dCompositionNativeScriptsForTests(): M3dCompositionNativeScripts {
  return Object.freeze({
    lifecycle: LIFECYCLE_SCRIPT,
    closeStart: CLOSE_START_SCRIPT,
    closeReading: CLOSE_READING_SCRIPT,
    closeFacade: CLOSE_FACADE_SCRIPT,
    closeFinish: CLOSE_FINISH_SCRIPT,
    cleanup: CLOSE_CLEANUP_SCRIPT,
  });
}
