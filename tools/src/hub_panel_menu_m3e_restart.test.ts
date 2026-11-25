// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";
import {
  assertHubPanelMenuM3eRestartJournal,
  resolveHubPanelMenuM3eSelectedCachePath,
} from "./hub_panel_menu_m3e_restart.ts";

Deno.test("restart mutation resolves only a unique exact selected digest beneath the owned cache", () => {
  const digest = "a".repeat(64);
  const id = "12345678-1234-1234-1234-123456789abc";
  const entry = { type: "file", path: `${digest}.json`, sha256: digest };
  const snapshot = { cache: { entries: [entry] } };
  assertEquals(
    resolveHubPanelMenuM3eSelectedCachePath("/owned/C", id, snapshot, digest),
    `/owned/C/floorp-signed-package-cache-v1/tests/${id}/hub-panel-menu/${digest}.json`,
  );
  for (
    const path of [
      "../outside",
      "/outside",
      "x/../../outside",
      "x\\..\\outside",
      "",
    ]
  ) {
    assertThrows(() =>
      resolveHubPanelMenuM3eSelectedCachePath("/owned/C", id, {
        cache: { entries: [{ ...entry, path }] },
      }, digest)
    );
  }
  for (const entries of [[], [entry, entry], [{ ...entry, sha256: "other" }]]) {
    assertThrows(() =>
      resolveHubPanelMenuM3eSelectedCachePath("/owned/C", id, {
        cache: { entries },
      }, digest)
    );
  }
  assertThrows(() =>
    resolveHubPanelMenuM3eSelectedCachePath("relative", id, snapshot, digest)
  );
  assertThrows(() =>
    resolveHubPanelMenuM3eSelectedCachePath(
      "/owned/C",
      "invalid",
      snapshot,
      digest,
    )
  );
});

Deno.test("disabled fresh-process reopen requires unchanged durable state and no evaluation", () => {
  const lifecycle = { desiredEnabled: false, pending: null, stateRevision: 12 };
  const state = {
    metadata: [],
    evaluationCount: 0,
    journal: { packages: [], highWater: { releaseSequence: 2 }, lifecycle },
    diagnostics: { desiredEnabled: false, revision: 12 },
  };
  assertHubPanelMenuM3eRestartJournal(state, state, false, false);
  for (
    const after of [
      { ...state, evaluationCount: 1 },
      { ...state, metadata: ["new"] },
      { ...state, journal: { ...state.journal, highWater: null } },
      null,
      {},
    ]
  ) {
    assertThrows(() =>
      assertHubPanelMenuM3eRestartJournal(state, after, false, false)
    );
  }
});

Deno.test("fresh enabled recovery requires a durable startup terminal and exactly one evaluation", () => {
  const lifecycle = { desiredEnabled: true, pending: null, stateRevision: 8 };
  const before = {
    metadata: [],
    journal: { packages: [], highWater: { releaseSequence: 2 }, lifecycle },
  };
  const after = {
    ...before,
    evaluationCount: 1,
    diagnostics: { desiredEnabled: true, revision: 14 },
    journal: {
      ...before.journal,
      lifecycle: {
        ...lifecycle,
        stateRevision: 14,
        lastOutcome: {
          status: "succeeded",
          request: { op: "startup_recovery" },
        },
      },
    },
  };
  assertHubPanelMenuM3eRestartJournal(before, after, true, false);
  for (const evaluationCount of [0, 2]) {
    assertThrows(() =>
      assertHubPanelMenuM3eRestartJournal(
        before,
        { ...after, evaluationCount },
        true,
        false,
      )
    );
  }
  assertThrows(() =>
    assertHubPanelMenuM3eRestartJournal(
      before,
      { ...after, journal: before.journal },
      true,
      false,
    )
  );
});

Deno.test("quarantined recovery retains historical health but cannot evaluate or publish", () => {
  const lifecycle = {
    desiredEnabled: true,
    pending: null,
    stateRevision: 8,
    selectedDigest: "selected",
    selectedHealth: "verified",
    selectedHealthBuild: { hostBuildId: "m0-schema-2" },
    rollbackVersion: null,
    rollbackDigest: null,
  };
  const before = {
    metadata: [],
    journal: { packages: [], highWater: { releaseSequence: 1 }, lifecycle },
  };
  const after = {
    ...before,
    evaluationCount: 0,
    diagnostics: {
      desiredEnabled: true,
      revision: 10,
      blocked: true,
      blockCategory: "lifecycle-blocked",
    },
    registryOwner: null,
    registry: {
      connections: 2,
      published: false,
      enabled: false,
      staged: false,
      pendingAcknowledgements: 0,
    },
    ui: [{ private: false, count: 0 }, { private: false, count: 0 }, {
      private: true,
      count: 1,
      privateNodeUnchanged: true,
    }],
    journal: {
      ...before.journal,
      lifecycle: {
        ...lifecycle,
        stateRevision: 10,
        blockedReason: "recovery-preflight-rejected",
        lastOutcome: {
          status: "failed_quarantined",
          request: { op: "startup_recovery" },
        },
      },
    },
  };
  assertHubPanelMenuM3eRestartJournal(before, after, true, true);
  for (
    const bad of [
      { ...after, evaluationCount: 1 },
      { ...after, registry: { ...after.registry, published: true } },
      { ...after, ui: [] },
      { ...after, diagnostics: { ...after.diagnostics, blocked: false } },
    ]
  ) {
    assertThrows(() =>
      assertHubPanelMenuM3eRestartJournal(before, bad, true, true)
    );
  }
});
