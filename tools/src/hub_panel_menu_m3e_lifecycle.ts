// SPDX-License-Identifier: MPL-2.0

import { HUB_PANEL_MENU_FIXTURES } from "../../browser-features/modules/modules/feature-host/HubPanelMenuFixtures.sys.mts";
import { runHubPanelMenuM3eFocus } from "./hub_panel_menu_m3e_focus.ts";
import { executeHubPanelMenuM3eBrowserScript } from "./hub_panel_menu_m3e_observations.ts";
import { waitForHubPanelMenuM3eFixtureIdle } from "./hub_panel_menu_m3e_fixture.ts";
import type { HubPanelMenuM3eSignedPackage } from "./hub_panel_menu_m3e_acceptance.types.ts";
import type {
  HubPanelMenuM3eLifecycleContext,
  HubPanelMenuM3eLifecycleEvidence,
  HubPanelMenuM3eLifecycleExpected,
} from "./hub_panel_menu_m3e_lifecycle.types.ts";

function check(value: unknown, label: string): asserts value {
  if (!value) throw new Error(`M3e lifecycle ${label}`);
}

function record(value: unknown): Record<string, unknown> {
  check(
    typeof value === "object" && value !== null && !Array.isArray(value),
    "record missing",
  );
  return value as Record<string, unknown>;
}

function same(left: unknown, right: unknown): boolean {
  return JSON.stringify(left) === JSON.stringify(right);
}

export function assertHubPanelMenuM3eLateDelivery(value: unknown): void {
  const snapshot = record(value);
  const expected = record(snapshot.registryOwner);
  const matches = (owner: unknown) => {
    const actual = record(owner);
    return actual.featureId === expected.featureId &&
      actual.hostEpoch === expected.hostEpoch &&
      actual.generation === expected.generation;
  };
  check(
    Array.isArray(snapshot.lateDeliveries) &&
      snapshot.lateDeliveries.length > 0,
    "late delivery evidence missing",
  );
  const deliveries = snapshot.lateDeliveries.map(record);
  check(
    deliveries.every((entry) =>
      entry.role === "late" && matches(entry.owner)
    ) && deliveries.some((entry) => entry.acknowledgement === "rendered"),
    "late adapter acknowledged a non-active generation",
  );
}

export function assertHubPanelMenuM3eSignedState(
  value: unknown,
  expected: HubPanelMenuM3eLifecycleExpected,
): void {
  const state = record(value);
  const diagnostics = record(state.diagnostics);
  const registry = record(state.registry);
  const journal = record(state.journal);
  const lifecycle = record(journal.lifecycle);
  const selected = expected.selected;
  check(
    journal.schemaVersion === 1 && journal.featureId === "hub-panel-menu" &&
      lifecycle.schemaVersion === 2 && lifecycle.featureId === "hub-panel-menu",
    "journal identity differs",
  );
  check(
    state.canonicalRootExists === false && diagnostics.blocked === false &&
      diagnostics.blockCategory === null && lifecycle.pending === null &&
      lifecycle.blockedReason === null,
    "ownership or pending state differs",
  );
  check(
    diagnostics.desiredEnabled === expected.enabled &&
      lifecycle.desiredEnabled === expected.enabled &&
      diagnostics.selectedHealth === expected.health &&
      lifecycle.selectedHealth === expected.health,
    "enabled/health differs",
  );
  check(
    Number.isSafeInteger(diagnostics.revision) &&
      diagnostics.revision === lifecycle.stateRevision,
    "revision differs",
  );
  check(
    diagnostics.selectedPackageSha256 === (selected?.packageSha256 ?? null) &&
      lifecycle.selectedVersion === (selected?.packageSha256 ?? null) &&
      lifecycle.selectedDigest === (selected?.packageSha256 ?? null),
    "selected identity differs",
  );
  if (selected) {
    const metadata = record(diagnostics.selected);
    const fixture = HUB_PANEL_MENU_FIXTURES[
      selected.version as keyof typeof HUB_PANEL_MENU_FIXTURES
    ];
    check(
      fixture && metadata.version === selected.version &&
        metadata.packageSha256 === selected.packageSha256 &&
        metadata.releaseSequence === selected.releaseSequence &&
        metadata.sourceSha256 === fixture.digest,
      "selected metadata differs",
    );
    if (expected.health === "verified") {
      check(
        record(lifecycle.selectedHealthBuild).hostBuildId === "m0-schema-2",
        "health build differs",
      );
    }
  } else check(diagnostics.selected === null, "unexpected selection");
  if (expected.highWater) {
    const highWater = record(journal.highWater);
    check(
      highWater.releaseSequence === expected.highWater.releaseSequence &&
        highWater.packageSha256 === expected.highWater.packageSha256,
      "high-water differs",
    );
  } else check(journal.highWater === null, "unexpected high-water");
  const live = expected.enabled && !expected.closed;
  if (live) {
    const owner = record(state.registryOwner);
    check(
      owner.featureId === "hub-panel-menu" &&
        typeof owner.hostEpoch === "string" &&
        Number.isSafeInteger(owner.generation) &&
        Number(owner.generation) > 0 && typeof owner.leaseId === "string",
      "registry generation identity differs",
    );
  } else {check(
      state.registryOwner === null,
      "unexpected published registry owner",
    );}
  check(
    registry.connections === expected.normalWindows &&
      registry.enabled === live && registry.published === live &&
      registry.staged === live && registry.pendingAcknowledgements === 0,
    "registry differs",
  );
  check(
    Array.isArray(state.ui) && state.ui.length === expected.normalWindows + 1,
    "window population differs",
  );
  const ui = state.ui.map(record);
  check(
    ui.filter((item) => item.private === true).length === 1 &&
      ui.filter((item) => item.private === false).length ===
        expected.normalWindows,
    "window privacy differs",
  );
  for (const item of ui) {
    const present = item.private === true || live;
    check(item.count === (present ? 1 : 0), "button count differs");
    if (present) {
      check(
        item.enabled === true && item.label === expected.label &&
          item.afterSettings === true && typeof item.nodeId === "number",
        "button contract differs",
      );
    }
    check(item.privateNodeUnchanged === true, "private legacy node changed");
  }
  check(
    Array.isArray(state.metadata) && Array.isArray(journal.packages),
    "catalog missing",
  );
  for (const entry of [...state.metadata, ...journal.packages]) {
    const metadata = record(entry);
    const fixture = HUB_PANEL_MENU_FIXTURES[
      metadata.version as keyof typeof HUB_PANEL_MENU_FIXTURES
    ];
    check(
      fixture && fixture.digest === metadata.sourceSha256 &&
        typeof metadata.packageSha256 === "string" &&
        /^[0-9a-f]{64}$/.test(metadata.packageSha256),
      "catalog metadata differs",
    );
  }
}

export function assertHubPanelMenuM3eReplacement(
  before: unknown,
  after: unknown,
): void {
  const oldOwner = record(record(before).registryOwner);
  const nextOwner = record(record(after).registryOwner);
  check(
    oldOwner.hostEpoch === nextOwner.hostEpoch &&
      Number(oldOwner.generation) < Number(nextOwner.generation) &&
      oldOwner.leaseId !== nextOwner.leaseId,
    "registry replacement identity differs",
  );
  const oldUi = record(before).ui;
  const newUi = record(after).ui;
  check(Array.isArray(oldUi) && Array.isArray(newUi), "replacement UI missing");
  for (const old of oldUi.map(record)) {
    const next = newUi.map(record).find((item) => item.role === old.role);
    check(
      next &&
        (old.private === true
          ? old.nodeId === next.nodeId
          : old.nodeId !== next.nodeId && next.count === 1),
      "generation node identity differs",
    );
  }
}

export async function runHubPanelMenuM3eSignedLifecycle(
  context: HubPanelMenuM3eLifecycleContext,
): Promise<void> {
  const { client, testId, bundle, fixture, label, runCase } = context;
  const payload = { testId, rowId: "C" };
  const invoke = (
    action: string,
    extra: Readonly<Record<string, unknown>> = {},
  ) =>
    executeHubPanelMenuM3eBrowserScript(client, {
      kind: "lifecycle",
      payload: { ...payload, action, ...extra },
    });
  const capture = () =>
    executeHubPanelMenuM3eBrowserScript(client, {
      kind: "capture-inertness",
      payload,
    });
  const snapshot = async () => record(await invoke("snapshot"));
  const v1 = bundle.packages["hub-menu-v1"];
  const v2 = bundle.packages["hub-menu-v2"];
  const broken = bundle.packages["hub-menu-broken"];
  let normalWindows = 2;
  let current: Record<string, unknown>;
  let bytesV2 = "";
  const stateIs = (
    state: unknown,
    selected: HubPanelMenuM3eSignedPackage | null,
    highWater: HubPanelMenuM3eSignedPackage | null,
    enabled: boolean,
    health: "verified" | "unverified" = "verified",
    closed = false,
  ) =>
    assertHubPanelMenuM3eSignedState(state, {
      selected,
      highWater,
      enabled,
      health,
      normalWindows,
      label,
      closed,
    });
  const evidence = (
    state: Record<string, unknown>,
    extra: HubPanelMenuM3eLifecycleEvidence = {},
  ): HubPanelMenuM3eLifecycleEvidence => ({
    snapshot: JSON.stringify(state),
    ...extra,
  });
  async function download(pkg: HubPanelMenuM3eSignedPackage): Promise<string> {
    const before = fixture.snapshot();
    fixture.setMode("valid-streaming");
    const result = record(
      await executeHubPanelMenuM3eBrowserScript(client, {
        kind: "download-package",
        payload: {
          request: {
            origin: fixture.trustedOrigin,
            packageSha256: pkg.packageSha256,
            policy: bundle.policy,
          },
        },
      }),
    );
    check(typeof result.packageBase64 === "string", "detached bytes missing");
    const bytes = Uint8Array.from(
      atob(result.packageBase64),
      (character) => character.charCodeAt(0),
    );
    check(
      bytes.length === pkg.bytes.length &&
        bytes.every((byte, index) => byte === pkg.bytes[index]),
      "download bytes differ",
    );
    const after = await waitForHubPanelMenuM3eFixtureIdle(fixture, 5000);
    const route = after.observations.at(-1);
    check(
      after.requests === before.requests + 1 &&
        after.observations.length === before.observations.length + 1 &&
        !after.observationOverflow && after.activeConnections === 0 &&
        after.activeStreams === 0 &&
        after.cancellations === before.cancellations &&
        after.redirectTargets === before.redirectTargets &&
        route?.origin === "trusted" && route.mode === "valid-streaming" &&
        route.routePath ===
          `/floorp-feature-packages/v1/hub-panel-menu/${pkg.packageSha256}.json` &&
        route.packageSha256 === pkg.packageSha256 &&
        route.featureId === "hub-panel-menu" && !route.cookiePresent &&
        !route.authorizationPresent && !route.referrerPresent &&
        !route.seedCookieMarkerMatch && !route.acquisitionCookieMarkerMatch &&
        !route.authorizationMarkerMatch && !route.referrerMarkerMatch,
      "HTTPS route/privacy/idle differs",
    );
    return result.packageBase64;
  }
  async function stage(
    packageBase64: string,
    pkg: HubPanelMenuM3eSignedPackage,
  ) {
    const metadata = record(
      await executeHubPanelMenuM3eBrowserScript(client, {
        kind: "host-operation",
        payload: { ...payload, operation: "stage", packageBase64 },
      }),
    );
    const source = HUB_PANEL_MENU_FIXTURES[
      pkg.version as keyof typeof HUB_PANEL_MENU_FIXTURES
    ];
    check(
      metadata.packageSha256 === pkg.packageSha256 &&
        metadata.version === pkg.version &&
        metadata.releaseSequence === pkg.releaseSequence &&
        metadata.sourceSha256 === source.digest,
      "staged metadata differs",
    );
  }
  async function stale(tag: string) {
    const before = await snapshot();
    const result = await invoke("stale", { tag });
    check(
      same(before, await snapshot()),
      "stale handler mutated current feature/UI",
    );
    return result;
  }
  async function transact(
    operation: string,
    selected: HubPanelMenuM3eSignedPackage,
    highWater: HubPanelMenuM3eSignedPackage,
    enabled: boolean,
    target?: HubPanelMenuM3eSignedPackage,
    status = "succeeded",
    health: "verified" | "unverified" = "verified",
  ) {
    const before = await snapshot();
    const expectedRevision = record(before.diagnostics).revision;
    const txId = crypto.randomUUID();
    const input = {
      ...payload,
      operation,
      txId,
      expectedRevision,
      packageSha256: target?.packageSha256,
    };
    const outcome = record(
      await executeHubPanelMenuM3eBrowserScript(client, {
        kind: "host-operation",
        payload: input,
      }),
    );
    const request = record(outcome.request);
    check(
      outcome.txId === txId && outcome.status === status &&
        outcome.selectedVersion === selected.packageSha256 &&
        request.featureId === "hub-panel-menu" && request.op === operation &&
        request.expectedRevision === expectedRevision &&
        request.target === (target?.packageSha256 ?? null),
      "terminal outcome differs",
    );
    const query = await executeHubPanelMenuM3eBrowserScript(client, {
      kind: "host-operation",
      payload: { ...payload, operation: "query", txId },
    });
    check(same(outcome, query), "query outcome differs");
    current = await snapshot();
    stateIs(current, selected, highWater, enabled, health);
    const journal = record(current.journal);
    const lifecycle = record(journal.lifecycle);
    check(
      same(lifecycle.lastOutcome, outcome) &&
        Number(record(current.diagnostics).revision) > Number(expectedRevision),
      "journal terminal/revision differs",
    );
    check(same(journal.packages, current.metadata), "durable catalog differs");
    for (const value of current.metadata as unknown[]) {
      const item = record(value);
      const pkg = bundle.packages[String(item.version)];
      check(
        pkg && item.packageSha256 === pkg.packageSha256 &&
          item.releaseSequence === pkg.releaseSequence,
        "exact envelope catalog differs",
      );
    }
    return outcome;
  }
  await runCase("windows-private", async () => {
    const state = record(await invoke("prepare-windows"));
    const registry = record(state.registry);
    check(
      registry.connections === 2 && registry.staged === false &&
        registry.published === false && registry.enabled === false &&
        state.canonicalRootExists === false && state.journal === null,
      "manual multi-window state differs",
    );
    check(
      Array.isArray(state.ui) && state.ui.length === 3,
      "manual window count differs",
    );
    for (const item of state.ui.map(record)) {
      check(
        item.private === true
          ? item.count === 1 && item.label === label && item.enabled === true &&
            item.afterSettings === true && item.privateNodeUnchanged === true
          : item.count === 0,
        "manual/private UI differs",
      );
    }
    return evidence(state);
  });
  await runCase("stage-install-disabled", async () => {
    await invoke("observe-generations");
    await executeHubPanelMenuM3eBrowserScript(client, {
      kind: "create-host",
      payload: { ...payload, policy: bundle.policy },
    });
    const before = await snapshot();
    stateIs(before, null, null, false, "unverified");
    await stage(context.v1DownloadedBase64, v1);
    const staged = await snapshot();
    stateIs(staged, null, null, false, "unverified");
    check(
      same(before.journal, staged.journal) &&
        before.evaluationCount === staged.evaluationCount &&
        same(before.ui, staged.ui) && Array.isArray(staged.metadata) &&
        staged.metadata.length === 1,
      "stage executed or mutated journal/UI",
    );
    const cache = record(staged.cache);
    check(
      cache.exists === true && Array.isArray(cache.entries) &&
        cache.entries.map(record).some((entry) =>
          entry.type === "file" && entry.sha256 === v1.packageSha256
        ),
      "stage cache bytes differ",
    );
    const outcome = await transact(
      "install",
      v1,
      v1,
      false,
      v1,
      "succeeded",
      "unverified",
    );
    check(
      current.evaluationCount === before.evaluationCount,
      "disabled install evaluated source",
    );
    return evidence(current, {
      outcome: JSON.stringify(outcome),
      stagedWithoutEvaluation: true,
    });
  });
  await runCase("enabled-command-opens-hub", async () => {
    const outcome = await transact("enable", v1, v1, true);
    const command = record(await invoke("command"));
    return evidence(current, {
      outcome: JSON.stringify(outcome),
      command: JSON.stringify(command),
    });
  });
  await runCase("active-acquisition-inert", async () => {
    const before = await capture();
    bytesV2 = await download(v2);
    const after = await capture();
    check(same(before, after), "active acquisition changed feature snapshot");
    return {
      exactDetachedBytes: true,
      featureSnapshotsUnchanged: true,
      httpsPackageSha256: v2.packageSha256,
      httpsRequestCount: fixture.snapshot().requests,
      before: JSON.stringify(before),
      after: JSON.stringify(after),
    };
  });
  await runCase("update-v2-generation-fencing", async () => {
    await invoke("retain", { tag: "v1" });
    const before = await snapshot();
    await stage(bytesV2, v2);
    const outcome = await transact("update", v2, v2, true, v2);
    assertHubPanelMenuM3eReplacement(before, current);
    const staleResult = await stale("v1");
    return evidence(current, {
      outcome: JSON.stringify(outcome),
      stale: JSON.stringify(staleResult),
    });
  });
  await runCase("new-window-v2-only", async () => {
    const before = current;
    current = record(await invoke("new-window"));
    normalWindows = 3;
    stateIs(current, v2, v2, true);
    assertHubPanelMenuM3eLateDelivery(current);
    check(
      Array.isArray(before.ui) && Array.isArray(current.ui) &&
        before.ui.map(record).every((item) =>
          (current.ui as unknown[]).map(record).find((next) =>
            next.role === item.role
          )?.nodeId === item.nodeId
        ),
      "new window replaced existing nodes",
    );
    return evidence(current);
  });
  await runCase("rollback-retains-high-water", async () => {
    const outcome = await transact("rollback", v1, v2, true);
    await stage(context.v1DownloadedBase64, v1);
    const before = await snapshot();
    const rejection = await invoke("reject-operation", {
      operation: "update",
      packageSha256: v1.packageSha256,
      expectedRevision: record(before.diagnostics).revision,
      txId: crypto.randomUUID(),
      expectedCode: "sequence-too-low",
    });
    const after = await snapshot();
    check(same(before, after), "rejected admission mutated state");
    return evidence(after, {
      outcome: JSON.stringify(outcome),
      verificationStageAllowed: true,
      lowerSequenceAdmission: JSON.stringify(rejection),
    });
  });
  await runHubPanelMenuM3eFocus(context);
  await runCase("broken-v3-restores-active-generation", async () => {
    const detached = await download(broken);
    await stage(detached, broken);
    const before = await snapshot();
    const outcome = await transact(
      "update",
      v1,
      broken,
      true,
      broken,
      "failed_restored",
    );
    assertHubPanelMenuM3eReplacement(before, current);
    return evidence(current, {
      outcome: JSON.stringify(outcome),
      restoredVersion: v1.version,
    });
  });
  await runCase("disable-reenable-close", async () => {
    await invoke("retain", { tag: "before-disable" });
    const disabled = await transact("disable", v1, broken, false);
    const disabledStale = await stale("before-disable");
    const enabled = await transact("enable", v1, broken, true);
    await invoke("retain", { tag: "before-close" });
    const cleanup = record(
      await executeHubPanelMenuM3eBrowserScript(client, {
        kind: "close-host",
        payload,
      }),
    );
    check(cleanup.proven === true, "host cleanup unproven");
    const closed = await snapshot();
    stateIs(closed, v1, broken, true, "verified", true);
    const closedStale = await stale("before-close");
    const rejected = await invoke("reject-operation", {
      operation: "stage",
      packageBase64: context.v1DownloadedBase64,
      expectedCode: "operation-failed",
    });
    check(same(closed, await snapshot()), "post-close stage mutated state");
    await invoke("close-windows");
    return evidence(closed, {
      disableOutcome: JSON.stringify(disabled),
      reenableOutcome: JSON.stringify(enabled),
      disabledStale: JSON.stringify(disabledStale),
      closedStale: JSON.stringify(closedStale),
      cleanup: JSON.stringify(cleanup),
      postCloseStage: JSON.stringify(rejected),
    });
  });
}
