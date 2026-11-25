// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertThrows } from "@std/assert";
import { HUB_PANEL_MENU_M3E_LIFECYCLE_SCRIPT } from "./hub_panel_menu_m3e_lifecycle_script.ts";
import {
  assertHubPanelMenuM3eReplacement,
  assertHubPanelMenuM3eSignedState,
} from "./hub_panel_menu_m3e_lifecycle.ts";
import { HUB_PANEL_MENU_FIXTURES } from "../../browser-features/modules/modules/feature-host/HubPanelMenuFixtures.sys.mts";

Deno.test("signed lifecycle oracle rejects wrong identity, journal, high-water, UI and private ownership", () => {
  const pkg = {
    version: "hub-menu-v1",
    releaseSequence: 1,
    packageSha256: "a".repeat(64),
    byteLength: 0,
    bytes: new Uint8Array(),
  };
  const metadata = {
    version: pkg.version,
    packageSha256: pkg.packageSha256,
    sourceSha256: HUB_PANEL_MENU_FIXTURES["hub-menu-v1"].digest,
    releaseSequence: 1,
  };
  const state = {
    canonicalRootExists: false,
    registryOwner: {
      featureId: "hub-panel-menu",
      hostEpoch: "host",
      generation: 1,
      leaseId: "lease-1",
    },
    diagnostics: {
      blocked: false,
      blockCategory: null,
      desiredEnabled: true,
      selectedHealth: "verified",
      revision: 6,
      selectedPackageSha256: pkg.packageSha256,
      selected: metadata,
    },
    registry: {
      connections: 2,
      staged: true,
      published: true,
      enabled: true,
      pendingAcknowledgements: 0,
    },
    journal: {
      schemaVersion: 1,
      featureId: "hub-panel-menu",
      packages: [metadata],
      highWater: { releaseSequence: 1, packageSha256: pkg.packageSha256 },
      lifecycle: {
        schemaVersion: 2,
        featureId: "hub-panel-menu",
        blockedReason: null,
        pending: null,
        desiredEnabled: true,
        selectedHealth: "verified",
        stateRevision: 6,
        selectedVersion: pkg.packageSha256,
        selectedDigest: pkg.packageSha256,
        selectedHealthBuild: { hostBuildId: "m0-schema-2" },
      },
    },
    metadata: [metadata],
    ui: [false, false, true].map((privateWindow, index) => ({
      role: String(index),
      private: privateWindow,
      nodeId: index + 1,
      count: 1,
      enabled: true,
      label: "Floorp Hub",
      afterSettings: true,
      privateNodeUnchanged: true,
    })),
  };
  const expected = {
    selected: pkg,
    highWater: pkg,
    enabled: true,
    health: "verified" as const,
    normalWindows: 2,
    label: "Floorp Hub",
  };
  assertHubPanelMenuM3eSignedState(state, expected);
  const absentUi = state.ui.map((item) =>
    item.private ? item : {
      ...item,
      count: 0,
      nodeId: null,
      enabled: null,
      label: null,
      afterSettings: false,
    }
  );
  const inactiveRegistry = {
    ...state.registry,
    staged: false,
    published: false,
    enabled: false,
  };
  const closed = {
    ...state,
    registryOwner: null,
    registry: inactiveRegistry,
    ui: absentUi,
  };
  assertHubPanelMenuM3eSignedState(closed, { ...expected, closed: true });
  const disabled = {
    ...closed,
    diagnostics: { ...state.diagnostics, desiredEnabled: false },
    journal: {
      ...state.journal,
      lifecycle: { ...state.journal.lifecycle, desiredEnabled: false },
    },
  };
  assertHubPanelMenuM3eSignedState(disabled, { ...expected, enabled: false });
  assertThrows(() =>
    assertHubPanelMenuM3eSignedState({ ...disabled, ui: state.ui }, {
      ...expected,
      enabled: false,
    })
  );
  const staged = {
    ...disabled,
    diagnostics: {
      ...disabled.diagnostics,
      selected: null,
      selectedPackageSha256: null,
      selectedHealth: "unverified",
    },
    journal: {
      ...disabled.journal,
      highWater: null,
      packages: [],
      lifecycle: {
        ...disabled.journal.lifecycle,
        selectedVersion: null,
        selectedDigest: null,
        selectedHealth: "unverified",
        selectedHealthBuild: null,
      },
    },
  };
  assertHubPanelMenuM3eSignedState(staged, {
    ...expected,
    selected: null,
    highWater: null,
    health: "unverified",
    enabled: false,
  });
  for (
    const mutate of [
      (value: typeof state) => {
        value.journal.highWater.releaseSequence = 0;
      },
      (value: typeof state) => {
        value.journal.lifecycle.selectedDigest = "b".repeat(64);
      },
      (value: typeof state) => {
        value.diagnostics.revision++;
      },
      (value: typeof state) => {
        value.ui[0].count = 2;
      },
      (value: typeof state) => {
        value.ui[2].privateNodeUnchanged = false;
      },
      (value: typeof state) => {
        value.metadata[0].sourceSha256 = "bad";
      },
    ]
  ) {
    const changed = structuredClone(state);
    mutate(changed);
    assertThrows(() => assertHubPanelMenuM3eSignedState(changed, expected));
  }
  for (
    const malformed of [null, {}, { ...state, ui: [] }, {
      ...state,
      journal: null,
    }]
  ) assertThrows(() => assertHubPanelMenuM3eSignedState(malformed, expected));
  const next = structuredClone(state);
  next.registryOwner.generation++;
  next.registryOwner.leaseId = "lease-2";
  next.ui[0].nodeId += 10;
  next.ui[1].nodeId += 10;
  assertHubPanelMenuM3eReplacement(state, next);
  assertThrows(() => assertHubPanelMenuM3eReplacement(state, state));
  next.ui[2].nodeId++;
  assertThrows(() => assertHubPanelMenuM3eReplacement(state, next));
});

Deno.test("generated closed-facade rejection preserves finite actual code and fails on success or wrong errors", async () => {
  const run = (host: unknown) =>
    new Function(
      "input",
      "getTaskState",
      "ChromeUtils",
      `const REGISTRY_URI = "registry"; const FEATURE_HOST_URI = "host"; return (async () => { ${HUB_PANEL_MENU_M3E_LIFECYCLE_SCRIPT} })();`,
    )(
      {
        kind: "lifecycle",
        action: "reject-operation",
        operation: "stage",
        packageBase64: "",
        expectedCode: "operation-failed",
      },
      () => ({ hosts: {}, closedHosts: { "hub-panel-menu": host } }),
      { importESModule: () => ({ getSharedAppMenuRegistry: () => ({}) }) },
    );
  const rejected = {
    stage: () =>
      Promise.reject(
        Object.assign(new Error("private marker"), {
          code: "operation-failed",
        }),
      ),
  };
  assertEquals(await run(rejected), {
    rejected: true,
    code: "operation-failed",
  });
  await assertRejects(() => run({ stage: () => Promise.resolve() }));
  await assertRejects(() =>
    run({ stage: () => Promise.reject(new Error("private marker")) })
  );
});

Deno.test("generated generation observer delegates exact arguments/result and restores the registry method", async () => {
  const task: Record<string, unknown> = { hosts: {}, closedHosts: {} };
  const lease = {
    owner: { featureId: "hub-panel-menu", hostEpoch: "host", generation: 1 },
    id: "lease",
  };
  const argumentsSeen: unknown[][] = [];
  const registry = {
    beginGeneration: (...args: unknown[]) => {
      argumentsSeen.push(args);
      return lease;
    },
  };
  const original = registry.beginGeneration;
  const result = await new Function(
    "input",
    "getTaskState",
    "ChromeUtils",
    `const REGISTRY_URI = "registry"; const FEATURE_HOST_URI = "host"; return (async () => { ${HUB_PANEL_MENU_M3E_LIFECYCLE_SCRIPT} })();`,
  )(
    { kind: "lifecycle", action: "observe-generations" },
    () => task,
    { importESModule: () => ({ getSharedAppMenuRegistry: () => registry }) },
  );
  assertEquals(result, { observing: true });
  const callback = () => {};
  assertEquals(registry.beginGeneration(lease.owner, callback) === lease, true);
  assertEquals(argumentsSeen, [[lease.owner, callback]]);
  assertEquals(task.registryOwners, [{ ...lease.owner, leaseId: "lease" }]);
  (task.registryObserver as { restore: () => void }).restore();
  assertEquals(registry.beginGeneration === original, true);
});
