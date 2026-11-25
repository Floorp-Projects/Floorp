// SPDX-License-Identifier: MPL-2.0

import { basename, dirname, isAbsolute, join, resolve } from "@std/path";
import { PROJECT_ROOT } from "./defines.ts";
import { snapshotProfileTree } from "./m3d_package_acquisition_acceptance_helpers.ts";
import type { M3dPackageHttpsFixture } from "./m3d_package_https_fixture.types.ts";
import {
  areHubPanelMenuM3ePortsClosed,
  assertHubPanelMenuM3eProcessProfile,
  assertHubPanelMenuM3eUserJsDoesNotOverride,
  assertNoPreexistingHubPanelMenuM3eProcesses,
  captureHubPanelMenuM3eLaunchProcesses,
  connectHubPanelMenuM3eBrowser,
  hubPanelMenuM3eProcessExited,
  listHubPanelMenuM3eOwnedListeningPorts,
  ownedM3eProcesses,
  readHubPanelMenuM3eProcessRegistry,
  refreshHubPanelMenuM3eOwnedProcesses,
  runHubPanelMenuM3eOwnedCommand,
  seedHubPanelMenuM3eProfile,
  terminateHubPanelMenuM3eOwnedProcesses,
} from "./hub_panel_menu_m3e_process.ts";
import {
  createHubPanelMenuM3eFixtureBundle,
  installHubPanelMenuM3ePublicCa,
  startHubPanelMenuM3eHttpsFixture,
  waitForHubPanelMenuM3eFixtureIdle,
} from "./hub_panel_menu_m3e_fixture.ts";
import {
  assertHubPanelMenuM3ePostStartImmutability,
  assertHubPanelMenuM3eStartupObservation,
  bindHubPanelMenuM3eBrowserSandbox,
  closeHubPanelMenuM3eBrowser,
  executeHubPanelMenuM3eBrowserScript,
} from "./hub_panel_menu_m3e_observations.ts";
import type { MarionetteClient } from "./browser_connector.ts";
import type {
  HubPanelMenuM3eAcceptanceReport,
  HubPanelMenuM3eCaseResult,
  HubPanelMenuM3eLaunchRecord,
  HubPanelMenuM3eOwnedCommandResult,
  HubPanelMenuM3ePolicyBinding,
  HubPanelMenuM3eStartupEvidence,
  HubPanelMenuM3eStartupSession,
  MutableHubPanelMenuM3eCleanup,
} from "./hub_panel_menu_m3e_acceptance.types.ts";
import {
  createHubPanelMenuM3eCaseResults,
  HUB_PANEL_MENU_M3E_FINAL_CLEANUP_CASE_ID,
  HUB_PANEL_MENU_M3E_FUNCTIONAL_CASE_IDS,
  HUB_PANEL_MENU_M3E_REQUIRED_CASE_IDS,
  hubPanelMenuM3eProfileSnapshotsEqual,
  hubPanelMenuM3eVerdict,
} from "./hub_panel_menu_m3e_verdict.ts";
import { buildHubPanelMenuM3eBrowserScriptSuite } from "./hub_panel_menu_m3e_browser_scripts.ts";
import { runHubPanelMenuM3eSignedLifecycle } from "./hub_panel_menu_m3e_lifecycle.ts";
import { HubPanelMenuM3eFocusFailure } from "./hub_panel_menu_m3e_focus.ts";
import { runHubPanelMenuM3eOperationClose } from "./hub_panel_menu_m3e_operation_close.ts";
import type { HubPanelMenuM3eLifecycleContext } from "./hub_panel_menu_m3e_lifecycle.types.ts";
import {
  HubPanelMenuM3eRestartFailure,
  runHubPanelMenuM3eRestartRecovery,
} from "./hub_panel_menu_m3e_restart.ts";

const RUN_ROOT = join(PROJECT_ROOT, "_dist", "hub-panel-menu-m3e-runs");
const SHARED_PROFILE = join(PROJECT_ROOT, "_dist", "profile", "test");
const STARTUP_ROWS = [
  {
    id: "A",
    caseId: "startup-pinned",
    feature: true,
    manual: false,
    pinned: true,
    selection: "pinned",
  },
  {
    id: "B",
    caseId: "startup-legacy-control",
    feature: false,
    manual: true,
    pinned: false,
    selection: "legacy",
  },
  {
    id: "C",
    caseId: "startup-manual",
    feature: true,
    manual: true,
    pinned: false,
    selection: "manual",
  },
] as const;

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function recordCase(
  cases: HubPanelMenuM3eCaseResult[],
  id: string,
  status: "passed" | "failed",
  observations: Readonly<Record<string, boolean | number | string | null>> = {},
  failure: string | null = null,
): void {
  const index = cases.findIndex((result) => result.id === id);
  if (index < 0 || cases[index].status !== "not-run") {
    throw new Error(`M3e case was absent or recorded more than once: ${id}`);
  }
  cases[index] = {
    id,
    status,
    startedAt: new Date().toISOString(),
    completedAt: new Date().toISOString(),
    failure,
    observations,
  };
}

function canonicalJson(value: unknown): string {
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
  const serialized = JSON.stringify(value);
  if (serialized === undefined) {
    throw new Error("M3e observation is not serializable");
  }
  return serialized;
}

function decodeBase64(value: string): Uint8Array {
  return Uint8Array.from(atob(value), (character) => character.charCodeAt(0));
}

function equalBytes(left: Uint8Array, right: Uint8Array): boolean {
  return left.length === right.length &&
    left.every((byte, index) => byte === right[index]);
}

function readPolicyBinding(value: unknown): HubPanelMenuM3ePolicyBinding {
  if (
    !isRecord(value) || typeof value.hostSchema !== "number" ||
    typeof value.hostBuildId !== "string" ||
    typeof value.runtimeBuildId !== "string" ||
    !Array.isArray(value.capabilities) ||
    !value.capabilities.every((capability) => typeof capability === "string")
  ) throw new Error("M3e browser policy binding is invalid");
  if (
    value.hostSchema !== 2 || value.hostBuildId !== "m0-schema-2" ||
    value.runtimeBuildId.length === 0 ||
    !value.capabilities.includes("app-menu")
  ) {
    throw new Error(
      "M3e browser policy binding is incompatible with the current host",
    );
  }
  return {
    hostSchema: value.hostSchema,
    hostBuildId: value.hostBuildId,
    runtimeBuildId: value.runtimeBuildId,
    capabilities: value.capabilities,
  };
}

function summarizeStartupEvidence(
  value: unknown,
): HubPanelMenuM3eStartupEvidence {
  if (
    !isRecord(value) || !isRecord(value.sharedRegistry) ||
    !Array.isArray(value.windows) || !Array.isArray(value.ui)
  ) throw new Error("M3e startup evidence cannot be summarized");
  const selection = value.selection;
  if (
    selection !== "legacy" && selection !== "pinned" && selection !== "manual"
  ) throw new Error("M3e startup evidence has an invalid selection");
  const state = isRecord(value.pinnedState) ? value.pinnedState : null;
  const buttons = value.ui.filter(isRecord);
  const labels = new Set(
    buttons.map((item) => item.buttonLabel).filter(
      (label): label is string => typeof label === "string",
    ),
  );
  const enabled =
    buttons.length > 0 && buttons.every((item) => item.buttonEnabled === true)
      ? true
      : buttons.some((item) => item.buttonCount === 0)
      ? null
      : false;
  return {
    startupSelection: selection,
    normalWindowCount: value.windows.length,
    registryConnections: typeof value.sharedRegistry.connections === "number"
      ? value.sharedRegistry.connections
      : -1,
    hubButtonCount: buttons.reduce(
      (total, item) =>
        total + (typeof item.buttonCount === "number" ? item.buttonCount : 0),
      0,
    ),
    hubButtonEnabled: enabled,
    hubButtonImmediatelyAfterSettings: enabled === null
      ? null
      : buttons.every((item) => item.immediatelyAfterSettings === true),
    hubButtonLabel: labels.size === 1 ? [...labels][0] : null,
    pinnedStateVersion: typeof state?.selectedVersion === "string"
      ? state.selectedVersion
      : null,
    pinnedStateDigest: typeof state?.selectedDigest === "string"
      ? state.selectedDigest
      : null,
    pinnedStateDesiredEnabled: typeof state?.desiredEnabled === "boolean"
      ? state.desiredEnabled
      : null,
    pinnedStateHealth: state?.selectedHealth === "verified" ||
        state?.selectedHealth === "unverified"
      ? state.selectedHealth
      : null,
    pinnedStatePending: state ? state.pending !== null : null,
    pinnedStateBlocked: state ? state.blockedReason !== null : null,
    pinnedStateOutcomeStatus: isRecord(state?.lastOutcome) &&
        typeof state.lastOutcome.status === "string"
      ? state.lastOutcome.status
      : null,
  };
}

async function resolveFreshProfileRoot(raw: string): Promise<string> {
  if (!isAbsolute(raw)) {
    throw new Error("FLOORP_TEST_PROFILE_DIR must be an absolute fresh path");
  }
  const shared = resolve(SHARED_PROFILE);
  const parent = await Deno.realPath(dirname(raw));
  const root = join(parent, basename(raw));
  if (
    root === shared || root.startsWith(`${shared}/`) ||
    shared.startsWith(`${root}/`)
  ) throw new Error("M3e profile root overlaps the shared profile");
  try {
    await Deno.lstat(root);
    throw new Error("FLOORP_TEST_PROFILE_DIR must not already exist");
  } catch (error) {
    if (!(error instanceof Deno.errors.NotFound)) throw error;
  }
  return root;
}

async function readMarionettePort(): Promise<number> {
  const value = (await Deno.readTextFile(
    join(PROJECT_ROOT, "_dist", "marionette-port.txt"),
  )).trim();
  if (!/^\d+$/.test(value)) throw new Error("M3e Marionette port is invalid");
  const port = Number(value);
  if (port < 1 || port > 65_535) {
    throw new Error("M3e Marionette port is invalid");
  }
  return port;
}

async function runDevTool(
  action: "start" | "status",
  profilePath: string,
  logPrefix: string,
  cleanup: MutableHubPanelMenuM3eCleanup,
): Promise<HubPanelMenuM3eOwnedCommandResult> {
  const args = action === "start"
    ? ["task", "dev-tool", "start", "--port-offset", "auto"]
    : ["task", "dev-tool", "status"];
  let result;
  try {
    result = await runHubPanelMenuM3eOwnedCommand(
      Deno.execPath(),
      args,
      profilePath,
      `${logPrefix}.${action}.stdout.log`,
      `${logPrefix}.${action}.stderr.log`,
    );
  } catch (error) {
    cleanup.ownedCommandCleanupProven = false;
    throw error;
  }
  if (!result.cleanupProven) cleanup.ownedCommandCleanupProven = false;
  return result;
}

async function startStartupRow(
  row: typeof STARTUP_ROWS[number],
  taskId: string,
  profileRoot: string,
  runDirectory: string,
  launchFailures: string[],
  cleanup: MutableHubPanelMenuM3eCleanup,
  launches: HubPanelMenuM3eLaunchRecord[],
  beforeLaunch?: (profilePath: string) => Promise<void>,
  reuseProfile = false,
  launchId: string = row.id,
): Promise<HubPanelMenuM3eStartupSession> {
  const profilePath = join(profileRoot, row.id);
  const logPrefix = join(runDirectory, `browser-${launchId}`);
  const launchRecord: HubPanelMenuM3eLaunchRecord = {
    rowId: row.id,
    profilePath,
    identities: [],
    ports: [],
    failure: null,
  };
  launches.push(launchRecord);
  let client: MarionetteClient | null = null;
  let identityCaptureProven = true;
  try {
    await assertNoPreexistingHubPanelMenuM3eProcesses();
    cleanup.launchOwnershipProven = true;
    if (!reuseProfile) {
      await seedHubPanelMenuM3eProfile(profilePath, row.feature, row.manual);
    }
    if (beforeLaunch) await beforeLaunch(profilePath);
    const before = await ownedM3eProcesses();
    let startResult: HubPanelMenuM3eOwnedCommandResult | null = null;
    let startError: unknown = null;
    try {
      startResult = await runDevTool("start", profilePath, logPrefix, cleanup);
    } catch (error) {
      startError = error;
    }
    try {
      const after = await ownedM3eProcesses();
      launchRecord.identities = captureHubPanelMenuM3eLaunchProcesses(
        before,
        after,
        profilePath,
        await readHubPanelMenuM3eProcessRegistry(),
      );
    } catch (error) {
      identityCaptureProven = false;
      cleanup.launchOwnershipProven = false;
      throw error;
    }
    if (launchRecord.identities.length === 0) {
      identityCaptureProven = false;
      cleanup.launchOwnershipProven = false;
      throw new Error("M3e start created no provable owned process identities");
    }
    try {
      launchRecord.ports = await listHubPanelMenuM3eOwnedListeningPorts(
        launchRecord.identities,
        profilePath,
        `${logPrefix}.owned-ports`,
      );
    } catch (error) {
      cleanup.ownedCommandCleanupProven = false;
      cleanup.ownedPortsClosed = false;
      throw error;
    }
    if (launchRecord.ports.length === 0) {
      throw new Error("M3e start exposed no owned listening ports");
    }
    if (startError !== null) throw startError;
    if (!startResult || !startResult.completed || !startResult.success) {
      const failure = `M3e ${row.id} dev-tool launch failed`;
      launchFailures.push(failure);
      throw new Error(failure);
    }
    await assertHubPanelMenuM3eUserJsDoesNotOverride(profilePath);
    const browserProcess = assertHubPanelMenuM3eProcessProfile(
      await ownedM3eProcesses(),
      profilePath,
    );
    if (
      !launchRecord.identities.some((identity) =>
        identity.pid === browserProcess.pid &&
        identity.startTime === browserProcess.startTime &&
        identity.commandLine === browserProcess.commandLine
      )
    ) throw new Error("M3e browser process is not owned by this launch");
    const port = await readMarionettePort();
    if (!launchRecord.ports.includes(port)) {
      throw new Error(
        "M3e Marionette port is absent from owned-port inventory",
      );
    }
    const status = await runDevTool("status", profilePath, logPrefix, cleanup);
    if (
      !status.completed || !status.success || !status.cleanupProven ||
      !status.stdout.includes("Connected to browser.")
    ) throw new Error("M3e dev-tool status did not prove browser connectivity");
    client = await connectHubPanelMenuM3eBrowser();
    await client.setContext("chrome");
    bindHubPanelMenuM3eBrowserSandbox(
      client,
      taskId,
      row.id,
      `${logPrefix}.protocol-diagnostics.json`,
    );
    const profile = await executeHubPanelMenuM3eBrowserScript(client, {
      kind: "profile-path",
    });
    if (!isRecord(profile) || profile.profileDir !== profilePath) {
      throw new Error("M3e in-browser profile path differs before mutation");
    }
    const observation = await executeHubPanelMenuM3eBrowserScript(client, {
      kind: "startup-observation",
      payload: { testId: taskId, rowId: row.id },
    });
    assertHubPanelMenuM3eStartupObservation(observation, {
      profilePath,
      featureEnabled: row.feature,
      manualHostEnabled: row.manual,
      pinnedRootExists: row.pinned,
      selection: row.selection,
    });
    const immutability = await executeHubPanelMenuM3eBrowserScript(client, {
      kind: "post-start-immutability",
      payload: {
        testId: taskId,
        rowId: row.id,
        expectedSelection: row.selection,
        toggleMode: row.id === "B" ? "test" : "production",
        toggleFeature: row.id === "B",
        toggleManual: row.id === "A",
      },
    });
    assertHubPanelMenuM3ePostStartImmutability(
      observation,
      immutability,
      row.selection,
    );
    const startupEvidence = summarizeStartupEvidence(observation);
    return {
      id: row.id,
      profilePath,
      client,
      statusOutputPath: `${logPrefix}.status.stdout.log`,
      launchRecord,
      startupEvidence,
    };
  } catch (error) {
    const failure = String(error);
    launchRecord.failure = failure;
    if (!launchFailures.some((item) => item.includes(row.id))) {
      launchFailures.push(`${row.id}: ${failure}`);
    }
    if (client) await closeHubPanelMenuM3eBrowser(client).catch(() => {});
    if (launchRecord.identities.length > 0) {
      await refreshAndStopOwnedLaunch(
        launchRecord,
        profilePath,
        `${logPrefix}.failed-start`,
        cleanup,
      );
    }
    if (!identityCaptureProven) {
      cleanup.launchOwnershipProven = false;
    }
    throw error;
  }
}

async function stopStartupRow(
  session: HubPanelMenuM3eStartupSession,
  cleanup: MutableHubPanelMenuM3eCleanup,
): Promise<boolean> {
  let logPreserved = false;
  try {
    await Deno.copyFile(
      join(PROJECT_ROOT, "_dist", "dev-server.log"),
      `${session.statusOutputPath}.dev-server.log`,
    );
    logPreserved = true;
  } catch {
    logPreserved = false;
  }
  let clientClosed = false;
  try {
    await closeHubPanelMenuM3eBrowser(session.client);
    clientClosed = true;
  } catch {
    clientClosed = false;
  }
  const stopped = await refreshAndStopOwnedLaunch(
    session.launchRecord,
    session.profilePath,
    `${session.statusOutputPath}.teardown`,
    cleanup,
  );
  return logPreserved && clientClosed && stopped;
}

async function refreshAndStopOwnedLaunch(
  launch: HubPanelMenuM3eLaunchRecord,
  profilePath: string,
  logPath: string,
  cleanup: MutableHubPanelMenuM3eCleanup,
): Promise<boolean> {
  let processes: Awaited<ReturnType<typeof ownedM3eProcesses>>;
  try {
    processes = await ownedM3eProcesses();
  } catch {
    cleanup.launchOwnershipProven = false;
    cleanup.ownedProcessesStopped = false;
    cleanup.ownedPortsClosed = false;
    cleanup.ownedCommandCleanupProven = false;
    return false;
  }
  let refreshed = refreshHubPanelMenuM3eOwnedProcesses(
    launch.identities,
    processes,
  );
  if (!refreshed.ownershipProven) {
    processes = await ownedM3eProcesses();
    refreshed = refreshHubPanelMenuM3eOwnedProcesses(
      launch.identities,
      processes,
    );
  }
  await Deno.writeTextFile(
    `${logPath}.process-refresh.json`,
    JSON.stringify(
      {
        ownershipProven: refreshed.ownershipProven,
        identities: launch.identities.map((identity) => {
          const current = processes.find((process) =>
            process.pid === identity.pid
          );
          return {
            pid: identity.pid,
            expectedStartTime: identity.startTime,
            observedStartTime: current?.startTime ?? null,
            commandLineMatches: current
              ? current.commandLine === identity.commandLine
              : null,
          };
        }),
        livePids: refreshed.liveIdentities.map((identity) => identity.pid),
      },
      null,
      2,
    ),
    { mode: 0o600 },
  );
  launch.identities = refreshed.identities;
  cleanup.launchOwnershipProven = cleanup.launchOwnershipProven &&
    refreshed.ownershipProven;
  let portInventoryProven = true;
  try {
    const refreshedPorts = await listHubPanelMenuM3eOwnedListeningPorts(
      refreshed.liveIdentities,
      profilePath,
      `${logPath}.owned-ports`,
    );
    launch.ports = [...new Set([...launch.ports, ...refreshedPorts])].sort(
      (left, right) => left - right,
    );
  } catch {
    portInventoryProven = false;
    cleanup.ownedCommandCleanupProven = false;
  }
  const stopped = await terminateHubPanelMenuM3eOwnedProcesses(
    refreshed.liveIdentities,
  ).catch(() => false);
  const portsClosed = portInventoryProven && stopped &&
    areHubPanelMenuM3ePortsClosed(launch.ports);
  cleanup.ownedProcessesStopped = cleanup.ownedProcessesStopped && stopped;
  cleanup.ownedPortsClosed = cleanup.ownedPortsClosed && portsClosed;
  return refreshed.ownershipProven && stopped && portsClosed;
}

export function assertInertnessSnapshotsEqual(
  before: unknown,
  after: unknown,
): void {
  if (!isRecord(before) || !isRecord(after)) {
    throw new Error("M3e acquisition snapshot is invalid");
  }
  for (
    const key of [
      "profileDir",
      "evaluationCount",
      "sharedRegistry",
      "hostDiagnostics",
      "hostMetadata",
      "taskStateToken",
      "canonicalRootSnapshot",
      "journalRootSnapshot",
      "packageCacheSnapshot",
      "ui",
    ]
  ) {
    if (canonicalJson(before[key]) !== canonicalJson(after[key])) {
      throw new Error(`M3e acquisition changed ${key}`);
    }
  }
}

async function writeReport(
  reportPath: string,
  report: HubPanelMenuM3eAcceptanceReport,
): Promise<void> {
  await Deno.writeTextFile(reportPath, `${JSON.stringify(report, null, 2)}\n`, {
    mode: 0o600,
  });
}

async function main(): Promise<void> {
  const startedAt = new Date().toISOString();
  const runId = crypto.randomUUID();
  const runDirectory = join(RUN_ROOT, runId);
  await Deno.mkdir(RUN_ROOT, { recursive: true, mode: 0o700 });
  await Deno.mkdir(runDirectory, { mode: 0o700 });
  const reportPath = join(runDirectory, "acceptance-report.json");
  const cases = createHubPanelMenuM3eCaseResults();
  const launchFailures: string[] = [];
  const cleanup: MutableHubPanelMenuM3eCleanup = {
    launchOwnershipProven: false,
    ownedProcessesStopped: true,
    ownedPortsClosed: true,
    fixtureClosed: false,
    fixtureObservationsBounded: false,
    profilesRemoved: false,
    sharedProfileUnchanged: false,
    ownedCommandCleanupProven: true,
    reportWritten: false,
  };
  let sharedProfileBefore:
    | Awaited<ReturnType<typeof snapshotProfileTree>>
    | null = null;
  let sharedProfileAfter:
    | Awaited<ReturnType<typeof snapshotProfileTree>>
    | null = null;
  let profileRoot: string | null = null;
  let runFailure: string | null = null;
  let policyBinding: HubPanelMenuM3ePolicyBinding | null = null;
  let bundle:
    | Awaited<ReturnType<typeof createHubPanelMenuM3eFixtureBundle>>
    | null = null;
  let fixture: M3dPackageHttpsFixture | null = null;
  const activeSessions: HubPanelMenuM3eStartupSession[] = [];
  const launches: HubPanelMenuM3eLaunchRecord[] = [];
  const packageMetadata:
    HubPanelMenuM3eAcceptanceReport["packageMetadata"][number][] = [];
  let currentCaseId: string | null = null;
  try {
    if (Deno.build.os !== "linux") {
      throw new Error("M3e native acceptance requires Linux");
    }
    const profileRaw = Deno.env.get("FLOORP_TEST_PROFILE_DIR");
    if (!profileRaw) throw new Error("FLOORP_TEST_PROFILE_DIR is required");
    for (const script of buildHubPanelMenuM3eBrowserScriptSuite()) {
      new Function(script);
    }
    profileRoot = await resolveFreshProfileRoot(profileRaw);
    await Deno.mkdir(profileRoot, { mode: 0o700 });
    sharedProfileBefore = await snapshotProfileTree(SHARED_PROFILE);
    await assertNoPreexistingHubPanelMenuM3eProcesses();
    cleanup.launchOwnershipProven = true;
    if (!profileRoot) throw new Error("M3e owned profile root was not created");
    const ownedProfileRoot = profileRoot;
    const rowA = STARTUP_ROWS[0];
    currentCaseId = rowA.caseId;
    const sessionA = await startStartupRow(
      rowA,
      runId,
      ownedProfileRoot,
      runDirectory,
      launchFailures,
      cleanup,
      launches,
    );
    activeSessions.push(sessionA);
    const bindingValue = await executeHubPanelMenuM3eBrowserScript(
      sessionA.client,
      { kind: "policy-binding" },
    );
    policyBinding = readPolicyBinding(bindingValue);
    recordCase(cases, rowA.caseId, "passed", {
      pinnedRoot: true,
      mode: "dev",
      devToolStatusOutput: sessionA.statusOutputPath,
      ...sessionA.startupEvidence,
    });
    currentCaseId = null;
    if (!await stopStartupRow(sessionA, cleanup)) {
      throw new Error("M3e A browser process cleanup was not proven");
    }
    activeSessions.splice(activeSessions.indexOf(sessionA), 1);

    bundle = await createHubPanelMenuM3eFixtureBundle(policyBinding);
    packageMetadata.push(
      ...Object.values(bundle.packages).map((item) => ({
        version: item.version,
        releaseSequence: item.releaseSequence,
        packageSha256: item.packageSha256,
        byteLength: item.byteLength,
      })),
    );
    fixture = await startHubPanelMenuM3eHttpsFixture(bundle);

    const rowB = STARTUP_ROWS[1];
    currentCaseId = rowB.caseId;
    const sessionB = await startStartupRow(
      rowB,
      runId,
      ownedProfileRoot,
      runDirectory,
      launchFailures,
      cleanup,
      launches,
    );
    activeSessions.push(sessionB);
    if (
      !sessionA.startupEvidence.hubButtonLabel ||
      sessionA.startupEvidence.hubButtonLabel !==
        sessionB.startupEvidence.hubButtonLabel
    ) throw new Error("M3e pinned and legacy HUB labels differ");
    recordCase(cases, rowB.caseId, "passed", {
      pinnedRoot: false,
      mode: "dev",
      devToolStatusOutput: sessionB.statusOutputPath,
      ...sessionB.startupEvidence,
    });
    currentCaseId = null;
    if (!await stopStartupRow(sessionB, cleanup)) {
      throw new Error("M3e B browser process cleanup was not proven");
    }
    activeSessions.splice(activeSessions.indexOf(sessionB), 1);

    const rowC = STARTUP_ROWS[2];
    currentCaseId = rowC.caseId;
    const sessionC = await startStartupRow(
      rowC,
      runId,
      ownedProfileRoot,
      runDirectory,
      launchFailures,
      cleanup,
      launches,
      async (profilePath) => {
        if (!fixture) throw new Error("M3e HTTPS fixture is not running");
        await installHubPanelMenuM3ePublicCa(
          profilePath,
          fixture.caCertificatePem,
          runDirectory,
        );
      },
    );
    activeSessions.push(sessionC);
    const manualBinding = readPolicyBinding(
      await executeHubPanelMenuM3eBrowserScript(sessionC.client, {
        kind: "policy-binding",
      }),
    );
    if (canonicalJson(policyBinding) !== canonicalJson(manualBinding)) {
      throw new Error("M3e core policy binding changed between A and C");
    }
    recordCase(cases, rowC.caseId, "passed", {
      pinnedRoot: false,
      mode: "dev",
      devToolStatusOutput: sessionC.statusOutputPath,
      ...sessionC.startupEvidence,
    });
    currentCaseId = null;

    if (!fixture || !bundle) {
      throw new Error("M3e signed HTTPS fixture is unavailable");
    }
    currentCaseId = "empty-acquisition-inert";
    const beforeObservation = await executeHubPanelMenuM3eBrowserScript(
      sessionC.client,
      { kind: "capture-inertness", payload: { testId: runId, rowId: "C" } },
    );
    const beforeFixture = fixture.snapshot();
    const v1 = bundle.packages["hub-menu-v1"];
    fixture.setMode("valid-streaming");
    const detached = await executeHubPanelMenuM3eBrowserScript(
      sessionC.client,
      {
        kind: "download-package",
        payload: {
          request: {
            origin: fixture.trustedOrigin,
            packageSha256: v1.packageSha256,
            policy: bundle.policy,
          },
        },
      },
    );
    if (!isRecord(detached) || typeof detached.packageBase64 !== "string") {
      throw new Error(
        "M3e HTTPS download did not return detached package bytes",
      );
    }
    const detachedBytes = decodeBase64(detached.packageBase64);
    if (!equalBytes(detachedBytes, v1.bytes)) {
      throw new Error(
        "M3e HTTPS download bytes differ from the signed package",
      );
    }
    const afterObservation = await executeHubPanelMenuM3eBrowserScript(
      sessionC.client,
      { kind: "capture-inertness", payload: { testId: runId, rowId: "C" } },
    );
    assertInertnessSnapshotsEqual(beforeObservation, afterObservation);
    const afterFixture = await waitForHubPanelMenuM3eFixtureIdle(
      fixture,
      5_000,
    );
    const route =
      afterFixture.observations[afterFixture.observations.length - 1];
    if (
      afterFixture.requests !== beforeFixture.requests + 1 ||
      afterFixture.observations.length !==
        beforeFixture.observations.length + 1 ||
      afterFixture.activeStreams !== 0 ||
      afterFixture.activeConnections !== 0 ||
      afterFixture.cancellations !== beforeFixture.cancellations ||
      afterFixture.redirectTargets !== beforeFixture.redirectTargets ||
      afterFixture.observations.slice(beforeFixture.observations.length).some((
        observation,
      ) => observation.origin !== "trusted") ||
      afterFixture.observationOverflow ||
      afterFixture.observations.length > 64 ||
      !route || route.origin !== "trusted" ||
      route.mode !== "valid-streaming" ||
      route.routePath !==
        `/floorp-feature-packages/v1/hub-panel-menu/${v1.packageSha256}.json` ||
      route.featureId !== "hub-panel-menu" ||
      route.packageSha256 !== v1.packageSha256 ||
      route.cookiePresent || route.authorizationPresent ||
      route.referrerPresent ||
      route.seedCookieMarkerMatch || route.acquisitionCookieMarkerMatch ||
      route.authorizationMarkerMatch || route.referrerMarkerMatch
    ) throw new Error("M3e empty-host HTTPS fixture was not idle and bounded");
    cleanup.fixtureObservationsBounded = true;
    recordCase(cases, "empty-acquisition-inert", "passed", {
      exactDetachedBytes: true,
      evaluationsUnchanged: true,
      profileUnchanged: true,
      requestCount: afterFixture.requests,
      httpsOrigin: route.origin,
      httpsMode: route.mode,
      httpsRoute: route.routePath,
      httpsFeatureId: route.featureId,
      httpsPackageSha256: route.packageSha256,
      activeStreams: afterFixture.activeStreams,
      activeConnections: afterFixture.activeConnections,
      credentialHeadersAbsent: !route.cookiePresent &&
        !route.authorizationPresent && !route.referrerPresent,
    });
    currentCaseId = null;
    const lifecycleContext: HubPanelMenuM3eLifecycleContext = {
      inputDiagnosticsPath: join(runDirectory, "input-action-diagnostics.json"),
      client: sessionC.client,
      testId: runId,
      bundle,
      fixture,
      label: sessionA.startupEvidence.hubButtonLabel!,
      v1DownloadedBase64: detached.packageBase64,
      async runCase(id, run) {
        currentCaseId = id;
        const observations = await run();
        recordCase(cases, id, "passed", observations);
        currentCaseId = null;
      },
    };
    await runHubPanelMenuM3eSignedLifecycle(lifecycleContext);
    await runHubPanelMenuM3eOperationClose(lifecycleContext);
    let restartSession = sessionC;
    let restartCount = 0;
    const restartEvidence: unknown[] = [];
    await runHubPanelMenuM3eRestartRecovery({
      ...lifecycleContext,
      profilePath: sessionC.profilePath,
      windowDiagnosticsPath: join(runDirectory, "restart-window-context.json"),
      getClient: () => restartSession.client,
      async restart(mutate) {
        const previous = restartSession;
        const before = await ownedM3eProcesses();
        const main = assertHubPanelMenuM3eProcessProfile(
          before,
          previous.profilePath,
        );
        const browserPids = new Set([main.pid]);
        let changed = true;
        while (changed) {
          changed = false;
          for (const process of before) {
            if (
              browserPids.has(process.ppid) && !browserPids.has(process.pid)
            ) {
              browserPids.add(process.pid);
              changed = true;
            }
          }
        }
        const browserIdentities = before.filter((process) =>
          browserPids.has(process.pid)
        ).map(({ pid, startTime, commandLine }) => ({
          pid,
          startTime,
          commandLine,
        }));
        const requestedAt = Date.now();
        await previous.client.sendWithTimeout("WebDriver:ExecuteScript", {
          script:
            "Services.tm.dispatchToMainThread(() => Services.startup.quit(Components.interfaces.nsIAppStartup.eAttemptQuit)); return true;",
          args: [],
        }, 5000);
        previous.client.abort();
        let exited = false;
        const deadline = Date.now() + 30000;
        while (Date.now() < deadline) {
          const states = await Promise.all(
            browserIdentities.map(async (identity) => {
              let stat: string | null;
              try {
                stat = await Deno.readTextFile(`/proc/${identity.pid}/stat`);
              } catch (error) {
                if (error instanceof Deno.errors.NotFound) stat = null;
                else throw error;
              }
              return {
                pid: identity.pid,
                exited: hubPanelMenuM3eProcessExited(identity, stat),
              };
            }),
          );
          if (
            states.every((state) => state.exited)
          ) {
            exited = true;
            break;
          }
          await new Promise((resolve) => setTimeout(resolve, 100));
        }
        if (!exited) throw new Error("M3e orderly application exit unproven");
        const proof = {
          requestedAt,
          exitedAt: Date.now(),
          browserIdentities,
          orderlyQuit: "Services.startup.quit(eAttemptQuit)",
          browserExitProvenBeforeOwnedToolTeardown: true,
          profilePath: previous.profilePath,
        };
        restartEvidence.push(proof);
        await Deno.writeTextFile(
          join(runDirectory, "orderly-restarts.json"),
          JSON.stringify(restartEvidence, null, 2),
          { mode: 0o600 },
        );
        await Deno.copyFile(
          join(PROJECT_ROOT, "_dist", "dev-server.log"),
          `${previous.statusOutputPath}.orderly-quit.dev-server.log`,
        );
        const toolLaunch = {
          ...previous.launchRecord,
          identities: previous.launchRecord.identities.filter((identity) =>
            !browserPids.has(identity.pid)
          ),
        };
        const stopped = await refreshAndStopOwnedLaunch(
          toolLaunch,
          previous.profilePath,
          `${previous.statusOutputPath}.orderly-quit-tool-teardown`,
          cleanup,
        );
        previous.launchRecord.identities = [
          ...new Map(
            [...previous.launchRecord.identities, ...toolLaunch.identities].map(
              (identity) => [identity.pid, identity],
            ),
          ).values(),
        ];
        previous.launchRecord.ports = toolLaunch.ports;
        if (!stopped) {
          throw new Error("M3e post-exit owned tool cleanup unproven");
        }
        activeSessions.splice(activeSessions.indexOf(previous), 1);
        const mutation = mutate ? await mutate() : null;
        restartCount++;
        restartSession = await startStartupRow(
          rowC,
          runId,
          ownedProfileRoot,
          runDirectory,
          launchFailures,
          cleanup,
          launches,
          undefined,
          true,
          `C-restart-${restartCount}`,
        );
        activeSessions.push(restartSession);
        return {
          applicationQuit: JSON.stringify(proof),
          mutation: JSON.stringify(mutation),
          sameProfile: restartSession.profilePath === previous.profilePath,
          startup: JSON.stringify(restartSession.startupEvidence),
          launch: JSON.stringify(restartSession.launchRecord),
        };
      },
    });
  } catch (error) {
    runFailure = String(error);
    if (
      currentCaseId &&
      cases.find((result) => result.id === currentCaseId)?.status === "not-run"
    ) {
      recordCase(
        cases,
        currentCaseId,
        "failed",
        error instanceof HubPanelMenuM3eFocusFailure ||
          error instanceof HubPanelMenuM3eRestartFailure
          ? error.evidence
          : {},
        runFailure,
      );
    }
  } finally {
    let remainingBrowserCleanupProven = true;
    for (const session of [...activeSessions].reverse()) {
      try {
        const stopped = await stopStartupRow(session, cleanup);
        if (stopped) {
          activeSessions.splice(activeSessions.indexOf(session), 1);
        } else {
          remainingBrowserCleanupProven = false;
        }
      } catch {
        remainingBrowserCleanupProven = false;
      }
    }
    cleanup.ownedProcessesStopped = cleanup.ownedProcessesStopped &&
      remainingBrowserCleanupProven && activeSessions.length === 0;
    cleanup.ownedPortsClosed = cleanup.ownedPortsClosed &&
      remainingBrowserCleanupProven && activeSessions.length === 0;
    if (fixture) {
      try {
        const evidence = await fixture.close();
        cleanup.fixtureClosed = evidence.closed && evidence.listenersClosed &&
          evidence.activeConnectionsClosed && evidence.stallsReleased &&
          evidence.acceptLoopsSettled && evidence.cleanupErrorCount === 0;
        cleanup.fixtureObservationsBounded =
          cleanup.fixtureObservationsBounded &&
          !fixture.snapshot().observationOverflow;
      } catch {
        cleanup.fixtureClosed = false;
      }
    }
    try {
      sharedProfileAfter = await snapshotProfileTree(SHARED_PROFILE);
      cleanup.sharedProfileUnchanged = hubPanelMenuM3eProfileSnapshotsEqual(
        sharedProfileBefore,
        sharedProfileAfter,
      );
    } catch (error) {
      runFailure ??= `M3e final shared-profile snapshot failed: ${
        String(error)
      }`;
      cleanup.sharedProfileUnchanged = false;
    }
  }

  const functionalCases = cases.filter((result) =>
    result.id !== HUB_PANEL_MENU_M3E_FINAL_CLEANUP_CASE_ID
  );
  const functionalComplete =
    functionalCases.length === HUB_PANEL_MENU_M3E_FUNCTIONAL_CASE_IDS.length &&
    functionalCases.every((result) => result.status === "passed");
  if (!functionalComplete) {
    runFailure ??= "M3e functional required cases are incomplete";
  }
  const createReport = (passed: boolean): HubPanelMenuM3eAcceptanceReport => ({
    schemaVersion: 1,
    runId,
    startedAt,
    completedAt: new Date().toISOString(),
    passed,
    cases,
    launchFailures,
    runFailure,
    sharedProfileBefore,
    sharedProfileAfter,
    cleanup,
    launches,
    packageMetadata,
    policyBinding,
    reportPath,
  });
  try {
    await writeReport(reportPath, createReport(false));
  } catch (error) {
    cleanup.reportWritten = false;
    runFailure ??= `M3e pre-cleanup report serialization failed: ${
      String(error)
    }`;
    console.error(JSON.stringify(createReport(false), null, 2));
    Deno.exitCode = 1;
    return;
  }
  cleanup.reportWritten = true;
  const eligibleForProfileRemoval = functionalComplete && runFailure === null &&
    launchFailures.length === 0 && cleanup.launchOwnershipProven &&
    cleanup.ownedProcessesStopped && cleanup.ownedPortsClosed &&
    cleanup.fixtureClosed && cleanup.fixtureObservationsBounded &&
    cleanup.sharedProfileUnchanged && cleanup.ownedCommandCleanupProven;
  let cleanupCaseFailure: string | null = null;
  if (eligibleForProfileRemoval && profileRoot) {
    try {
      await Deno.remove(profileRoot, { recursive: true });
      await Deno.lstat(profileRoot);
      cleanupCaseFailure = "M3e owned profile remained after cleanup";
    } catch (error) {
      if (error instanceof Deno.errors.NotFound) cleanup.profilesRemoved = true;
      else {cleanupCaseFailure = `M3e owned profile cleanup failed: ${
          String(error)
        }`;}
    }
  } else {
    cleanupCaseFailure =
      "M3e profile cleanup was not eligible before all evidence proofs";
  }
  if (cleanup.profilesRemoved) {
    recordCase(cases, HUB_PANEL_MENU_M3E_FINAL_CLEANUP_CASE_ID, "passed", {
      profilesRemoved: true,
      ownershipAndPortProof: true,
      evidenceReportWritten: true,
    });
  } else {
    runFailure ??= cleanupCaseFailure;
    recordCase(
      cases,
      HUB_PANEL_MENU_M3E_FINAL_CLEANUP_CASE_ID,
      "failed",
      { profilesRemoved: false },
      cleanupCaseFailure,
    );
  }
  if (cases.some((result) => result.status === "not-run")) {
    runFailure ??= "M3e acceptance contains required cases that were not run";
  }
  const complete =
    cases.length === HUB_PANEL_MENU_M3E_REQUIRED_CASE_IDS.length &&
    cases.every((result) => result.status === "passed");
  const passed = hubPanelMenuM3eVerdict({
    complete,
    runFailure,
    cases,
    launchFailures,
    sharedProfileBefore,
    sharedProfileAfter,
    cleanup,
  });
  const report = createReport(passed);
  try {
    await writeReport(reportPath, report);
  } catch (error) {
    cleanup.reportWritten = false;
    runFailure ??= `M3e final report serialization failed: ${String(error)}`;
    console.error(JSON.stringify(createReport(false), null, 2));
    Deno.exitCode = 1;
    return;
  }
  console.log(JSON.stringify(report, null, 2));
  if (!passed) Deno.exitCode = 1;
}

if (import.meta.main) {
  if (!Deno.args.includes("--run-native")) {
    console.error("Refusing native M3e launches without --run-native");
    Deno.exitCode = 2;
  } else {
    await main();
  }
}
