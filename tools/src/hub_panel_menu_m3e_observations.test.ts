// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert";
import type { MarionetteClient } from "./browser_connector.ts";
import { assertInertnessSnapshotsEqual } from "./hub_panel_menu_m3e_acceptance.ts";
import {
  assertHubPanelMenuM3eStartupObservation,
  bindHubPanelMenuM3eBrowserSandbox,
  closeHubPanelMenuM3eBrowser,
  executeHubPanelMenuM3eBrowserScript,
  HubPanelMenuM3eConnectionUnusableError,
  hubPanelMenuM3eProtocolError,
  runHubPanelMenuM3eGatedOperation,
  sendHubPanelMenuM3eBrowserCommand,
} from "./hub_panel_menu_m3e_observations.ts";

Deno.test("non-input protocol diagnostics retain bounded original category/message without changing public failures", async () => {
  assertEquals(hubPanelMenuM3eProtocolError(null), {
    category: "unknown",
    message: "unknown",
  });
  assertEquals(hubPanelMenuM3eProtocolError(new Error("local")), {
    category: "Error",
    message: "local",
  });
  const error = new Error("public", {
    cause: { name: "x".repeat(301), message: "y".repeat(301) },
  });
  assertEquals(hubPanelMenuM3eProtocolError(error), {
    category: "x".repeat(300),
    message: "y".repeat(300),
  });
  const root = await Deno.makeTempDir();
  try {
    let aborted = false;
    const client = {
      sendWithTimeout: () => Promise.reject(error),
      abort() {
        aborted = true;
      },
    } as unknown as MarionetteClient;
    const path = `${root}/protocol.json`;
    bindHubPanelMenuM3eBrowserSandbox(
      client,
      "00000000-0000-4000-8000-000000000000",
      "C",
      path,
    );
    await assertRejects(
      () =>
        executeHubPanelMenuM3eBrowserScript(client, { kind: "profile-path" }),
      HubPanelMenuM3eConnectionUnusableError,
    );
    const diagnostic = JSON.parse(await Deno.readTextFile(path));
    assertEquals(diagnostic.diagnosticsOnly, true);
    assertEquals(diagnostic.command, "WebDriver:ExecuteAsyncScript");
    assertEquals(diagnostic.category, "x".repeat(300));
    assertEquals(diagnostic.message, "y".repeat(300));
    assert(aborted);
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("input diagnostics receive the original Gecko failure while the public connection fails closed", async () => {
  const cause = { name: "invalid argument", message: "Gecko input failure" };
  const original = new Error("protocol failure", { cause });
  let retained: unknown;
  let aborted = false;
  let sends = 0;
  const client = {
    sendWithTimeout() {
      sends++;
      return Promise.reject(original);
    },
    abort() {
      aborted = true;
    },
  } as unknown as MarionetteClient;
  await assertRejects(
    () =>
      sendHubPanelMenuM3eBrowserCommand(
        client,
        "WebDriver:PerformActions",
        {},
        100,
        (error) => {
          retained = error;
          return Promise.resolve();
        },
      ),
    HubPanelMenuM3eConnectionUnusableError,
  );
  assert(retained === original);
  assertEquals((retained as Error).cause, cause);
  assert(aborted);
  await assertRejects(
    () =>
      sendHubPanelMenuM3eBrowserCommand(client, "WebDriver:ReleaseActions", {}),
    HubPanelMenuM3eConnectionUnusableError,
  );
  assertEquals(sends, 1);
});

Deno.test("M3e connector retains a task/row sandbox and clears it before closing", async () => {
  const calls: Record<string, unknown>[] = [];
  let closed = false;
  const client = {
    sendWithTimeout(_command: string, params: Record<string, unknown>) {
      calls.push(params);
      return Promise.resolve({ value: { cleared: true } });
    },
    close() {
      closed = true;
      return Promise.resolve();
    },
  } as unknown as MarionetteClient;
  await assertRejects(
    () => executeHubPanelMenuM3eBrowserScript(client, { kind: "profile-path" }),
    Error,
    "not bound",
  );
  assertThrows(() => bindHubPanelMenuM3eBrowserSandbox(client, "invalid", "A"));
  bindHubPanelMenuM3eBrowserSandbox(
    client,
    "00000000-0000-4000-8000-000000000000",
    "A",
  );
  assertThrows(() =>
    bindHubPanelMenuM3eBrowserSandbox(
      client,
      "00000000-0000-4000-8000-000000000000",
      "B",
    )
  );
  await executeHubPanelMenuM3eBrowserScript(client, { kind: "profile-path" });
  await executeHubPanelMenuM3eBrowserScript(client, { kind: "policy-binding" });
  await closeHubPanelMenuM3eBrowser(client);
  assertEquals(calls.length, 3);
  assert(
    calls.every((params) =>
      params.sandbox ===
        "hub-panel-menu-m3e-00000000-0000-4000-8000-000000000000-A" &&
      params.newSandbox === false
    ),
  );
  assert(String(calls[2].script).includes('"kind":"clear-task-state"'));
  assert(closed);
  await assertRejects(
    () => executeHubPanelMenuM3eBrowserScript(client, { kind: "profile-path" }),
    Error,
    "not bound",
  );
});

Deno.test("M3e connector still closes when state clearing fails", async () => {
  let closed = false;
  const client = {
    abort() {
      closed = true;
    },
    sendWithTimeout() {
      return Promise.reject(new Error("controlled failure"));
    },
    close() {
      closed = true;
      return Promise.resolve();
    },
  } as unknown as MarionetteClient;
  bindHubPanelMenuM3eBrowserSandbox(
    client,
    "00000000-0000-4000-8000-000000000000",
    "C",
  );
  await assertRejects(
    () => closeHubPanelMenuM3eBrowser(client),
    Error,
    "connection is unusable",
  );
  assert(closed);
  await assertRejects(
    () => executeHubPanelMenuM3eBrowserScript(client, { kind: "profile-path" }),
    Error,
    "connection is unusable",
  );
});

function startupObservation(
  row: {
    readonly id: string;
    readonly featureEnabled: boolean;
    readonly manualHostEnabled: boolean;
    readonly selection: "legacy" | "pinned" | "manual";
    readonly connections: number;
    readonly buttonCount: number;
  },
): Record<string, unknown> {
  const hasButton = row.buttonCount === 1;
  const stateSha256 = "0".repeat(64);
  return {
    startupMode: "dev",
    profileDir: `/owned/profile/${row.id}`,
    featureEnabled: row.featureEnabled,
    manualHostEnabled: row.manualHostEnabled,
    loaderStatus: "loaded",
    startupError: "",
    mainWindowCandidate: row.featureEnabled,
    pinnedRootExists: row.selection === "pinned",
    pinnedRootSnapshot: {
      exists: row.selection === "pinned",
      entries: row.selection === "pinned"
        ? [{ path: "state.json", type: "file", size: 1, sha256: stateSha256 }]
        : [],
    },
    pinnedState: row.selection === "pinned"
      ? {
        schemaVersion: 2,
        featureId: "hub-panel-menu",
        selectedVersion: "hub-menu-v1",
        selectedDigest:
          "c5f95c9be1dba7668e271f1a531a41897b6a2e3abcc2fb871ad3eb4c55184a44",
        expectedDigest:
          "c5f95c9be1dba7668e271f1a531a41897b6a2e3abcc2fb871ad3eb4c55184a44",
        desiredEnabled: true,
        selectedHealth: "verified",
        pending: null,
        blockedReason: null,
        lastOutcome: { status: "succeeded", selectedVersion: "hub-menu-v1" },
        snapshotSha256: stateSha256,
        snapshotSize: 1,
      }
      : null,
    selection: row.selection,
    menuReady: true,
    windows: [{
      windowType: "navigator:browser",
      private: false,
      delayedStartupFinished: true,
    }],
    sharedRegistry: {
      connections: row.connections,
      published: row.selection === "pinned",
      enabled: row.selection === "pinned",
      staged: false,
      pendingAcknowledgements: 0,
    },
    ui: [{
      buttonPresent: hasButton,
      buttonCount: row.buttonCount,
      buttonId: hasButton ? "appMenu-floorp-hub-button" : null,
      buttonEnabled: hasButton ? true : null,
      buttonLabel: hasButton ? "Hub" : null,
      immediatelyAfterSettings: hasButton,
      nodeId: hasButton ? "node-1" : null,
    }],
  };
}

Deno.test("startup observations accept valid pinned, legacy and manual rows", () => {
  const rows = [
    {
      expected: {
        profilePath: "/owned/profile/A",
        featureEnabled: true,
        manualHostEnabled: false,
        pinnedRootExists: true,
        selection: "pinned" as const,
      },
      observation: startupObservation({
        id: "A",
        featureEnabled: true,
        manualHostEnabled: false,
        selection: "pinned",
        connections: 1,
        buttonCount: 1,
      }),
    },
    {
      expected: {
        profilePath: "/owned/profile/B",
        featureEnabled: false,
        manualHostEnabled: true,
        pinnedRootExists: false,
        selection: "legacy" as const,
      },
      observation: startupObservation({
        id: "B",
        featureEnabled: false,
        manualHostEnabled: true,
        selection: "legacy",
        connections: 0,
        buttonCount: 1,
      }),
    },
    {
      expected: {
        profilePath: "/owned/profile/C",
        featureEnabled: true,
        manualHostEnabled: true,
        pinnedRootExists: false,
        selection: "manual" as const,
      },
      observation: startupObservation({
        id: "C",
        featureEnabled: true,
        manualHostEnabled: true,
        selection: "manual",
        connections: 1,
        buttonCount: 0,
      }),
    },
  ] as const;
  for (const row of rows) {
    assertHubPanelMenuM3eStartupObservation(row.observation, row.expected);
  }
});

Deno.test("startup observations reject malformed, incomplete and contradictory evidence", () => {
  const expected = {
    profilePath: "/owned/profile/A",
    featureEnabled: true,
    manualHostEnabled: false,
    pinnedRootExists: true,
    selection: "pinned" as const,
  };
  const pinned = startupObservation({
    id: "A",
    featureEnabled: true,
    manualHostEnabled: false,
    selection: "pinned",
    connections: 1,
    buttonCount: 1,
  });
  const malformed = structuredClone(pinned);
  malformed.startupMode = 17;
  assertThrows(() =>
    assertHubPanelMenuM3eStartupObservation(malformed, expected)
  );
  const incomplete = structuredClone(pinned);
  incomplete.sharedRegistry = { published: true, enabled: true };
  assertThrows(() =>
    assertHubPanelMenuM3eStartupObservation(incomplete, expected)
  );
  const contradictory = structuredClone(pinned);
  contradictory.selection = "manual";
  assertThrows(() =>
    assertHubPanelMenuM3eStartupObservation(contradictory, expected)
  );

  const blankLabel = structuredClone(pinned);
  (blankLabel.ui as Array<Record<string, unknown>>)[0].buttonLabel = "  ";
  assertThrows(() =>
    assertHubPanelMenuM3eStartupObservation(blankLabel, expected)
  );
  const duplicateButton = structuredClone(pinned);
  (duplicateButton.ui as Array<Record<string, unknown>>)[0].buttonCount = 2;
  assertThrows(() =>
    assertHubPanelMenuM3eStartupObservation(duplicateButton, expected)
  );
  const disabledButton = structuredClone(pinned);
  (disabledButton.ui as Array<Record<string, unknown>>)[0].buttonEnabled =
    false;
  assertThrows(() =>
    assertHubPanelMenuM3eStartupObservation(disabledButton, expected)
  );
  const legacy = startupObservation({
    id: "B",
    featureEnabled: false,
    manualHostEnabled: true,
    selection: "legacy",
    connections: 0,
    buttonCount: 1,
  });
  (legacy.sharedRegistry as Record<string, unknown>).connections = 1;
  assertThrows(() =>
    assertHubPanelMenuM3eStartupObservation(legacy, {
      profilePath: "/owned/profile/B",
      featureEnabled: false,
      manualHostEnabled: true,
      pinnedRootExists: false,
      selection: "legacy",
    })
  );
  const manual = startupObservation({
    id: "C",
    featureEnabled: true,
    manualHostEnabled: true,
    selection: "manual",
    connections: 0,
    buttonCount: 0,
  });
  assertThrows(() =>
    assertHubPanelMenuM3eStartupObservation(manual, {
      profilePath: "/owned/profile/C",
      featureEnabled: true,
      manualHostEnabled: true,
      pinnedRootExists: false,
      selection: "manual",
    })
  );
});

Deno.test("empty acquisition compares exact journal, cache, root and UI ownership snapshots", () => {
  const before: Record<string, unknown> = {
    profileDir: "/owned/profile/C",
    evaluationCount: 0,
    sharedRegistry: { connections: 1, published: false, enabled: false },
    hostDiagnostics: null,
    hostMetadata: null,
    taskStateToken: "task:C:token",
    canonicalRootSnapshot: { exists: false, entries: [] },
    journalRootSnapshot: { exists: false, entries: [] },
    packageCacheSnapshot: { exists: false, entries: [] },
    ui: [{ private: false, buttonPresent: false, nodeId: null }],
  };
  assertInertnessSnapshotsEqual(before, structuredClone(before));
  const changedJournal = structuredClone(before);
  changedJournal.journalRootSnapshot = {
    exists: true,
    entries: [{
      path: "state.json",
      type: "file",
      size: 1,
      sha256: "0".repeat(64),
    }],
  };
  assertThrows(() => assertInertnessSnapshotsEqual(before, changedJournal));
});

Deno.test("gated operation releases and awaits after timeout or early settlement", async () => {
  const calls: string[] = [];
  const input = {
    testId: "task",
    txId: "tx",
    operation: "update",
    target: "pkg",
  };
  await assertRejects(() =>
    runHubPanelMenuM3eGatedOperation(
      async (script) => {
        await Promise.resolve();
        calls.push(script.kind);
        if (script.kind === "arm-barrier") return { armed: true, txId: "tx" };
        if (script.kind === "start-gated-operation") {
          return {
            barrierReached: false,
            terminal: { rejected: true },
          };
        }
        if (script.kind === "await-operation") {
          return {
            terminal: { rejected: true },
          };
        }
        return { released: true };
      },
      input,
      async () => {
        await Promise.resolve();
        throw new Error("must not enter the gate");
      },
      50,
    )
  );
  assert(
    calls.includes("release-barrier") && calls.includes("await-operation"),
  );

  const timeoutCalls: string[] = [];
  await assertRejects(() =>
    runHubPanelMenuM3eGatedOperation(
      async (script) => {
        await Promise.resolve();
        timeoutCalls.push(script.kind);
        if (script.kind === "arm-barrier") return { armed: true, txId: "tx" };
        if (script.kind === "start-gated-operation") {
          return { barrierReached: false, timedOut: true };
        }
        if (script.kind === "await-operation") {
          return {
            terminal: { settled: true },
          };
        }
        return { released: true };
      },
      input,
      async () => {
        await Promise.resolve();
      },
      5,
    )
  );
  assert(
    timeoutCalls.includes("release-barrier") &&
      timeoutCalls.includes("await-operation"),
  );
});

Deno.test("gated controller never sends cleanup commands after connection safety failure", async () => {
  const calls: string[] = [];
  await assertRejects(() =>
    runHubPanelMenuM3eGatedOperation(
      async (input) => {
        await Promise.resolve();
        calls.push(input.kind);
        if (input.kind === "arm-barrier") return { armed: true, txId: "tx" };
        throw new HubPanelMenuM3eConnectionUnusableError();
      },
      { testId: "task", txId: "tx", operation: "update" },
      async () => {},
    )
  );
  assertEquals(calls, ["arm-barrier", "start-gated-operation"]);
});
