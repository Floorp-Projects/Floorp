// SPDX-License-Identifier: MPL-2.0

import { MarionetteClient } from "./browser_connector.ts";
import {
  buildHubPanelMenuM3eBrowserScript,
} from "./hub_panel_menu_m3e_browser_scripts.ts";
import type { HubPanelMenuM3eBrowserScriptInput } from "./hub_panel_menu_m3e_browser_scripts.types.ts";
import type { HubPanelMenuM3eStartupExpectation } from "./hub_panel_menu_m3e_observations.types.ts";

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function requireBoolean(record: Record<string, unknown>, key: string): boolean {
  const value = record[key];
  if (typeof value !== "boolean") {
    throw new Error(`M3e observation lacks ${key}`);
  }
  return value;
}

const browserSandboxes = new WeakMap<MarionetteClient, string>();
const unusableConnections = new WeakSet<MarionetteClient>();
const protocolDiagnostics = new WeakMap<MarionetteClient, string>();

export function hubPanelMenuM3eProtocolError(error: unknown) {
  const cause = error instanceof Error ? error.cause : error;
  const detail = isRecord(cause) ? cause : {};
  return {
    category: String(
      detail.name ?? (error instanceof Error ? error.name : "unknown"),
    ).slice(0, 300),
    message: String(
      detail.message ?? (error instanceof Error ? error.message : "unknown"),
    ).slice(0, 300),
  };
}

export class HubPanelMenuM3eConnectionUnusableError extends Error {
  constructor() {
    super("M3e browser connection is unusable");
  }
}

export function bindHubPanelMenuM3eBrowserSandbox(
  client: MarionetteClient,
  taskId: string,
  rowId: string,
  diagnosticsPath?: string,
): void {
  if (!/^[0-9a-f-]{36}$/.test(taskId) || !/^[ABC]$/.test(rowId)) {
    throw new Error("M3e browser sandbox key is invalid");
  }
  if (browserSandboxes.has(client)) {
    throw new Error("M3e browser sandbox is already bound");
  }
  browserSandboxes.set(client, `hub-panel-menu-m3e-${taskId}-${rowId}`);
  if (diagnosticsPath) protocolDiagnostics.set(client, diagnosticsPath);
}

export async function closeHubPanelMenuM3eBrowser(
  client: MarionetteClient,
): Promise<void> {
  try {
    if (unusableConnections.has(client)) {
      throw new HubPanelMenuM3eConnectionUnusableError();
    }
    const result = await executeHubPanelMenuM3eBrowserScript(client, {
      kind: "clear-task-state",
    });
    if (!isRecord(result) || result.cleared !== true) {
      throw new Error("M3e browser sandbox state was not cleared");
    }
  } finally {
    browserSandboxes.delete(client);
    if (unusableConnections.has(client)) client.abort();
    else await client.close();
  }
}

export async function executeHubPanelMenuM3eBrowserScript(
  client: MarionetteClient,
  input: HubPanelMenuM3eBrowserScriptInput,
  timeoutMs = 60_000,
): Promise<unknown> {
  const sandbox = browserSandboxes.get(client);
  if (unusableConnections.has(client)) {
    throw new HubPanelMenuM3eConnectionUnusableError();
  }
  if (!sandbox) throw new Error("M3e browser sandbox is not bound");
  const response: unknown = await sendHubPanelMenuM3eBrowserCommand(
    client,
    "WebDriver:ExecuteAsyncScript",
    {
      script: buildHubPanelMenuM3eBrowserScript(input),
      args: [],
      sandbox,
      newSandbox: false,
    },
    timeoutMs,
  );
  return validateHubPanelMenuM3eBrowserResponse(response);
}

export async function sendHubPanelMenuM3eBrowserCommand(
  client: MarionetteClient,
  command: string,
  params: Readonly<Record<string, unknown>>,
  timeoutMs = 10000,
  onError?: (error: unknown) => Promise<void>,
): Promise<unknown> {
  if (unusableConnections.has(client)) {
    throw new HubPanelMenuM3eConnectionUnusableError();
  }
  try {
    return await client.sendWithTimeout(command, params, timeoutMs);
  } catch (error) {
    unusableConnections.add(client);
    client.abort();
    const path = protocolDiagnostics.get(client);
    if (path) {
      await Deno.writeTextFile(
        path,
        JSON.stringify(
          {
            diagnosticsOnly: true,
            command,
            at: Date.now(),
            ...hubPanelMenuM3eProtocolError(error),
          },
          null,
          2,
        ) + "\n",
        { mode: 0o600 },
      );
    }
    if (onError) await onError(error);
    throw new HubPanelMenuM3eConnectionUnusableError();
  }
}

function validateHubPanelMenuM3eBrowserResponse(response: unknown): unknown {
  if (!isRecord(response) || !Object.hasOwn(response, "value")) {
    throw new Error("M3e browser script returned no value");
  }
  const value = response.value;
  const serialized = JSON.stringify(value);
  if (serialized === undefined || serialized.length > 16 * 1024 * 1024) {
    throw new Error("M3e browser script result exceeded its output bound");
  }
  if (isRecord(value) && isRecord(value.scriptError)) {
    const stages = new Set([
      "profile-path",
      "clear-task-state",
      "lifecycle",
      "focus",
      "startup-observation",
      "post-start-immutability",
      "policy-binding",
      "capture-inertness",
      "download-package",
      "create-host",
      "reopen-host",
      "host-operation",
      "arm-barrier",
      "start-gated-operation",
      "release-barrier",
      "await-operation",
      "close-host",
    ]);
    const stage = typeof value.scriptError.stage === "string" &&
        stages.has(value.scriptError.stage)
      ? value.scriptError.stage
      : "unknown";
    const checks = new Set([
      "imports",
      "startup-imports",
      "startup-bootstrap-import",
      "startup-registry-import",
      "task-state",
      "task-state-key",
      "task-state-window",
      "task-state-table",
      "task-state-entry",
      "startup-readiness",
      "menu-shown",
      "menu-ui",
      "menu-hidden",
      "pinned-state",
      "pinned-snapshot",
      "startup-registry",
      "prepare-windows",
      "new-window",
      "retain",
      "command",
      "stale",
      "reject-operation",
      "close-windows",
      "window-context",
      "interactive-menu",
      "snapshot",
      "snapshot-window-role",
      "snapshot-menu-open",
      "snapshot-translated-label",
      "snapshot-menu-close",
      "snapshot-journal",
      "snapshot-cache",
      "passive-ui",
      ...["initial", "second", "private", "late"].flatMap((role) =>
        [
          "snapshot-menu-open-",
          "snapshot-menu-close-",
          "snapshot-translated-label-",
        ].map((phase) => phase + role)
      ),
      "observe-generations",
      "operation-close-window",
      "operation-closed-window-command",
      "operation-begin-host-close",
      "operation-await-host-close",
      "focus-prepare",
      "focus-open",
      "focus-state",
      "focus-act",
      "focus-result",
      "focus-monitor",
      "focus-dispose",
    ]);
    const check = typeof value.scriptError.check === "string" &&
        checks.has(value.scriptError.check)
      ? value.scriptError.check
      : "unknown";
    throw new Error(`M3e browser script failed [${stage}/${check}]`);
  }
  return value;
}

export function assertHubPanelMenuM3eStartupObservation(
  value: unknown,
  expected: HubPanelMenuM3eStartupExpectation,
): void {
  if (!isRecord(value)) {
    throw new Error("M3e startup observation is not an object");
  }
  if (
    value.startupMode !== "dev" || value.profileDir !== expected.profilePath
  ) {
    throw new Error("M3e browser startup mode or profile path differs");
  }
  if (
    value.featureEnabled !== expected.featureEnabled ||
    value.manualHostEnabled !== expected.manualHostEnabled
  ) throw new Error("M3e browser did not retain seeded startup preferences");
  if (value.loaderStatus !== "loaded" || value.startupError !== "") {
    throw new Error("M3e development loader did not complete cleanly");
  }
  if (
    value.mainWindowCandidate !== expected.featureEnabled ||
    value.pinnedRootExists !== expected.pinnedRootExists ||
    value.selection !== expected.selection ||
    value.menuReady !== true
  ) {
    throw new Error(
      "M3e startup selection, pinned state, or menu readiness differs",
    );
  }
  if (
    !isRecord(value.pinnedRootSnapshot) ||
    value.pinnedRootSnapshot.exists !== expected.pinnedRootExists ||
    (expected.pinnedRootExists &&
      (!Array.isArray(value.pinnedRootSnapshot.entries) ||
        value.pinnedRootSnapshot.entries.length === 0))
  ) throw new Error("M3e canonical pinned state snapshot is missing or empty");
  if (expected.selection === "pinned") {
    const state = value.pinnedState;
    const pinnedEntries = value.pinnedRootSnapshot.entries;
    const lastOutcome = isRecord(state) && isRecord(state.lastOutcome)
      ? state.lastOutcome
      : null;
    if (!Array.isArray(pinnedEntries)) {
      throw new Error("M3e pinned root snapshot has no entries");
    }
    const stateEntry = pinnedEntries.find((entry) =>
      isRecord(entry) && entry.path === "state.json" && entry.type === "file"
    );
    if (
      !isRecord(state) ||
      state.schemaVersion !== 2 || state.featureId !== "hub-panel-menu" ||
      state.selectedVersion !== "hub-menu-v1" ||
      typeof state.expectedDigest !== "string" ||
      state.selectedDigest !== state.expectedDigest ||
      state.desiredEnabled !== true || state.selectedHealth !== "verified" ||
      state.pending !== null || state.blockedReason !== null ||
      !lastOutcome || lastOutcome.status !== "succeeded" ||
      lastOutcome.selectedVersion !== "hub-menu-v1" ||
      !isRecord(stateEntry) || stateEntry.sha256 !== state.snapshotSha256 ||
      stateEntry.size !== state.snapshotSize
    ) throw new Error("M3e pinned state.json is not verified and successful");
  } else if (value.pinnedState !== null) {
    throw new Error("M3e non-pinned startup exposed pinned state");
  }
  if (!Array.isArray(value.windows)) {
    throw new Error("M3e startup observation lacks browser windows");
  }
  const normalWindows = value.windows.filter((window) =>
    isRecord(window) && window.windowType === "navigator:browser" &&
    window.private === false
  );
  if (normalWindows.length === 0) {
    throw new Error("M3e startup has no normal browser window");
  }
  if (
    !normalWindows.every((window) =>
      isRecord(window) && window.delayedStartupFinished === true
    )
  ) throw new Error("M3e browser delayed startup is incomplete");
  if (!isRecord(value.sharedRegistry)) {
    throw new Error("M3e startup omitted shared AppMenuRegistry diagnostics");
  }
  const registry = value.sharedRegistry;
  if (typeof registry.connections !== "number") {
    throw new Error("M3e shared AppMenuRegistry connection count is missing");
  }
  if (expected.selection === "legacy") {
    if (
      registry.connections !== 0 ||
      requireBoolean(registry, "published") ||
      requireBoolean(registry, "enabled") ||
      requireBoolean(registry, "staged") ||
      registry.pendingAcknowledgements !== 0 || value.pinnedRootExists
    ) throw new Error("legacy startup unexpectedly owns a pinned adapter");
  } else if (expected.selection === "manual") {
    if (
      registry.connections < normalWindows.length ||
      requireBoolean(registry, "published") ||
      requireBoolean(registry, "enabled") ||
      requireBoolean(registry, "staged") ||
      registry.pendingAcknowledgements !== 0 || value.pinnedRootExists
    ) throw new Error("manual startup did not retain only window adapters");
  } else if (
    registry.connections < normalWindows.length ||
    !requireBoolean(registry, "published") ||
    !requireBoolean(registry, "enabled")
  ) {
    throw new Error(
      "pinned startup did not publish through the shared app-menu registry",
    );
  }
  if (!Array.isArray(value.ui) || value.ui.length !== normalWindows.length) {
    throw new Error("M3e startup omitted normal-window menu UI observations");
  }
  const buttons = value.ui;
  if (expected.selection === "pinned" || expected.selection === "legacy") {
    if (
      !buttons.every((item) =>
        isRecord(item) && item.buttonPresent === true &&
        item.buttonCount === 1 &&
        item.buttonId === "appMenu-floorp-hub-button" &&
        item.buttonEnabled === true && typeof item.buttonLabel === "string" &&
        item.buttonLabel.trim().length > 0 &&
        item.immediatelyAfterSettings === true
      )
    ) {
      throw new Error(
        "M3e actual HUB button ID, count, enabled state, label, or placement differs",
      );
    }
  } else if (
    !buttons.every((item) =>
      isRecord(item) && item.buttonPresent === false &&
      item.buttonCount === 0 &&
      item.buttonId === null && item.buttonEnabled === null &&
      item.buttonLabel === null
    )
  ) {
    throw new Error(
      "manual startup unexpectedly has a normal-window HUB button",
    );
  }
}

export function assertHubPanelMenuM3ePostStartImmutability(
  before: unknown,
  after: unknown,
  selection: "legacy" | "pinned" | "manual",
): void {
  if (
    !isRecord(before) || !isRecord(after) ||
    after.selectedModeUnchanged !== true ||
    after.preferencesRestored !== true || after.selection !== selection ||
    before.selection !== selection ||
    before.taskStateToken !== after.taskStateToken ||
    before.pinnedRootExists !== after.rootExists ||
    canonicalForObservation(before.pinnedRootSnapshot) !==
      canonicalForObservation(after.rootSnapshot) ||
    !isRecord(before.sharedRegistry) || !isRecord(after.registry) ||
    canonicalForObservation(before.sharedRegistry) !==
      canonicalForObservation(after.registry) ||
    !Array.isArray(before.ui) || !Array.isArray(after.ui)
  ) {
    throw new Error(
      "M3e post-start selection immutability or preference restoration failed",
    );
  }
  const beforeButtons = before.ui.filter((item) => isRecord(item)).map((item) =>
    `${item.buttonCount}:${item.buttonId}:${item.buttonEnabled}:${item.buttonLabel}:${item.immediatelyAfterSettings}:${item.nodeId}`
  );
  const afterButtons = after.ui.filter((item) => isRecord(item)).map((item) =>
    `${item.buttonCount}:${item.buttonId}:${item.buttonEnabled}:${item.buttonLabel}:${item.immediatelyAfterSettings}:${item.nodeId}`
  );
  if (
    beforeButtons.join(",") !== afterButtons.join(",") ||
    after.buttonPresent !== (selection !== "manual")
  ) {
    throw new Error(
      "M3e post-start UI ownership changed after preference mutation",
    );
  }
}

function canonicalForObservation(value: unknown): string {
  if (Array.isArray(value)) {
    return `[${value.map(canonicalForObservation).join(",")}]`;
  }
  if (isRecord(value)) {
    return `{${
      Object.keys(value).sort().map((key) =>
        `${key}:${canonicalForObservation(value[key])}`
      ).join(",")
    }}`;
  }
  return JSON.stringify(value) ?? "undefined";
}

export async function runHubPanelMenuM3eGatedOperation(
  invoke: (input: HubPanelMenuM3eBrowserScriptInput) => Promise<unknown>,
  input: Readonly<Record<string, unknown>>,
  whileStopped: () => Promise<void>,
  timeoutMs = 15_000,
): Promise<unknown> {
  const testId = input.testId;
  if (typeof testId !== "string") {
    throw new Error("M3e gated operation lacks test ID");
  }
  let operationStarted = false;
  let bodyError: unknown;
  let releaseError: unknown;
  let terminal: unknown;
  let connectionUsable = true;
  try {
    const armed = await invoke({
      kind: "arm-barrier",
      payload: { ...input, phase: "OLD_STOPPED", timeoutMs },
    });
    if (!isRecord(armed) || armed.armed !== true || armed.txId !== input.txId) {
      throw new Error("M3e operation barrier could not be armed");
    }
    operationStarted = true;
    const barrier = await invoke({
      kind: "start-gated-operation",
      payload: { ...input, timeoutMs },
    });
    if (
      !isRecord(barrier) || barrier.barrierReached !== true ||
      barrier.txId !== input.txId || barrier.operation !== input.operation
    ) throw new Error("M3e transaction did not reach its OLD_STOPPED barrier");
    await whileStopped();
  } catch (error) {
    bodyError = error;
    connectionUsable =
      !(error instanceof HubPanelMenuM3eConnectionUnusableError);
  } finally {
    if (operationStarted && connectionUsable) {
      try {
        await invoke({ kind: "release-barrier", payload: input });
      } catch (error) {
        releaseError = error;
        connectionUsable =
          !(error instanceof HubPanelMenuM3eConnectionUnusableError);
      }
    }
    if (operationStarted && connectionUsable) {
      try {
        terminal = await invoke({
          kind: "await-operation",
          payload: { ...input, timeoutMs },
        });
        if (!isRecord(terminal) || terminal.timedOut === true) {
          releaseError = new Error("M3e terminal operation timed out");
        }
      } catch (error) {
        releaseError = releaseError === undefined ? error : new AggregateError(
          [releaseError, error],
          "M3e barrier release and terminal wait failed",
        );
      }
    }
  }
  if (bodyError !== undefined && releaseError !== undefined) {
    throw new AggregateError(
      [bodyError, releaseError],
      "M3e barrier operation and cleanup failed",
    );
  }
  if (bodyError !== undefined) throw bodyError;
  if (releaseError !== undefined) throw releaseError;
  return terminal;
}
