// SPDX-License-Identifier: MPL-2.0

import {
  basename,
  dirname,
  isAbsolute,
  join,
  normalize,
  resolve,
} from "@std/path";
import {
  killBrowser,
  launchBrowser,
  reservePort,
  resolveExpectedRuntimeBuildId,
  writePrivateProfile,
} from "./hotfix_process_recovery.ts";
import type { BrowserLaunch } from "./hotfix_process_recovery_types.ts";
import { launchM3cBrowserWithEvidence } from "./m3c_signed_process_recovery_helpers.ts";
import { PROJECT_ROOT } from "./defines.ts";
import { signFeaturePackage } from "../../browser-features/modules/modules/feature-host/package/FeaturePackageVerifier.ts";
import type {
  FeaturePackageAuthoringMetadata,
  FeaturePackagePolicy,
} from "../../browser-features/modules/modules/feature-host/package/types.ts";
import { M0_FIXTURES } from "../../browser-features/modules/modules/feature-host/M0Fixtures.sys.mts";
import { startM3dPackageHttpsFixture } from "./m3d_package_https_fixture.ts";
import type {
  M3dPackageHttpsFixture,
  M3dPackageHttpsMode,
  M3dPackageHttpsSnapshot,
} from "./m3d_package_https_fixture.types.ts";
import type {
  M3dAcceptanceCaseResult,
  M3dAcceptanceReport,
} from "./m3d_package_acquisition_acceptance.types.ts";
import { superviseM3dOwnedCommandOperations } from "./m3d_package_acquisition_command.ts";
import { runM3dCompositionCases } from "./m3d_package_acquisition_composition.ts";
import type {
  M3dCommandReadOperation,
  M3dOwnedCommandOperations,
} from "./m3d_package_acquisition_command.types.ts";
import { M3D_REQUIRED_CASE_IDS } from "./m3d_package_acquisition_acceptance.types.ts";
import {
  m3dAcceptanceVerdict,
  m3dProfileSnapshotsEqual,
  snapshotProfileTree,
} from "./m3d_package_acquisition_acceptance_helpers.ts";

const MODULE_URI =
  "resource://noraneko/modules/feature-host/package/FeaturePackageDownloader.sys.mjs";
const FEATURE_HOST_URI =
  "resource://noraneko/modules/feature-host/FeatureHost.sys.mjs";
const FEATURE_ID = "m3d-transport-acceptance";
const KEY_ID = "m3d-transport-acceptance-key";
const SOURCE_PROBE_PREF = "nora.m3a.package_source_execution_probe";
const PENDING_DOWNLOAD_STATE_KEY = "__floorpM3dPendingDownloads";
const SHARED_PROFILE = join(PROJECT_ROOT, "_dist", "profile", "test");
const RUN_ROOT = join(PROJECT_ROOT, "_dist", "m3d-package-acquisition-runs");
let ownedCommandsCleanupProven = true;

function toHex(bytes: Uint8Array): string {
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0"))
    .join("");
}

function toBase64(bytes: Uint8Array): string {
  let binary = "";
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary);
}

async function sha256(bytes: Uint8Array): Promise<string> {
  return toHex(
    new Uint8Array(await crypto.subtle.digest("SHA-256", bytes.slice())),
  );
}

function readBounded(
  stream: ReadableStream<Uint8Array>,
  limit: number,
): M3dCommandReadOperation {
  const reader = stream.getReader();
  let readerReleased = false;
  const promise = (async (): Promise<Uint8Array> => {
    const chunks: Uint8Array[] = [];
    let total = 0;
    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        total += value.byteLength;
        if (total > limit) {
          throw new Error("owned command output exceeded limit");
        }
        chunks.push(value);
      }
      const output = new Uint8Array(total);
      let offset = 0;
      for (const chunk of chunks) {
        output.set(chunk, offset);
        offset += chunk.byteLength;
      }
      return output;
    } finally {
      reader.releaseLock();
      readerReleased = true;
    }
  })();
  return {
    promise,
    cancel: async () => {
      if (readerReleased) {
        await stream.cancel();
      } else {
        await reader.cancel();
      }
    },
  };
}

async function boundedCommand(
  command: string,
  args: string[],
  cwd?: string,
): Promise<{ readonly stdout: string; readonly success: boolean }> {
  const child = new Deno.Command(command, {
    args,
    cwd,
    stdout: "piped",
    stderr: "piped",
    stdin: "null",
  }).spawn();
  const emptyRead: M3dCommandReadOperation = {
    promise: Promise.resolve(new Uint8Array()),
    cancel: async () => {},
  };
  const operations: M3dOwnedCommandOperations = {
    stdout: child.stdout ? readBounded(child.stdout, 64 * 1024) : emptyRead,
    stderr: child.stderr ? readBounded(child.stderr, 64 * 1024) : emptyRead,
    status: child.status,
    terminate: () => child.kill("SIGKILL"),
  };
  const result = await superviseM3dOwnedCommandOperations(
    operations,
    15_000,
    5_000,
  );
  if (!result.cleanupProven) ownedCommandsCleanupProven = false;
  if (
    !result.completed || !result.cleanupProven || result.stdout === null ||
    result.success === null
  ) {
    throw new Error("owned command failed or timed out");
  }
  return {
    stdout: new TextDecoder().decode(result.stdout),
    success: result.success,
  };
}

async function checkedCommand(
  command: string,
  args: string[],
  cwd?: string,
): Promise<string> {
  const result = await boundedCommand(command, args, cwd);
  if (!result.success) throw new Error("owned command failed");
  return result.stdout;
}

async function canonicalPath(path: string): Promise<string> {
  let current = normalize(resolve(path));
  const missing: string[] = [];
  while (true) {
    try {
      const resolved = await Deno.realPath(current);
      return normalize(join(resolved, ...missing.reverse()));
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error;
      const parent = dirname(current);
      if (parent === current) throw error;
      missing.push(basename(current));
      current = parent;
    }
  }
}

function portIsClosed(port: number): boolean {
  try {
    const listener = Deno.listen({
      transport: "tcp",
      hostname: "127.0.0.1",
      port,
    });
    listener.close();
    return true;
  } catch {
    return false;
  }
}

async function runtimeBuildId(): Promise<string> {
  const [source, installed] = await Promise.all([
    Deno.readFile(join(PROJECT_ROOT, "floorp-runtime.lock.json")),
    Deno.readFile(
      join(PROJECT_ROOT, "_dist", "bin", ".floorp-runtime-lock.json"),
    ),
  ]);
  return resolveExpectedRuntimeBuildId(source, installed);
}

function asyncScript(body: string): string {
  return `
    const done = arguments[arguments.length - 1];
    const input = arguments[0];
    Promise.resolve()
      .then(async () => { ${body} })
      .then(value => done(value), () => done({ scriptFailure: true }));
  `;
}

async function browserOperation(
  launch: BrowserLaunch,
  body: string,
  args: unknown,
  timeoutMs = 30_000,
): Promise<Record<string, unknown>> {
  const result: unknown = await launch.client.sendWithTimeout(
    "WebDriver:ExecuteAsyncScript",
    { script: asyncScript(body), args: [args] },
    timeoutMs,
  );
  if (typeof result !== "object" || result === null || !("value" in result)) {
    throw new Error("Marionette returned no acceptance result");
  }
  const value: unknown = result.value;
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    throw new Error("Marionette returned an invalid acceptance result");
  }
  return value as Record<string, unknown>;
}

async function waitForActiveStall(
  fixture: M3dPackageHttpsFixture,
  baseline: M3dPackageHttpsSnapshot,
  timeoutMs: number,
): Promise<M3dPackageHttpsSnapshot | null> {
  const deadline = performance.now() + timeoutMs;
  while (performance.now() < deadline) {
    const snapshot = fixture.snapshot();
    const stallObserved = snapshot.observations
      .slice(baseline.observations.length)
      .some((entry) =>
        entry.origin === "trusted" && entry.mode === "stalled-body"
      );
    if (
      snapshot.requests === baseline.requests + 1 &&
      !snapshot.observationOverflow &&
      stallObserved && snapshot.activeStreams > 0
    ) return snapshot;
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  return null;
}

async function waitForFixtureIdle(
  fixture: M3dPackageHttpsFixture,
  timeoutMs: number,
): Promise<boolean> {
  const deadline = performance.now() + timeoutMs;
  while (performance.now() < deadline) {
    const snapshot = fixture.snapshot();
    if (snapshot.activeStreams === 0 && snapshot.activeConnections === 0) {
      return true;
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  return false;
}

async function waitForFixturePeerCancellation(
  fixture: M3dPackageHttpsFixture,
  cancellationBaseline: number,
  timeoutMs: number,
): Promise<M3dPackageHttpsSnapshot | null> {
  const deadline = performance.now() + timeoutMs;
  while (performance.now() < deadline) {
    const snapshot = fixture.snapshot();
    if (
      snapshot.cancellations > cancellationBaseline &&
      snapshot.activeStreams === 0 && snapshot.activeConnections === 0
    ) return snapshot;
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  return null;
}

async function startPendingDownload(
  launch: BrowserLaunch,
  runId: string,
  request: unknown,
  installAbortListener: boolean,
): Promise<Record<string, unknown>> {
  return await browserOperation(
    launch,
    `
      const window = Services.wm.getMostRecentWindow("navigator:browser");
      const key = ${JSON.stringify(PENDING_DOWNLOAD_STATE_KEY)};
      const registry = window[key] || (window[key] = Object.create(null));
      if (Object.prototype.hasOwnProperty.call(registry, input.runId)) {
        return {started: false};
      }
      const module = ChromeUtils.importESModule(${JSON.stringify(MODULE_URI)});
      const controller = new window.AbortController();
      if (input.installAbortListener) {
        controller.signal.addEventListener(
          "abort",
          event => event.stopImmediatePropagation(),
          {once: true}
        );
      }
      const state = {
        controller,
        startedAt: ChromeUtils.now(),
        outcome: null,
        promise: null
      };
      registry[input.runId] = state;
      const knownCodes = new Set([
        "invalid-request", "invalid-endpoint", "invalid-policy", "busy",
        "aborted", "timeout", "network-failure", "unacceptable-response",
        "size-limit", "package-rejected", "expected-package-mismatch",
        "unsupported-platform", "cleanup-unproven"
      ]);
      let requestPromise;
      try {
        requestPromise = module.downloadFeaturePackageBytes({
          ...input.request,
          signal: controller.signal
        });
      } catch (error) {
        requestPromise = Promise.reject(error);
      }
      state.promise = Promise.resolve(requestPromise).then(
        () => ({
          ok: true,
          code: null,
          fixedMessage: false,
          reasonLeaked: false,
          settledAt: ChromeUtils.now()
        }),
        error => ({
          ok: false,
          code: knownCodes.has(error?.code) ? error.code : null,
          fixedMessage: error?.message === "Package download was aborted.",
          reasonLeaked: String(error).includes("m3d-caller-abort-sentinel"),
          settledAt: ChromeUtils.now()
        })
      ).then(outcome => {
        state.outcome = outcome;
        return outcome;
      });
      return {started: true};
    `,
    { runId, request, installAbortListener },
  );
}

async function waitForPendingReading(
  launch: BrowserLaunch,
  runId: string,
  timeoutMs: number,
): Promise<Record<string, unknown>> {
  return await browserOperation(
    launch,
    `
      const window = Services.wm.getMostRecentWindow("navigator:browser");
      const registry = window[${JSON.stringify(PENDING_DOWNLOAD_STATE_KEY)}];
      const state = registry && registry[input.runId];
      if (!state) return {stateFound: false, readingObserved: false};
      const module = ChromeUtils.importESModule(${JSON.stringify(MODULE_URI)});
      const pref = "nora.startup.mode";
      const readDiagnostics = () => {
        const hadMode = Services.prefs.prefHasUserValue(pref);
        const previousMode = Services.prefs.getStringPref(pref, "");
        try {
          Services.prefs.setStringPref(pref, "test");
          return module.featurePackageDownloadDiagnosticsForTests();
        } finally {
          if (hadMode) Services.prefs.setStringPref(pref, previousMode);
          else Services.prefs.clearUserPref(pref);
        }
      };
      const deadline = ChromeUtils.now() + input.timeoutMs;
      let diagnostics = readDiagnostics();
      while (
        diagnostics && diagnostics.reading === 0 && state.outcome === null &&
        ChromeUtils.now() < deadline
      ) {
        await new Promise(resolve => window.setTimeout(resolve, 10));
        diagnostics = readDiagnostics();
      }
      return {
        stateFound: true,
        readingObserved: typeof diagnostics === "object" &&
          diagnostics !== null && diagnostics.reading > 0,
        outcomeSettled: state.outcome !== null
      };
    `,
    { runId, timeoutMs },
    timeoutMs + 2_000,
  );
}

async function abortPendingDownload(
  launch: BrowserLaunch,
  runId: string,
): Promise<Record<string, unknown>> {
  return await browserOperation(
    launch,
    `
      const window = Services.wm.getMostRecentWindow("navigator:browser");
      const registry = window[${JSON.stringify(PENDING_DOWNLOAD_STATE_KEY)}];
      const state = registry && registry[input.runId];
      if (!state) return {stateFound: false, abortIssued: false};
      state.controller.abort("m3d-caller-abort-sentinel");
      return {stateFound: true, abortIssued: state.controller.signal.aborted};
    `,
    { runId },
  );
}

async function consumePendingOutcome(
  launch: BrowserLaunch,
  runId: string,
  timeoutMs: number,
): Promise<Record<string, unknown>> {
  return await browserOperation(
    launch,
    `
      const window = Services.wm.getMostRecentWindow("navigator:browser");
      const registry = window[${JSON.stringify(PENDING_DOWNLOAD_STATE_KEY)}];
      const state = registry && registry[input.runId];
      if (!state) return {stateFound: false, outcomeConsumed: false};
      const outcome = await state.promise;
      return {
        stateFound: true,
        outcomeConsumed: true,
        ok: outcome.ok,
        code: outcome.code,
        fixedMessage: outcome.fixedMessage,
        reasonLeaked: outcome.reasonLeaked,
        elapsedMs: outcome.settledAt - state.startedAt
      };
    `,
    { runId },
    timeoutMs,
  );
}

async function waitForNativeDiagnosticsZero(
  launch: BrowserLaunch,
  timeoutMs: number,
): Promise<Record<string, unknown>> {
  return await browserOperation(
    launch,
    `
      const window = Services.wm.getMostRecentWindow("navigator:browser");
      const module = ChromeUtils.importESModule(${JSON.stringify(MODULE_URI)});
      const names = [
        "inFlight",
        "reading",
        "verifying",
        "cleanupPending",
        "blocked"
      ];
      const zero = value => typeof value === "object" && value !== null &&
        names.every(name => value[name] === 0);
      const pref = "nora.startup.mode";
      const readDiagnostics = () => {
        const hadMode = Services.prefs.prefHasUserValue(pref);
        const previousMode = Services.prefs.getStringPref(pref, "");
        try {
          Services.prefs.setStringPref(pref, "test");
          return module.featurePackageDownloadDiagnosticsForTests();
        } finally {
          if (hadMode) Services.prefs.setStringPref(pref, previousMode);
          else Services.prefs.clearUserPref(pref);
        }
      };
      const deadline = ChromeUtils.now() + input.timeoutMs;
      let diagnostics = readDiagnostics();
      while (!zero(diagnostics) && ChromeUtils.now() < deadline) {
        await new Promise(resolve => window.setTimeout(resolve, 10));
        diagnostics = readDiagnostics();
      }
      return {diagnosticsZero: zero(diagnostics)};
    `,
    { timeoutMs },
    timeoutMs + 2_000,
  );
}

async function removePendingDownload(
  launch: BrowserLaunch,
  runId: string,
): Promise<Record<string, unknown>> {
  return await browserOperation(
    launch,
    `
      const window = Services.wm.getMostRecentWindow("navigator:browser");
      const key = ${JSON.stringify(PENDING_DOWNLOAD_STATE_KEY)};
      const registry = window[key];
      const state = registry && registry[input.runId];
      if (!state || state.outcome === null) return {deleted: false};
      delete registry[input.runId];
      if (Object.keys(registry).length === 0) delete window[key];
      return {deleted: true};
    `,
    { runId },
  );
}

async function finishPendingDownload(
  launch: BrowserLaunch,
  fixture: M3dPackageHttpsFixture,
  runId: string,
  cancellationBaseline: number,
  issueAbort: boolean,
  outcomeTimeoutMs: number,
): Promise<Record<string, unknown>> {
  const initialOutcomeTimeoutMs = issueAbort
    ? Math.min(outcomeTimeoutMs, 15_000)
    : outcomeTimeoutMs;
  let abortIssued = false;
  if (issueAbort) {
    try {
      const result = await abortPendingDownload(launch, runId);
      abortIssued = result.abortIssued === true;
    } catch {
      abortIssued = false;
    }
  }
  let outcome: Record<string, unknown> = {};
  try {
    outcome = await consumePendingOutcome(
      launch,
      runId,
      initialOutcomeTimeoutMs,
    );
  } catch {
    outcome = { stateFound: false, outcomeConsumed: false };
  }
  if (outcome.outcomeConsumed !== true) {
    try {
      const result = await abortPendingDownload(launch, runId);
      abortIssued = abortIssued || result.abortIssued === true;
    } catch {
      abortIssued = false;
    }
    try {
      outcome = await consumePendingOutcome(launch, runId, 10_000);
    } catch {
      outcome = { stateFound: false, outcomeConsumed: false };
    }
  }
  let diagnosticsZero = false;
  try {
    diagnosticsZero =
      (await waitForNativeDiagnosticsZero(launch, 8_000)).diagnosticsZero ===
        true;
  } catch {
    diagnosticsZero = false;
  }
  let fixtureAfter: Awaited<ReturnType<typeof waitForFixturePeerCancellation>> =
    null;
  try {
    fixtureAfter = await waitForFixturePeerCancellation(
      fixture,
      cancellationBaseline,
      10_000,
    );
  } catch {
    fixtureAfter = null;
  }
  let stateDeleted = false;
  if (outcome.outcomeConsumed === true) {
    try {
      stateDeleted =
        (await removePendingDownload(launch, runId)).deleted === true;
    } catch {
      stateDeleted = false;
    }
  }
  return {
    ...outcome,
    abortIssued,
    diagnosticsZero,
    fixturePeerCancelled: fixtureAfter !== null,
    activeStreams: fixtureAfter?.activeStreams ?? -1,
    activeConnections: fixtureAfter?.activeConnections ?? -1,
    stateDeleted,
  };
}

function updateCase(
  cases: M3dAcceptanceCaseResult[],
  id: string,
  passed: boolean,
  observations: M3dAcceptanceCaseResult["observations"] = {},
): void {
  const index = cases.findIndex((entry) => entry.id === id);
  if (index < 0) throw new Error("unknown acceptance case");
  const previous = cases[index];
  cases[index] = {
    id,
    status: passed && previous.status !== "failed" ? "passed" : "failed",
    observations: { ...previous.observations, ...observations },
  };
}

async function installPublicTrust(
  profile: string,
  caPem: string,
  runDirectory: string,
): Promise<void> {
  const caPath = join(runDirectory, "public-root-ca.pem");
  await Deno.writeTextFile(caPath, caPem, { mode: 0o600, createNew: true });
  await checkedCommand("certutil", [
    "-N",
    "-d",
    `sql:${profile}`,
    "--empty-password",
  ]);
  await checkedCommand("certutil", [
    "-A",
    "-d",
    `sql:${profile}`,
    "-n",
    "M3d owned test CA",
    "-t",
    "C,,",
    "-i",
    caPath,
  ]);
  const listing = await checkedCommand("certutil", [
    "-L",
    "-d",
    `sql:${profile}`,
  ]);
  if (!/M3d owned test CA\s+C,,/.test(listing)) {
    throw new Error("owned NSS root trust was not verified");
  }
}

async function main(): Promise<void> {
  const startedAt = new Date().toISOString();
  const runId = crypto.randomUUID();
  const runDirectory = join(RUN_ROOT, runId);
  const profileRaw = Deno.env.get("FLOORP_TEST_PROFILE_DIR");
  if (Deno.build.os !== "linux") {
    throw new Error("M3d native acceptance requires Linux");
  }
  if (!profileRaw || !isAbsolute(profileRaw)) {
    throw new Error("FLOORP_TEST_PROFILE_DIR must be an absolute fresh path");
  }
  const profile = await canonicalPath(profileRaw);
  const sharedProfilePath = await canonicalPath(SHARED_PROFILE);
  if (
    profile === sharedProfilePath ||
    profile.startsWith(`${sharedProfilePath}/`)
  ) {
    throw new Error("FLOORP_TEST_PROFILE_DIR must not use the shared profile");
  }
  try {
    await Deno.lstat(profile);
    throw new Error("FLOORP_TEST_PROFILE_DIR must not already exist");
  } catch (error) {
    if (!(error instanceof Deno.errors.NotFound)) throw error;
  }
  await Deno.mkdir(RUN_ROOT, { recursive: true, mode: 0o700 });
  await Deno.mkdir(runDirectory, { mode: 0o700 });
  const cases: M3dAcceptanceCaseResult[] = M3D_REQUIRED_CASE_IDS.map((id) => ({
    id,
    status: "not-run",
    observations: {},
  }));
  const cleanup: Record<string, boolean | number | null> = {
    fixtureClosed: false,
    browserStopped: false,
    profileRemoved: false,
    portsClosed: false,
    fixtureIdleBeforeClose: false,
    fixtureObservationsBounded: false,
    ownedCommandsClosed: false,
  };
  let sharedBefore: Awaited<ReturnType<typeof snapshotProfileTree>> | null =
    null;
  let sharedAfter: Awaited<ReturnType<typeof snapshotProfileTree>> | null =
    null;
  let fixture: M3dPackageHttpsFixture | null = null;
  let fixturePorts: number[] = [];
  let browserPorts: number[] = [];
  let profileOwned = false;
  let launch: BrowserLaunch | null = null;
  let launchAttempted = false;
  let launchSucceeded = false;
  let browserCleanupProven = false;
  let launchFailure: string | null = null;
  let runFailure: string | null = null;
  let reportWritten = false;
  const packages = new Map<string, Uint8Array>();
  const packageMetadata: Array<
    {
      featureId: string;
      version: string;
      releaseSequence: number;
      packageSha256: string;
      byteLength: number;
    }
  > = [];
  const reportPath = join(runDirectory, "report.json");
  const writeReport = async (complete: boolean): Promise<boolean> => {
    const unchanged = sharedBefore && sharedAfter
      ? m3dProfileSnapshotsEqual(sharedBefore, sharedAfter)
      : null;
    const passed = m3dAcceptanceVerdict({
      complete,
      cases,
      launchFailure,
      runFailure,
      launchAttempted,
      launchSucceeded,
      browserCleanupProven,
      sharedProfileBefore: sharedBefore,
      sharedProfileAfter: sharedAfter,
      cleanup,
    });
    const report: M3dAcceptanceReport = {
      schemaVersion: 1,
      runId,
      startedAt,
      completedAt: complete ? new Date().toISOString() : null,
      passed,
      cases,
      packageMetadata,
      sharedProfileBefore: sharedBefore,
      sharedProfileAfter: sharedAfter,
      sharedProfileUnchanged: unchanged,
      launchFailure,
      runFailure,
      launchAttempted,
      launchSucceeded,
      browserCleanupProven,
      cleanup,
    };
    await Deno.writeTextFile(reportPath, JSON.stringify(report, null, 2), {
      mode: 0o600,
    });
    reportWritten = true;
    return passed;
  };
  try {
    sharedBefore = await snapshotProfileTree(SHARED_PROFILE);
    await writeReport(false);
    await Deno.mkdir(profile, { mode: 0o700 });
    profileOwned = true;
    const runtimeBuild = await runtimeBuildId();
    const signer = await crypto.subtle.generateKey({ name: "Ed25519" }, false, [
      "sign",
      "verify",
    ]) as CryptoKeyPair;
    const publicKey = toBase64(
      new Uint8Array(await crypto.subtle.exportKey("raw", signer.publicKey)),
    )
      .replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/g, "");
    const policy: FeaturePackagePolicy = {
      featureId: FEATURE_ID,
      hostSchema: 2,
      hostBuildId: "m0-schema-2",
      runtimeBuildId: runtimeBuild,
      allowedCapabilities: [],
      minimumReleaseSequence: 1,
      trustedKeys: [{ keyId: KEY_ID, publicKey }],
    };
    const sourceByVersion = new Map<string, string>();
    for (
      const [version, sequence, fixtureSource] of [
        ["v1", 1, M0_FIXTURES.v1.source],
        ["v2", 2, M0_FIXTURES.v2.source],
      ] as const
    ) {
      const source = [
        `if (typeof Services !== "undefined") {`,
        `  Services.prefs.setBoolPref("${SOURCE_PROBE_PREF}", true);`,
        `}`,
        fixtureSource,
      ].join("\n");
      sourceByVersion.set(version, source);
      const metadata: FeaturePackageAuthoringMetadata = {
        featureId: FEATURE_ID,
        version,
        releaseSequence: sequence,
        compatibility: {
          hostSchema: 2,
          hostBuildId: "m0-schema-2",
          runtimeBuildIds: [runtimeBuild],
          capabilities: [],
        },
      };
      const bytes = await signFeaturePackage(
        metadata,
        source,
        KEY_ID,
        signer.privateKey,
      );
      const packageSha256 = await sha256(bytes);
      packages.set(packageSha256, bytes);
      packageMetadata.push({
        featureId: FEATURE_ID,
        version,
        releaseSequence: sequence,
        packageSha256,
        byteLength: bytes.length,
      });
    }
    fixture = await startM3dPackageHttpsFixture(packages);
    fixturePorts = Object.values(fixture.snapshot().ports);
    await installPublicTrust(profile, fixture.caCertificatePem, runDirectory);
    const marionettePort = reservePort();
    let osServerPort = reservePort();
    if (osServerPort === marionettePort) osServerPort = reservePort();
    if (osServerPort === marionettePort) {
      throw new Error("could not reserve distinct browser service ports");
    }
    browserPorts = [marionettePort, osServerPort];
    await writePrivateProfile(profile, marionettePort, osServerPort);
    const buildId = await runtimeBuildId();
    const launchId = Date.now();
    const stdoutLogPath = join(runDirectory, `browser-${launchId}.stdout.log`);
    const stderrLogPath = join(runDirectory, `browser-${launchId}.stderr.log`);
    try {
      launchAttempted = true;
      launch = await launchM3cBrowserWithEvidence(
        () =>
          launchBrowser(
            profile,
            marionettePort,
            launchId,
            runDirectory,
            buildId,
          ),
        { launchId, mode: "create", stdoutLogPath, stderrLogPath },
        () => {
          launchFailure = "launch-failed";
        },
      );
      launchSucceeded = true;
    } catch {
      launchFailure = "launch-failed";
      throw new Error("browser launch failed");
    }
    const loaded = await browserOperation(
      launch,
      `
        const module = ChromeUtils.importESModule(${
        JSON.stringify(MODULE_URI)
      });
        const expected =
          "FeaturePackageDownloadError,downloadFeaturePackageBytes,featurePackageDownloadDiagnosticsForTests";
        const exports = Object.keys(module).sort().join(",");
        const pref = "nora.startup.mode";
        const hadMode = Services.prefs.prefHasUserValue(pref);
        const previousMode = Services.prefs.getStringPref(pref, "");
        const startupModeTestAtLaunch = previousMode === "test";
        const countNames = [
          "inFlight",
          "reading",
          "verifying",
          "cleanupPending",
          "blocked"
        ];
        const diagnosticsShape = value => {
          if (typeof value !== "object" || value === null) return false;
          const keys = Reflect.ownKeys(value);
          return keys.length === countNames.length &&
            keys.every(key => typeof key === "string") &&
            keys.sort().join(",") === countNames.slice().sort().join(",") &&
            countNames.every(name =>
              Number.isSafeInteger(value[name]) && value[name] >= 0
            );
        };
        const diagnosticsBeforeGateControl =
          module.featurePackageDownloadDiagnosticsForTests();
        let diagnosticsInTestGate = null;
        let hiddenOutsideTest = false;
        let modeRestored = false;
        try {
          Services.prefs.setStringPref(pref, "test");
          diagnosticsInTestGate =
            module.featurePackageDownloadDiagnosticsForTests();
          Services.prefs.setStringPref(pref, "dev");
          hiddenOutsideTest =
            module.featurePackageDownloadDiagnosticsForTests() === null;
        } finally {
          if (hadMode) Services.prefs.setStringPref(pref, previousMode);
          else Services.prefs.clearUserPref(pref);
          modeRestored = Services.prefs.prefHasUserValue(pref) === hadMode &&
            Services.prefs.getStringPref(pref, "") === previousMode;
        }
        const diagnosticsAfterGateControl =
          module.featurePackageDownloadDiagnosticsForTests();
        const launchGateMatchesStartupMode = startupModeTestAtLaunch
          ? diagnosticsShape(diagnosticsBeforeGateControl)
          : diagnosticsBeforeGateControl === null;
        const gateRestoreMatchesStartupMode = startupModeTestAtLaunch
          ? diagnosticsShape(diagnosticsAfterGateControl)
          : diagnosticsAfterGateControl === null;
        return {
          loaded: typeof module.downloadFeaturePackageBytes === "function",
          errorExport: typeof module.FeaturePackageDownloadError === "function",
          diagnosticsExport: typeof module.featurePackageDownloadDiagnosticsForTests ===
            "function",
          diagnosticsShape: diagnosticsShape(diagnosticsInTestGate),
          launchGateMatchesStartupMode,
          gateRestoreMatchesStartupMode,
          hiddenOutsideTest,
          modeRestored,
          startupModeTestAtLaunch,
          gateControlOnly: true,
          normalStartupQualified: false,
          profilePathMatches: PathUtils.profileDir === input.profile,
          publicExportsOnly: exports === expected,
        };
      `,
      { profile },
    );
    updateCase(
      cases,
      "wrapper-contract",
      loaded.loaded === true && loaded.errorExport === true &&
        loaded.diagnosticsExport === true && loaded.diagnosticsShape === true &&
        loaded.hiddenOutsideTest === true && loaded.modeRestored === true &&
        loaded.launchGateMatchesStartupMode === true &&
        loaded.gateRestoreMatchesStartupMode === true &&
        loaded.gateControlOnly === true &&
        loaded.normalStartupQualified === false &&
        loaded.profilePathMatches === true &&
        loaded.publicExportsOnly === true,
      {
        loaded: loaded.loaded === true,
        errorExport: loaded.errorExport === true,
        diagnosticsExport: loaded.diagnosticsExport === true,
        diagnosticsShape: loaded.diagnosticsShape === true,
        publicExportsOnly: loaded.publicExportsOnly === true,
        hiddenOutsideTest: loaded.hiddenOutsideTest === true,
        modeRestored: loaded.modeRestored === true,
        launchGateMatchesStartupMode:
          loaded.launchGateMatchesStartupMode === true,
        gateRestoreMatchesStartupMode:
          loaded.gateRestoreMatchesStartupMode === true,
        startupModeTestAtLaunch: loaded.startupModeTestAtLaunch === true,
        gateControlOnly: loaded.gateControlOnly === true,
        normalStartupQualified: loaded.normalStartupQualified === true,
        profilePathMatches: loaded.profilePathMatches === true,
      },
    );
    const p1 = packageMetadata[0];
    const p2 = packageMetadata[1];
    const trustedSource = sourceByVersion.get(p1.version);
    if (trustedSource === undefined) {
      throw new Error("trusted package source is unavailable");
    }
    const validMode: M3dPackageHttpsMode = "valid-streaming";
    const download = async (
      id: string,
      mode: M3dPackageHttpsMode,
      origin: string,
      hash: string,
      expectedCode?: string,
    ): Promise<Record<string, unknown>> => {
      const before = fixture!.snapshot();
      fixture!.setMode(mode);
      const expectedBytes = packages.get(hash);
      if (!expectedBytes) throw new Error("expected package bytes unavailable");
      const result = await browserOperation(
        launch!,
        `
          const m = ChromeUtils.importESModule(${JSON.stringify(MODULE_URI)});
          const knownCodes = new Set([
            "invalid-request", "invalid-endpoint", "invalid-policy", "busy",
            "aborted", "timeout", "network-failure", "unacceptable-response",
            "size-limit", "package-rejected", "expected-package-mismatch",
            "unsupported-platform", "cleanup-unproven"
          ]);
          const finiteCode = error => knownCodes.has(error?.code) ? error.code : null;
          const decode = value => Uint8Array.from(atob(value), character => character.charCodeAt(0));
          const expected = decode(input.expectedBytes);
          let outcome;
          try {
            const bytes = await m.downloadFeaturePackageBytes(input.request);
            const hash = Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)), value => value.toString(16).padStart(2, "0")).join("");
            const exactBytes = bytes.length === expected.length &&
              bytes.every((value, index) => value === expected[index]);
            outcome = {ok: true, byteLength: bytes.length, hash, exactBytes};
          } catch (error) {
            outcome = {ok: false, code: finiteCode(error)};
          }
          return outcome;
        `,
        {
          request: { origin, packageSha256: hash, policy },
          expectedBytes: toBase64(expectedBytes),
        },
      );
      let diagnosticsSettled = false;
      try {
        diagnosticsSettled =
          (await waitForNativeDiagnosticsZero(launch!, 5_000))
            .diagnosticsZero === true;
      } catch {
        diagnosticsSettled = false;
      }
      const after = fixture!.snapshot();
      const modeObserved = after.observations.slice(before.observations.length)
        .some((entry) => entry.mode === mode);
      const fixtureEvidenceMatched = origin === fixture!.untrustedOrigin
        ? after.requests === before.requests
        : modeObserved;
      const code = typeof result.code === "string" ? result.code : null;
      const successMatchesPackage = result.ok === true &&
        result.byteLength === expectedBytes.byteLength &&
        result.exactBytes === true && result.hash === hash;
      const pass = expectedCode === "either"
        ? successMatchesPackage || (result.ok === false &&
          (code === "network-failure" || code === "unacceptable-response"))
        : expectedCode === "reject"
        ? result.ok === false &&
          (code === "network-failure" || code === "unacceptable-response")
        : expectedCode
        ? result.ok === false && code === expectedCode
        : successMatchesPackage;
      const diagnosticsZero = diagnosticsSettled;
      const fixtureIdle = await waitForFixtureIdle(fixture!, 5_000);
      const casePassed = result.scriptFailure !== true && pass &&
        fixtureEvidenceMatched && diagnosticsZero && fixtureIdle;
      updateCase(cases, id, casePassed, {
        ok: result.ok === true,
        code,
        modeObserved,
        fixtureEvidenceMatched,
        bytesMatch: result.exactBytes === true &&
          result.byteLength === expectedBytes.byteLength,
        hashMatch: result.hash === hash,
        diagnosticsZero,
        fixtureIdle,
        scriptFailure: result.scriptFailure === true,
        casePassed,
      });
      return { ...result, code, casePassed };
    };
    const trustedBefore = fixture.snapshot();
    fixture.setMode(validMode);
    const firstBody = `
      const m = ChromeUtils.importESModule(${JSON.stringify(MODULE_URI)});
      const host = ChromeUtils.importESModule(${
      JSON.stringify(FEATURE_HOST_URI)
    });
      const p = input.package;
      const pref = "${SOURCE_PROBE_PREF}";
      const hadPref = Services.prefs.prefHasUserValue(pref);
      const previousPref = Services.prefs.getBoolPref(pref, false);
      const decode = value => Uint8Array.from(
        atob(value),
        character => character.charCodeAt(0)
      );
      const expected = decode(p.bytes);
      let positiveControl = false;
      let sandboxNuked = false;
      let sentinelFalseBeforeControl = false;
      let sentinelFalseAfterControl = false;
      let controlEvaluationCountUnchanged = false;
      let sentinelFalseBeforeDownloads = false;
      let sentinelFalseAfterFirstDownload = false;
      let sentinelFalseAfterDownloads = false;
      let evaluationCountUnchanged = false;
      let exactBytes = false;
      let lengthsMatch = false;
      let hashesMatch = false;
      let detached = false;
      try {
        Services.prefs.clearUserPref(pref);
        sentinelFalseBeforeControl =
          Services.prefs.getBoolPref(pref, false) === false;
        const evaluationsBeforeControl = host.m0PayloadEvaluationsForTests();
        let sandbox = null;
        try {
          sandbox = Components.utils.Sandbox(
            Services.scriptSecurityManager.getSystemPrincipal(),
            {
              sandboxName: "m3d-source-probe-control",
              freshCompartment: true,
            },
          );
          sandbox.Services = Services;
          Components.utils.evalInSandbox(input.source, sandbox);
          positiveControl = Services.prefs.getBoolPref(pref, false) === true;
        } catch {
          positiveControl = false;
        } finally {
          if (sandbox !== null) {
            try {
              Components.utils.nukeSandbox(sandbox);
              sandboxNuked = true;
            } catch {
              sandboxNuked = false;
            }
          }
          Services.prefs.clearUserPref(pref);
        }
        sentinelFalseAfterControl =
          Services.prefs.getBoolPref(pref, false) === false;
        controlEvaluationCountUnchanged =
          Number.isSafeInteger(evaluationsBeforeControl) &&
          host.m0PayloadEvaluationsForTests() === evaluationsBeforeControl;
        if (sandboxNuked && sentinelFalseAfterControl) {
          sentinelFalseBeforeDownloads =
            Services.prefs.getBoolPref(pref, false) === false;
          const evaluationsBeforeDownloads = host.m0PayloadEvaluationsForTests();
          const a = await m.downloadFeaturePackageBytes({
            origin: input.origin,
            packageSha256: p.hash,
            policy: input.policy
          });
          sentinelFalseAfterFirstDownload =
            Services.prefs.getBoolPref(pref, false) === false;
          const evaluationsAfterFirstDownload = host.m0PayloadEvaluationsForTests();
          const b = await m.downloadFeaturePackageBytes({
            origin: input.origin,
            packageSha256: p.hash,
            policy: input.policy
          });
          sentinelFalseAfterDownloads =
            Services.prefs.getBoolPref(pref, false) === false;
          const evaluationsAfterDownloads = host.m0PayloadEvaluationsForTests();
          lengthsMatch = a.byteLength === expected.byteLength &&
            b.byteLength === expected.byteLength;
          exactBytes = lengthsMatch &&
            a.every((value, index) => value === expected[index]) &&
            b.every((value, index) => value === expected[index]);
          const digest = async bytes => Array.from(
            new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)),
            value => value.toString(16).padStart(2, "0")
          ).join("");
          hashesMatch = await digest(a) === p.hash && await digest(b) === p.hash;
          detached = a !== b && a.buffer !== b.buffer;
          evaluationCountUnchanged =
            Number.isSafeInteger(evaluationsBeforeDownloads) &&
            Number.isSafeInteger(evaluationsAfterFirstDownload) &&
            Number.isSafeInteger(evaluationsAfterDownloads) &&
            evaluationsBeforeDownloads === evaluationsAfterFirstDownload &&
            evaluationsBeforeDownloads === evaluationsAfterDownloads;
        }
      } finally {
        if (hadPref) Services.prefs.setBoolPref(pref, previousPref);
        else Services.prefs.clearUserPref(pref);
      }
      return {
        positiveControl,
        sandboxNuked,
        sentinelFalseBeforeControl,
        sentinelFalseAfterControl,
        controlEvaluationCountUnchanged,
        sentinelFalseBeforeDownloads,
        sentinelFalseAfterFirstDownload,
        sentinelFalseAfterDownloads,
        evaluationCountUnchanged,
        exactBytes,
        lengthsMatch,
        hashesMatch,
        detached
      };
    `;
    const trusted = await browserOperation(launch, firstBody, {
      origin: fixture.trustedOrigin,
      source: trustedSource,
      package: {
        hash: p1.packageSha256,
        bytes: toBase64(packages.get(p1.packageSha256)!),
      },
      policy,
    });
    const trustedDiagnostics = await waitForNativeDiagnosticsZero(
      launch,
      5_000,
    );
    const trustedFixtureIdle = await waitForFixtureIdle(fixture, 5_000);
    const trustedAfter = fixture.snapshot();
    const trustedRequests = trustedAfter.observations.slice(
      trustedBefore.observations.length,
    );
    const exactlyTwoTrustedValidRequests =
      trustedAfter.requests - trustedBefore.requests === 2 &&
      trustedRequests.length === 2 && !trustedBefore.observationOverflow &&
      !trustedAfter.observationOverflow && trustedRequests.every((entry) =>
        entry.origin === "trusted" && entry.mode === "valid-streaming"
      );
    const trustedAcquisitionPassed = trusted.scriptFailure !== true &&
      trusted.exactBytes === true && trusted.lengthsMatch === true &&
      trusted.hashesMatch === true && trusted.detached === true &&
      trustedDiagnostics.diagnosticsZero === true && trustedFixtureIdle &&
      exactlyTwoTrustedValidRequests;
    updateCase(
      cases,
      "trusted-bytes-reuse",
      trustedAcquisitionPassed,
      {
        exactBytes: trusted.exactBytes === true,
        lengthsMatch: trusted.lengthsMatch === true,
        hashesMatch: trusted.hashesMatch === true,
        detached: trusted.detached === true,
        scriptFailure: trusted.scriptFailure === true,
        diagnosticsZero: trustedDiagnostics.diagnosticsZero === true,
        fixtureIdle: trustedFixtureIdle,
        exactlyTwoTrustedValidRequests,
        casePassed: trustedAcquisitionPassed,
      },
    );
    const sourceInertPassed = trustedAcquisitionPassed &&
      trusted.positiveControl === true && trusted.sandboxNuked === true &&
      trusted.sentinelFalseBeforeControl === true &&
      trusted.sentinelFalseAfterControl === true &&
      trusted.controlEvaluationCountUnchanged === true &&
      trusted.sentinelFalseBeforeDownloads === true &&
      trusted.sentinelFalseAfterFirstDownload === true &&
      trusted.sentinelFalseAfterDownloads === true &&
      trusted.evaluationCountUnchanged === true;
    updateCase(
      cases,
      "source-inert",
      sourceInertPassed,
      {
        trustedAcquisitionPassed,
        scriptFailure: trusted.scriptFailure === true,
        sourceInert: trusted.sentinelFalseBeforeDownloads === true &&
          trusted.sentinelFalseAfterFirstDownload === true &&
          trusted.sentinelFalseAfterDownloads === true,
        sentinelPositiveControl: trusted.positiveControl === true,
        sandboxNuked: trusted.sandboxNuked === true,
        sentinelFalseBeforeControl: trusted.sentinelFalseBeforeControl === true,
        sentinelFalseAfterControl: trusted.sentinelFalseAfterControl === true,
        controlEvaluationCountUnchanged:
          trusted.controlEvaluationCountUnchanged === true,
        evaluationCountUnchanged: trusted.evaluationCountUnchanged === true,
        diagnosticsZero: trustedDiagnostics.diagnosticsZero === true,
        fixtureIdle: trustedFixtureIdle,
        exactlyTwoTrustedValidRequests,
        casePassed: sourceInertPassed,
      },
    );
    fixture.setMode("valid-streaming");
    const beforeUntrusted = fixture.snapshot();
    const untrusted = await browserOperation(
      launch,
      `
        const module = ChromeUtils.importESModule(${
        JSON.stringify(MODULE_URI)
      });
        const expectedOrigin = new URL(input.origin).origin;
        const knownCodes = new Set([
          "invalid-request",
          "invalid-endpoint",
          "invalid-policy",
          "busy",
          "aborted",
          "timeout",
          "network-failure",
          "unacceptable-response",
          "size-limit",
          "package-rejected",
          "expected-package-mismatch",
          "unsupported-platform",
          "cleanup-unproven"
        ]);
        const normalizeCode = error => {
          try {
            const candidate = error?.code;
            return typeof candidate === "string" && knownCodes.has(candidate)
              ? candidate
              : null;
          } catch {
            return null;
          }
        };
        let matchingOriginObserved = false;
        let matchingModifyObserved = false;
        let certificateStopObserved = false;
        let selfSignedCertificate = false;
        let modifyObserverInstalled = false;
        let stopObserverInstalled = false;
        let modifyObserverRemoved = false;
        let stopObserverRemoved = false;
        let downloadResolved = false;
        let code = null;
        const observer = {
          QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
          observe(subject, topic) {
            try {
              const channel = subject.QueryInterface(Ci.nsIHttpChannel);
              if (channel.URI.prePath !== expectedOrigin) return;
              matchingOriginObserved = true;
              if (topic === "http-on-modify-request") {
                matchingModifyObserved = true;
              }
              if (topic === "http-on-stop-request") {
                certificateStopObserved = true;
                try {
                  const securityInfo = channel.securityInfo.QueryInterface(
                    Ci.nsITransportSecurityInfo
                  );
                  selfSignedCertificate = selfSignedCertificate ||
                    securityInfo.errorCodeString ===
                      "MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT";
                } catch {
                  return;
                }
              }
            } catch {
              return;
            }
          }
        };
        try {
          Services.obs.addObserver(observer, "http-on-modify-request");
          modifyObserverInstalled = true;
          Services.obs.addObserver(observer, "http-on-stop-request");
          stopObserverInstalled = true;
          try {
            await module.downloadFeaturePackageBytes({
              origin: input.origin,
              packageSha256: input.hash,
              policy: input.policy
            });
            downloadResolved = true;
          } catch (error) {
            code = normalizeCode(error);
          }
          const deadline = ChromeUtils.now() + 10_000;
          const window = Services.wm.getMostRecentWindow("navigator:browser");
          while (!selfSignedCertificate && ChromeUtils.now() < deadline) {
            await new Promise(resolve => window.setTimeout(resolve, 10));
          }
        } finally {
          if (modifyObserverInstalled) {
            try {
              Services.obs.removeObserver(observer, "http-on-modify-request");
              modifyObserverRemoved = true;
            } catch {
              modifyObserverRemoved = false;
            }
          }
          if (stopObserverInstalled) {
            try {
              Services.obs.removeObserver(observer, "http-on-stop-request");
              stopObserverRemoved = true;
            } catch {
              stopObserverRemoved = false;
            }
          }
        }
        return {
          scriptFailure: false,
          code,
          downloadResolved,
          matchingOriginObserved,
          matchingModifyObserved,
          certificateStopObserved,
          selfSignedCertificate,
          observersRemoved: modifyObserverRemoved && stopObserverRemoved
        };
      `,
      { origin: fixture.untrustedOrigin, hash: p1.packageSha256, policy },
      35_000,
    );
    const untrustedDiagnostics = await waitForNativeDiagnosticsZero(
      launch,
      5_000,
    );
    const untrustedFixtureIdle = await waitForFixtureIdle(fixture, 5_000);
    const afterUntrusted = fixture.snapshot();
    const trustedControlPassed = cases.find((entry) =>
      entry.id === "trusted-bytes-reuse"
    )?.status === "passed";
    const untrustedNoHttpRequest = afterUntrusted.requests ===
      beforeUntrusted.requests;
    const knownUntrustedCodes = new Set([
      "invalid-request",
      "invalid-endpoint",
      "invalid-policy",
      "busy",
      "aborted",
      "timeout",
      "network-failure",
      "unacceptable-response",
      "size-limit",
      "package-rejected",
      "expected-package-mismatch",
      "unsupported-platform",
      "cleanup-unproven",
    ]);
    const returnedCode: unknown = untrusted.code;
    const untrustedCode = typeof returnedCode === "string" &&
        knownUntrustedCodes.has(returnedCode)
      ? returnedCode
      : null;
    const certificateEvidence = untrusted.matchingOriginObserved === true &&
      untrusted.certificateStopObserved === true &&
      untrusted.selfSignedCertificate === true;
    const diagnosticsZero = untrustedDiagnostics.diagnosticsZero === true;
    const untrustedCasePassed = trustedControlPassed &&
      untrusted.scriptFailure === false &&
      untrusted.downloadResolved === false &&
      untrustedCode === "network-failure" && certificateEvidence &&
      untrustedNoHttpRequest && diagnosticsZero && untrustedFixtureIdle &&
      untrusted.observersRemoved === true;
    updateCase(
      cases,
      "untrusted-tls",
      untrustedCasePassed,
      {
        casePassed: untrustedCasePassed,
        scriptFailure: untrusted.scriptFailure === true,
        networkRejected: untrustedCode === "network-failure" &&
          untrusted.downloadResolved === false,
        errorCode: untrustedCode,
        matchingOriginObserved: untrusted.matchingOriginObserved === true,
        matchingModifyObserved: untrusted.matchingModifyObserved === true,
        certificateStopObserved: untrusted.certificateStopObserved === true,
        certificateEvidence,
        observersRemoved: untrusted.observersRemoved === true,
        trustedControlPassed,
        noHttpRequest: untrustedNoHttpRequest,
        diagnosticsZero,
        fixtureIdle: untrustedFixtureIdle,
      },
    );
    for (
      const [id, mode, code] of [
        ["redirect", "redirect", "network-failure"],
        ["http-status", "status-rejected", "unacceptable-response"],
        ["content-type", "content-type-rejected", "unacceptable-response"],
        [
          "content-encoding",
          "content-encoding-rejected",
          "unacceptable-response",
        ],
        ["declared-oversize", "oversized-declared", "unacceptable-response"],
        ["streamed-oversize", "oversized-streamed", "size-limit"],
        ["truncated-length", "truncated-length", "network-failure"],
        ["read-failure", "read-failure", "network-failure"],
      ] as const
    ) {
      await download(id, mode, fixture.trustedOrigin, p1.packageSha256, code);
    }
    await download(
      "wrong-package",
      "wrong-package",
      fixture.trustedOrigin,
      p1.packageSha256,
      "expected-package-mismatch",
    );
    await download(
      "raw-equal-length",
      "equal-content-length",
      fixture.rawOrigin,
      p1.packageSha256,
      "either",
    );
    await download(
      "raw-conflicting-length",
      "conflicting-content-length",
      fixture.rawOrigin,
      p1.packageSha256,
      "reject",
    );
    const cancellationRunId = `${runId}:caller-abort`;
    const cancellationBaseline = fixture.snapshot();
    fixture.setMode("stalled-body");
    let cancellationStart: Record<string, unknown> = {};
    try {
      cancellationStart = await startPendingDownload(
        launch,
        cancellationRunId,
        {
          origin: fixture.trustedOrigin,
          packageSha256: p1.packageSha256,
          policy,
        },
        true,
      );
    } catch {
      cancellationStart = { started: false, startRpcFailed: true };
    }
    const cancelStallSnapshot = await waitForActiveStall(
      fixture,
      cancellationBaseline,
      10_000,
    );
    let cancelReading: Record<string, unknown> = {};
    if (cancelStallSnapshot !== null) {
      try {
        cancelReading = await waitForPendingReading(
          launch,
          cancellationRunId,
          8_000,
        );
      } catch {
        cancelReading = { stateFound: false, readingObserved: false };
      }
    }
    const cancellation = await finishPendingDownload(
      launch,
      fixture,
      cancellationRunId,
      cancellationBaseline.cancellations,
      true,
      15_000,
    );
    const cancelReuse = await download(
      "cancel-reuse",
      validMode,
      fixture.trustedOrigin,
      p1.packageSha256,
    );
    const cancelCasePassed = cancellationStart.started === true &&
      cancelStallSnapshot !== null &&
      cancelStallSnapshot.activeConnections > 0 &&
      cancelReading.stateFound === true &&
      cancelReading.readingObserved === true &&
      cancelReading.outcomeSettled === false &&
      cancellation.abortIssued === true &&
      cancellation.outcomeConsumed === true &&
      cancellation.code === "aborted" &&
      cancellation.fixedMessage === true &&
      cancellation.reasonLeaked === false &&
      cancellation.diagnosticsZero === true &&
      cancellation.fixturePeerCancelled === true &&
      cancellation.activeStreams === 0 &&
      cancellation.activeConnections === 0 &&
      cancellation.stateDeleted === true &&
      cancelReuse.casePassed === true;
    updateCase(
      cases,
      "caller-abort",
      cancelCasePassed,
      {
        casePassed: cancelCasePassed,
        startObserved: cancellationStart.started === true,
        readingObserved: cancelReading.readingObserved === true,
        outcomePendingAtRead: cancelReading.outcomeSettled === false,
        readingStateFound: cancelReading.stateFound === true,
        stallObserved: cancelStallSnapshot !== null,
        stallRequests: cancelStallSnapshot === null
          ? -1
          : cancelStallSnapshot.requests - cancellationBaseline.requests,
        stallActiveStreams: cancelStallSnapshot?.activeStreams ?? -1,
        stallActiveConnections: cancelStallSnapshot?.activeConnections ?? -1,
        abortIssued: cancellation.abortIssued === true,
        outcomeConsumed: cancellation.outcomeConsumed === true,
        errorCode: typeof cancellation.code === "string"
          ? cancellation.code
          : null,
        fixedMessage: cancellation.fixedMessage === true,
        sentinelNotLeaked: cancellation.reasonLeaked === false,
        peerCancellationObserved: cancellation.fixturePeerCancelled === true,
        diagnosticsZero: cancellation.diagnosticsZero === true,
        activeStreams: typeof cancellation.activeStreams === "number"
          ? cancellation.activeStreams
          : -1,
        activeConnections: typeof cancellation.activeConnections === "number"
          ? cancellation.activeConnections
          : -1,
        stateDeleted: cancellation.stateDeleted === true,
        validReuse: cancelReuse.casePassed === true,
      },
    );
    const deadlineRunId = `${runId}:twenty-second-deadline`;
    const deadlineBaseline = fixture.snapshot();
    fixture.setMode("stalled-body");
    let deadlineStart: Record<string, unknown> = {};
    try {
      deadlineStart = await startPendingDownload(
        launch,
        deadlineRunId,
        {
          origin: fixture.trustedOrigin,
          packageSha256: p1.packageSha256,
          policy,
        },
        false,
      );
    } catch {
      deadlineStart = { started: false, startRpcFailed: true };
    }
    const deadlineStallSnapshot = await waitForActiveStall(
      fixture,
      deadlineBaseline,
      10_000,
    );
    let deadlineReading: Record<string, unknown> = {};
    if (deadlineStallSnapshot !== null) {
      try {
        deadlineReading = await waitForPendingReading(
          launch,
          deadlineRunId,
          8_000,
        );
      } catch {
        deadlineReading = { stateFound: false, readingObserved: false };
      }
    }
    const deadlinePreconditions = deadlineStart.started === true &&
      deadlineStallSnapshot !== null &&
      deadlineStallSnapshot.activeConnections > 0 &&
      deadlineReading.stateFound === true &&
      deadlineReading.readingObserved === true &&
      deadlineReading.outcomeSettled === false;
    const deadline = await finishPendingDownload(
      launch,
      fixture,
      deadlineRunId,
      deadlineBaseline.cancellations,
      !deadlinePreconditions,
      35_000,
    );
    const deadlineReuse = await download(
      "final-reuse",
      validMode,
      fixture.trustedOrigin,
      p1.packageSha256,
    );
    const deadlinePassed = deadlinePreconditions &&
      deadline.outcomeConsumed === true && deadline.code === "timeout" &&
      typeof deadline.elapsedMs === "number" &&
      deadline.elapsedMs >= 20_000 && deadline.elapsedMs < 30_000 &&
      deadline.diagnosticsZero === true &&
      deadline.fixturePeerCancelled === true &&
      deadline.activeStreams === 0 && deadline.activeConnections === 0 &&
      deadline.stateDeleted === true && deadlineReuse.casePassed === true;
    updateCase(cases, "twenty-second-deadline", deadlinePassed, {
      startObserved: deadlineStart.started === true,
      preconditionsMet: deadlinePreconditions,
      stallObserved: deadlineStallSnapshot !== null,
      stallRequests: deadlineStallSnapshot === null
        ? -1
        : deadlineStallSnapshot.requests - deadlineBaseline.requests,
      stallActiveStreams: deadlineStallSnapshot?.activeStreams ?? -1,
      stallActiveConnections: deadlineStallSnapshot?.activeConnections ?? -1,
      readingObserved: deadlineReading.readingObserved === true,
      outcomePendingAtRead: deadlineReading.outcomeSettled === false,
      readingStateFound: deadlineReading.stateFound === true,
      outcomeConsumed: deadline.outcomeConsumed === true,
      errorCode: typeof deadline.code === "string" ? deadline.code : null,
      elapsedMs: typeof deadline.elapsedMs === "number"
        ? deadline.elapsedMs
        : -1,
      diagnosticsZero: deadline.diagnosticsZero === true,
      peerCancellationObserved: deadline.fixturePeerCancelled === true,
      activeStreams: typeof deadline.activeStreams === "number"
        ? deadline.activeStreams
        : -1,
      activeConnections: typeof deadline.activeConnections === "number"
        ? deadline.activeConnections
        : -1,
      stateDeleted: deadline.stateDeleted === true,
      validReuse: deadlineReuse.casePassed === true,
      casePassed: deadlinePassed,
    });
    fixture.setMode(validMode);
    const beforeOrdinaryAbort = fixture.snapshot();
    const ordinaryAbort = await browserOperation(
      launch,
      `
        const module = ChromeUtils.importESModule(${
        JSON.stringify(MODULE_URI)
      });
        const knownCodes = new Set([
          "invalid-request",
          "invalid-endpoint",
          "invalid-policy",
          "busy",
          "aborted",
          "timeout",
          "network-failure",
          "unacceptable-response",
          "size-limit",
          "package-rejected",
          "expected-package-mismatch",
          "unsupported-platform",
          "cleanup-unproven"
        ]);
        const normalizeCode = error => {
          try {
            const candidate = error?.code;
            return typeof candidate === "string" && knownCodes.has(candidate)
              ? candidate
              : null;
          } catch {
            return null;
          }
        };
        const hasFixedMessage = error => {
          try {
            return error?.message === "Package download was aborted.";
          } catch {
            return false;
          }
        };
        const window = Services.wm.getMostRecentWindow("navigator:browser");
        const controller = new window.AbortController();
        controller.abort({secret: "sentinel"});
        let code = null;
        let downloadResolved = false;
        let fixedMessage = false;
        try {
          await module.downloadFeaturePackageBytes({
            origin: input.origin,
            packageSha256: input.hash,
            policy: input.policy,
            signal: controller.signal
          });
          downloadResolved = true;
        } catch (error) {
          code = normalizeCode(error);
          fixedMessage = hasFixedMessage(error);
        }
        return {scriptFailure: false, code, downloadResolved, fixedMessage};
      `,
      { origin: fixture.trustedOrigin, hash: p1.packageSha256, policy },
    );
    const abortDiagnostics = await waitForNativeDiagnosticsZero(
      launch,
      5_000,
    );
    const abortFixtureIdle = await waitForFixtureIdle(fixture, 5_000);
    const afterOrdinaryAbort = fixture.snapshot();
    const abortNoHttpRequest = afterOrdinaryAbort.requests ===
      beforeOrdinaryAbort.requests;
    const knownAbortCodes = new Set([
      "invalid-request",
      "invalid-endpoint",
      "invalid-policy",
      "busy",
      "aborted",
      "timeout",
      "network-failure",
      "unacceptable-response",
      "size-limit",
      "package-rejected",
      "expected-package-mismatch",
      "unsupported-platform",
      "cleanup-unproven",
    ]);
    const returnedAbortCode: unknown = ordinaryAbort.code;
    const ordinaryAbortCode = typeof returnedAbortCode === "string" &&
        knownAbortCodes.has(returnedAbortCode)
      ? returnedAbortCode
      : null;
    const ordinaryAbortCleanupPassed = ordinaryAbort.scriptFailure === false &&
      ordinaryAbort.downloadResolved === false &&
      ordinaryAbortCode === "aborted" &&
      ordinaryAbort.fixedMessage === true &&
      abortDiagnostics.diagnosticsZero === true && abortFixtureIdle &&
      abortNoHttpRequest;
    const abortReuse = await download(
      "abort-reuse",
      validMode,
      fixture.trustedOrigin,
      p1.packageSha256,
    );
    const ordinaryAbortPassed = ordinaryAbortCleanupPassed &&
      abortReuse.casePassed === true;
    updateCase(
      cases,
      "abort-reuse",
      ordinaryAbortPassed,
      {
        errorRejected: ordinaryAbortCode === "aborted",
        errorCode: ordinaryAbortCode,
        fixedMessage: ordinaryAbort.fixedMessage === true,
        scriptFailure: ordinaryAbort.scriptFailure === true,
        diagnosticsZero: abortDiagnostics.diagnosticsZero === true,
        fixtureIdle: abortFixtureIdle,
        noHttpRequest: abortNoHttpRequest,
        validReuse: abortReuse.casePassed === true,
        casePassed: ordinaryAbortPassed,
      },
    );
    updateCase(
      cases,
      "ordinary-abort",
      ordinaryAbortPassed,
      {
        errorRejected: ordinaryAbortCode === "aborted",
        errorCode: ordinaryAbortCode,
        fixedMessage: ordinaryAbort.fixedMessage === true,
        scriptFailure: ordinaryAbort.scriptFailure === true,
        diagnosticsZero: abortDiagnostics.diagnosticsZero === true,
        fixtureIdle: abortFixtureIdle,
        noHttpRequest: abortNoHttpRequest,
        validReuse: abortReuse.casePassed === true,
        casePassed: ordinaryAbortPassed,
      },
    );
    const networkFailure = await download(
      "ordinary-network-failure",
      validMode,
      fixture.untrustedOrigin,
      p1.packageSha256,
      "network-failure",
    );
    const networkReuse = await download(
      "network-reuse",
      validMode,
      fixture.trustedOrigin,
      p1.packageSha256,
    );
    updateCase(
      cases,
      "network-reuse",
      networkFailure.casePassed === true && networkReuse.casePassed === true,
      {
        errorRejected: networkFailure.casePassed === true,
        validReuse: networkReuse.casePassed === true,
      },
    );
    const readFailure = await download(
      "ordinary-read-failure",
      "read-failure",
      fixture.trustedOrigin,
      p1.packageSha256,
      "network-failure",
    );
    const readReuse = await download(
      "read-reuse",
      validMode,
      fixture.trustedOrigin,
      p1.packageSha256,
    );
    updateCase(
      cases,
      "read-reuse",
      readFailure.casePassed === true && readReuse.casePassed === true,
      {
        errorRejected: readFailure.casePassed === true,
        validReuse: readReuse.casePassed === true,
      },
    );
    await download(
      "final-reuse",
      validMode,
      fixture.trustedOrigin,
      p2.packageSha256,
    );
    await runM3dCompositionCases({
      launch,
      fixture,
      policy,
      featureId: FEATURE_ID,
      validMode,
      stalledMode: "stalled-body",
      sourceProbePref: SOURCE_PROBE_PREF,
      packages: [
        {
          version: p1.version,
          releaseSequence: p1.releaseSequence,
          packageSha256: p1.packageSha256,
          bytes: packages.get(p1.packageSha256) ?? new Uint8Array(),
        },
        {
          version: p2.version,
          releaseSequence: p2.releaseSequence,
          packageSha256: p2.packageSha256,
          bytes: packages.get(p2.packageSha256) ?? new Uint8Array(),
        },
      ],
      browserOperation: (body, args, timeoutMs) =>
        browserOperation(launch!, body, args, timeoutMs),
      waitForActiveStall: (baseline, timeoutMs) =>
        waitForActiveStall(fixture!, baseline, timeoutMs),
      waitForFixtureIdle: (timeoutMs) =>
        waitForFixtureIdle(fixture!, timeoutMs),
      waitForNativeDiagnosticsZero: async (timeoutMs) => {
        try {
          return (await waitForNativeDiagnosticsZero(launch!, timeoutMs))
            .diagnosticsZero === true;
        } catch {
          return false;
        }
      },
      record: (id, passed, observations) =>
        updateCase(cases, id, passed, observations),
    });
    const exactlyOneTrustedObservation = (
      before: M3dPackageHttpsSnapshot,
      after: M3dPackageHttpsSnapshot,
      mode: M3dPackageHttpsMode,
    ): boolean => {
      const added = after.observations.slice(before.observations.length);
      return !before.observationOverflow && !after.observationOverflow &&
        after.requests - before.requests === 1 && added.length === 1 &&
        added[0].origin === "trusted" && added[0].mode === mode;
    };
    fixture.setMode("cookie-seed");
    const beforeSeed = fixture.snapshot();
    const seed = await browserOperation(
      launch,
      `
        const window = Services.wm.getMostRecentWindow("navigator:browser");
        const response = await window.fetch(
          input.origin + "/__m3d/cookie/seed",
          { credentials: "include", cache: "no-store" }
        );
        return {ok: response.status === 204};
      `,
      { origin: fixture.trustedOrigin },
    );
    const afterSeed = fixture.snapshot();
    const seedObservationExact = exactlyOneTrustedObservation(
      beforeSeed,
      afterSeed,
      "cookie-seed",
    );

    fixture.setMode("cookie-control");
    const beforeControl = fixture.snapshot();
    const controlBefore = await browserOperation(
      launch,
      `
        const window = Services.wm.getMostRecentWindow("navigator:browser");
        const response = await window.fetch(
          input.origin + "/__m3d/cookie/control",
          { credentials: "include", cache: "no-store" }
        );
        return {ok: response.status === 204};
      `,
      { origin: fixture.trustedOrigin },
    );
    const afterControlBefore = fixture.snapshot();
    const controlBeforeObservations = afterControlBefore.observations.slice(
      beforeControl.observations.length,
    );
    const controlBeforeExact = exactlyOneTrustedObservation(
      beforeControl,
      afterControlBefore,
      "cookie-control",
    );
    const seedCookieInControlBefore = controlBeforeObservations.length === 1 &&
      controlBeforeObservations[0].seedCookieMarkerMatch;
    const acquisitionCookieAbsentAtBaseline =
      controlBeforeObservations.length === 1 &&
      !controlBeforeObservations[0].acquisitionCookieMarkerMatch;

    fixture.setMode("valid-streaming");
    const beforeAcquisition = fixture.snapshot();
    const acquisition = await download(
      "cookie-privacy",
      validMode,
      fixture.trustedOrigin,
      p1.packageSha256,
    );
    const afterAcquisition = fixture.snapshot();
    const acquisitionObservations = afterAcquisition.observations.slice(
      beforeAcquisition.observations.length,
    );
    const acquisitionObservationExact = exactlyOneTrustedObservation(
      beforeAcquisition,
      afterAcquisition,
      "valid-streaming",
    );
    const acquisitionHeadersAbsent = acquisitionObservations.length === 1 &&
      !acquisitionObservations[0].cookiePresent &&
      !acquisitionObservations[0].authorizationPresent &&
      !acquisitionObservations[0].referrerPresent;

    fixture.setMode("cookie-control");
    const beforeControlAfter = fixture.snapshot();
    const controlAfter = await browserOperation(
      launch,
      `
        const window = Services.wm.getMostRecentWindow("navigator:browser");
        const response = await window.fetch(
          input.origin + "/__m3d/cookie/control",
          { credentials: "include", cache: "no-store" }
        );
        return {ok: response.status === 204};
      `,
      { origin: fixture.trustedOrigin },
    );
    const afterControlAfter = fixture.snapshot();
    const controlAfterObservations = afterControlAfter.observations.slice(
      beforeControlAfter.observations.length,
    );
    const controlAfterExact = exactlyOneTrustedObservation(
      beforeControlAfter,
      afterControlAfter,
      "cookie-control",
    );
    const seedCookieInControlAfter = controlAfterObservations.length === 1 &&
      controlAfterObservations[0].seedCookieMarkerMatch;
    const acquisitionCookieNotPersisted =
      controlAfterObservations.length === 1 &&
      !controlAfterObservations[0].acquisitionCookieMarkerMatch;
    const cookieDiagnostics = await waitForNativeDiagnosticsZero(
      launch,
      5_000,
    );
    const cookieFixtureIdle = await waitForFixtureIdle(fixture, 5_000);
    const cookieFinalSnapshot = fixture.snapshot();
    const cookieRequestsStable =
      cookieFinalSnapshot.requests === afterControlAfter.requests &&
      cookieFinalSnapshot.observations.length ===
        afterControlAfter.observations.length &&
      !cookieFinalSnapshot.observationOverflow;
    const seedCookieStoredAndSent = seedCookieInControlBefore &&
      seedCookieInControlAfter;
    const acquisitionCasePassed = acquisition.casePassed === true &&
      seed.ok === true && seedObservationExact && controlBefore.ok === true &&
      controlBeforeExact && seedCookieInControlBefore &&
      acquisitionCookieAbsentAtBaseline && acquisitionObservationExact &&
      acquisitionHeadersAbsent && controlAfter.ok === true &&
      controlAfterExact && seedCookieInControlAfter &&
      acquisitionCookieNotPersisted &&
      cookieDiagnostics.diagnosticsZero === true && cookieFixtureIdle &&
      cookieRequestsStable;
    updateCase(
      cases,
      "cookie-privacy",
      acquisitionCasePassed,
      {
        seedObservationExact,
        controlBeforeExact,
        controlAfterExact,
        acquisitionObservationExact,
        seedCookieStoredAndSent,
        acquisitionCookieAbsentAtBaseline,
        acquisitionHeadersAbsent,
        acquisitionCookieNotPersisted,
        diagnosticsZero: cookieDiagnostics.diagnosticsZero === true,
        fixtureIdle: cookieFixtureIdle,
        requestsStable: cookieRequestsStable,
        casePassed: acquisitionCasePassed,
      },
    );
    const finalSnapshot = fixture.snapshot();
    cleanup.fixtureIdleBeforeClose = finalSnapshot.activeStreams === 0 &&
      finalSnapshot.activeConnections === 0;
    cleanup.fixtureObservationsBounded = !finalSnapshot.observationOverflow;
    const redirectCase = cases.find((entry) =>
      entry.id === "redirect"
    );
    updateCase(
      cases,
      "redirect",
      redirectCase?.status === "passed" && finalSnapshot.redirectTargets === 0,
      {
        ...redirectCase?.observations,
        redirectTargets: finalSnapshot.redirectTargets,
        targetNotReached: finalSnapshot.redirectTargets === 0,
      },
    );
    const profilePort = join(PROJECT_ROOT, "_dist", "marionette-port.txt");
    try {
      await Deno.lstat(profilePort);
      throw new Error("Marionette status port file already exists");
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error;
    }
    await launch.client.close();
    await Deno.writeTextFile(profilePort, `${marionettePort}\n`, {
      createNew: true,
      mode: 0o600,
    });
    try {
      let connectedLiteral = false;
      let statusStdout = "";
      let statusCommandSucceeded = false;
      try {
        const output = await boundedCommand(
          Deno.execPath(),
          ["task", "dev-tool", "status"],
          PROJECT_ROOT,
        );
        statusStdout = output.stdout;
        statusCommandSucceeded = output.success;
        connectedLiteral = output.success &&
          output.stdout.includes("Connected to browser.");
      } catch {
        connectedLiteral = false;
      }
      const finalReusePassed = cases.find((entry) => entry.id === "final-reuse")
        ?.status === "passed";
      updateCase(
        cases,
        "connected-status",
        connectedLiteral && finalReusePassed,
        {
          connectedLiteral,
          statusCommandSucceeded,
          statusStdout,
          finalReusePassed,
        },
      );
    } finally {
      await Deno.remove(profilePort);
    }
  } catch {
    runFailure = "acceptance-run-failed";
    if (launchAttempted && !launchSucceeded) launchFailure = "launch-failed";
    console.error("M3d transport acceptance failed");
  } finally {
    if (launch) {
      try {
        const stopped = await killBrowser(launch);
        cleanup.browserStopped = stopped.portClosed === true;
      } catch {
        cleanup.browserStopped = false;
      }
    }
    if (fixture) {
      try {
        const evidence = await fixture.close();
        cleanup.fixtureClosed = evidence.closed && evidence.listenersClosed &&
          evidence.activeConnectionsClosed && evidence.stallsReleased &&
          evidence.acceptLoopsSettled && evidence.cleanupErrorCount === 0;
      } catch {
        cleanup.fixtureClosed = false;
      }
    }
    const allPorts = [...browserPorts, ...fixturePorts];
    const allPortsClosed = allPorts.length > 0 &&
      (await Promise.all(allPorts.map(portIsClosed))).every(Boolean);
    cleanup.portsClosed = allPortsClosed &&
      (!launch || cleanup.browserStopped) &&
      (fixture === null || cleanup.fixtureClosed);
    browserCleanupProven = launchAttempted && launchSucceeded &&
      cleanup.browserStopped === true && browserPorts.length === 2 &&
      (await Promise.all(browserPorts.map(portIsClosed))).every(Boolean);
    cleanup.ownedCommandsClosed = ownedCommandsCleanupProven;
    try {
      sharedAfter = await snapshotProfileTree(SHARED_PROFILE);
    } catch {
      sharedAfter = null;
    }
    if (!profileOwned) {
      cleanup.profileRemoved = true;
    } else if (!launchAttempted || browserCleanupProven) {
      try {
        await Deno.remove(profile, { recursive: true });
        cleanup.profileRemoved = true;
      } catch (error) {
        cleanup.profileRemoved = error instanceof Deno.errors.NotFound;
      }
    }
    const passed = await writeReport(true);
    console.log(`M3d transport report: ${reportPath}`);
    Deno.exitCode = reportWritten && passed ? 0 : 1;
  }
}

if (import.meta.main) await main();
