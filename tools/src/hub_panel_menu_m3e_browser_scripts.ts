// SPDX-License-Identifier: MPL-2.0

import type {
  HubPanelMenuM3eBrowserScriptInput,
  HubPanelMenuM3eBrowserScriptKind,
} from "./hub_panel_menu_m3e_browser_scripts.types.ts";
import { HUB_PANEL_MENU_M3E_GATE_SCRIPT } from "./hub_panel_menu_m3e_gate_script.ts";
import { HUB_PANEL_MENU_M3E_LIFECYCLE_SCRIPT } from "./hub_panel_menu_m3e_lifecycle_script.ts";
import { HUB_PANEL_MENU_M3E_FOCUS_SCRIPT } from "./hub_panel_menu_m3e_focus_script.ts";
export type {
  HubPanelMenuM3eBrowserScriptInput,
  HubPanelMenuM3eBrowserScriptKind,
} from "./hub_panel_menu_m3e_browser_scripts.types.ts";

export const HUB_PANEL_MENU_M3E_SIGNATURE_CONTROL_SCRIPT = `
  async function observeReopenVerification(open, digest) {
    const cacheModule = ChromeUtils.importESModule("resource://noraneko/modules/feature-host/package/FeaturePackageCache.sys.mjs");
    const subtle = Components.utils.getGlobalForObject(cacheModule.openTestProfileFeaturePackageCache).crypto.subtle;
    const nativeDigest = subtle.digest;
    const nativeVerify = subtle.verify;
    const digestDescriptor = Object.getOwnPropertyDescriptor(subtle, "digest");
    const verifyDescriptor = Object.getOwnPropertyDescriptor(subtle, "verify");
    const signatures = new Set();
    const observed = [];
    try {
    Object.defineProperty(subtle, "digest", { configurable: true, value: async function(algorithm, data) {
      const result = await nativeDigest.call(this, algorithm, data);
      const hash = Array.from(new Uint8Array(result), (byte) => byte.toString(16).padStart(2, "0")).join("");
      if (hash === digest) {
        const envelope = JSON.parse(new TextDecoder().decode(data));
        if (typeof envelope.signature !== "string") throw new Error("reopen digest was not an envelope");
        signatures.add(envelope.signature);
      }
      return result;
    } });
    Object.defineProperty(subtle, "verify", { configurable: true, value: async function(algorithm, key, signature, data) {
      const valid = await nativeVerify.call(this, algorithm, key, signature, data);
      const encoded = btoa(String.fromCharCode(...new Uint8Array(signature))).replaceAll("+", "-").replaceAll("/", "_").replaceAll("=", "");
      if (algorithm.name === "Ed25519" && signatures.has(encoded)) observed.push({ packageSha256: digest, verified: valid === true });
      return valid;
    } });
      const host = await open();
      if (!observed.length || !observed.every((entry) => entry.verified)) throw new Error("real reopen signature verification unobserved");
      return { host, observed };
    } finally {
      if (digestDescriptor) Object.defineProperty(subtle, "digest", digestDescriptor); else delete subtle.digest;
      if (verifyDescriptor) Object.defineProperty(subtle, "verify", verifyDescriptor); else delete subtle.verify;
    }
  }

  async function signatureControl(testId, digest, policy) {
    const cacheModule = ChromeUtils.importESModule("resource://noraneko/modules/feature-host/package/FeaturePackageCache.sys.mjs");
    const cache = await cacheModule.openTestProfileFeaturePackageCache("hub-panel-menu", testId);
    try {
      const verified = await cache.read(digest, policy);
      if (verified.packageSha256 !== digest) throw new Error("verification digest differs");
      const wrongKey = await window.crypto.subtle.generateKey({ name: "Ed25519" }, false, ["sign", "verify"]);
      const wrongRaw = new Uint8Array(await window.crypto.subtle.exportKey("raw", wrongKey.publicKey));
      const wrongPublicKey = btoa(String.fromCharCode(...wrongRaw)).replaceAll("+", "-").replaceAll("/", "_").replaceAll("=", "");
      const wrongPolicy = { ...policy, trustedKeys: policy.trustedKeys.map((key) => ({ ...key, publicKey: wrongPublicKey })) };
      let rejected = false;
      try { await cache.read(digest, wrongPolicy); }
      catch (error) { if (error?.code !== "invalid-signature") throw error; rejected = true; }
      if (!rejected) throw new Error("signature-only negative control was accepted");
      return { packageSha256: digest, correctKeyVerified: true, wrongKeyRejected: true, rejection: "invalid-signature" };
    } finally { await cache.close(); }
  }
`;

const scriptBody = `async function runHubPanelMenuM3e(input) {
  if (input.kind === "clear-task-state") {
    for (const task of Object.values(globalThis.__hubPanelMenuM3eStateByTask ?? {})) task.registryObserver?.restore();
    delete globalThis.__hubPanelMenuM3eStateByTask;
    return { cleared: !Object.hasOwn(globalThis, "__hubPanelMenuM3eStateByTask") };
  }
  input.__check = "imports";
  const FEATURE_HOST_URI = "resource://noraneko/modules/feature-host/FeatureHost.sys.mjs";
  const SIGNED_HOST_URI = "resource://noraneko/modules/feature-host/M3cSignedExecution.sys.mjs";
  const REGISTRY_URI = "resource://noraneko/modules/feature-host/AppMenuRegistry.sys.mjs";
  const DOWNLOADER_URI = "resource://noraneko/modules/feature-host/package/FeaturePackageDownloader.sys.mjs";
  const { setTimeout, clearTimeout } = ChromeUtils.importESModule("resource://gre/modules/Timer.sys.mjs");
  const { PrivateBrowsingUtils } = ChromeUtils.importESModule("resource://gre/modules/PrivateBrowsingUtils.sys.mjs");
  const { FileUtils } = ChromeUtils.importESModule("resource://gre/modules/FileUtils.sys.mjs");
  const MAX_SNAPSHOT_ENTRIES = 4096;
  const MAX_SNAPSHOT_FILE_BYTES = 4 * 1024 * 1024;
  const MAX_SNAPSHOT_TOTAL_BYTES = 16 * 1024 * 1024;
  const MAX_PINNED_STATE_BYTES = 256 * 1024;
  ${HUB_PANEL_MENU_M3E_SIGNATURE_CONTROL_SCRIPT}
  ${HUB_PANEL_MENU_M3E_GATE_SCRIPT}
  function getTaskState(taskId, rowId) {
    input.__check = "task-state-key";
    if (typeof taskId !== "string" || !/^[0-9a-f-]{36}$/.test(taskId) || !/^[ABC]$/.test(rowId)) throw new Error("invalid task-scoped state key");
    input.__check = "task-state-table";
    const table = globalThis.__hubPanelMenuM3eStateByTask ??= Object.create(null);
    const key = taskId + ":" + rowId;
    input.__check = "task-state-entry";
    return table[key] ??= { key, token: key + ":" + Date.now(), hosts: Object.create(null), closedHosts: Object.create(null), barriers: Object.create(null), nodeIds: new WeakMap(), nextNodeId: 0 };
  }
  function nodeId(state, node) {
    if (!node) return null;
    let id = state.nodeIds.get(node);
    if (!id) { id = ++state.nextNodeId; state.nodeIds.set(node, id); }
    return id;
  }
  async function snapshotDirectory(path) {
    if (!await IOUtils.exists(path)) return { exists: false, entries: [] };
    if (new FileUtils.File(path).isSymlink()) throw new Error("snapshot root is a symlink");
    const rootInfo = await IOUtils.stat(path);
    if (rootInfo.type !== "directory") throw new Error("snapshot root is not a directory");
    const entries = [];
    let totalBytes = 0;
    async function visit(directory, prefix) {
      const children = await IOUtils.getChildren(directory);
      for (const child of children) {
        if (entries.length >= MAX_SNAPSHOT_ENTRIES) throw new Error("feature snapshot entry limit exceeded");
        if (new FileUtils.File(child).isSymlink()) throw new Error("snapshot contains a symlink");
        const name = PathUtils.filename(child);
        const relativePath = prefix ? prefix + "/" + name : name;
        const info = await IOUtils.stat(child);
        if (info.type === "directory") {
          entries.push({ path: relativePath, type: "directory" });
          await visit(child, relativePath);
        } else if (info.type === "regular") {
          if (info.size > MAX_SNAPSHOT_FILE_BYTES || totalBytes + info.size > MAX_SNAPSHOT_TOTAL_BYTES) throw new Error("feature snapshot byte limit exceeded");
          const bytes = await IOUtils.read(child, { maxBytes: MAX_SNAPSHOT_FILE_BYTES + 1 });
          if (bytes.length !== info.size || bytes.length > MAX_SNAPSHOT_FILE_BYTES) throw new Error("feature snapshot file changed or exceeded its bound");
          totalBytes += bytes.length;
          const digest = new Uint8Array(await crypto.subtle.digest("SHA-256", bytes));
          entries.push({ path: relativePath, type: "file", size: bytes.length, sha256: Array.from(digest, (value) => value.toString(16).padStart(2, "0")).join("") });
        } else {
          throw new Error("unsupported feature snapshot entry type");
        }
      }
    }
    await visit(path, "");
    entries.sort((left, right) => left.path.localeCompare(right.path));
    return { exists: true, entries };
  }
  async function readPinnedState(path, expectedDigest) {
    if (new FileUtils.File(path).isSymlink()) throw new Error("pinned state is a symlink");
    const info = await IOUtils.stat(path);
    if (info.type !== "regular" || info.size > MAX_PINNED_STATE_BYTES) throw new Error("pinned state file is not bounded regular data");
    const bytes = await IOUtils.read(path, { maxBytes: MAX_PINNED_STATE_BYTES + 1 });
    if (bytes.length !== info.size || bytes.length > MAX_PINNED_STATE_BYTES) throw new Error("pinned state file changed or exceeded its bound");
    const state = JSON.parse(new TextDecoder("utf-8", { fatal: true }).decode(bytes));
    if (typeof state !== "object" || state === null || Array.isArray(state) || state.schemaVersion !== 2 || state.featureId !== "hub-panel-menu" || state.selectedVersion !== "hub-menu-v1" || state.selectedDigest !== expectedDigest || state.desiredEnabled !== true || state.blockedReason !== null) throw new Error("pinned state identity or selection differs");
    if (state.pending !== null || state.selectedHealth !== "verified" || typeof state.lastOutcome !== "object" || state.lastOutcome === null || state.lastOutcome.status !== "succeeded" || state.lastOutcome.selectedVersion !== "hub-menu-v1") return null;
    const digest = new Uint8Array(await crypto.subtle.digest("SHA-256", bytes));
    return {
      schemaVersion: state.schemaVersion,
      featureId: state.featureId,
      selectedVersion: state.selectedVersion,
      selectedDigest: state.selectedDigest,
      expectedDigest,
      desiredEnabled: state.desiredEnabled,
      selectedHealth: state.selectedHealth,
      pending: state.pending,
      blockedReason: state.blockedReason,
      lastOutcome: { status: state.lastOutcome.status, selectedVersion: state.lastOutcome.selectedVersion },
      snapshotSize: bytes.length,
      snapshotSha256: Array.from(digest, (value) => value.toString(16).padStart(2, "0")).join(""),
    };
  }
  function delay(ms) { return new Promise((resolve) => { setTimeout(resolve, ms); }); }
  async function waitFor(predicate, timeoutMs, label) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      const result = await predicate();
      if (result) return result;
      await delay(50);
    }
    throw new Error(label + " timed out");
  }
  function waitForPopupEvent(popup, eventName, timeoutMs) {
    let cancel = () => {};
    const promise = new Promise((resolve, reject) => {
      let settled = false;
      let timer = null;
      const cleanup = () => { popup.removeEventListener(eventName, onEvent); if (timer !== null) clearTimeout(timer); };
      const finish = (callback) => { if (settled) return; settled = true; cleanup(); callback(); };
      const onEvent = () => finish(() => resolve(true));
      cancel = () => finish(() => resolve(false));
      popup.addEventListener(eventName, onEvent, { once: true });
      timer = setTimeout(() => finish(() => reject(new Error("app menu event timed out"))), timeoutMs);
    });
    return { promise, cancel: () => cancel() };
  }
  ${HUB_PANEL_MENU_M3E_LIFECYCLE_SCRIPT}
  ${HUB_PANEL_MENU_M3E_FOCUS_SCRIPT}
  if (input.kind === "profile-path") return { profileDir: PathUtils.profileDir };
  if (input.kind === "startup-observation") {
    input.__check = "startup-bootstrap-import";
    const bootstrap = ChromeUtils.importESModule("resource://noraneko/modules/feature-host/HubPanelMenuBootstrap.sys.mjs");
    input.__check = "startup-registry-import";
    const registryModule = ChromeUtils.importESModule(REGISTRY_URI);
    const state = getTaskState(input.testId, input.rowId);
    const readiness = await waitFor(() => {
      input.__check = "startup-readiness";
      const windows = [];
      const enumerator = Services.wm.getEnumerator("navigator:browser");
      while (enumerator.hasMoreElements()) windows.push(enumerator.getNext());
      const mainWindow = windows.find((win) => !PrivateBrowsingUtils.isWindowPrivate(win));
      const loaderStatus = Services.prefs.getStringPref("nora.startup.loader", "");
      const startupError = Services.prefs.getStringPref("nora.startup.error", "");
      if (startupError !== "") throw new Error("development startup reported an error");
      if (Services.prefs.getStringPref("nora.startup.mode", "") !== "dev") throw new Error("native row did not start in dev mode");
      if (loaderStatus !== "loaded" || !mainWindow || !windows.every((win) => win.gBrowserInit?.delayedStartupFinished === true)) return null;
      return { windows, mainWindow };
    }, 30_000, "startup loader and delayed windows");
    const { windows, mainWindow } = readiness;
    const featureEnabled = Services.prefs.getBoolPref("nora.dev.hub_menu_feature", false);
    const manualHostEnabled = Services.prefs.getBoolPref("nora.dev.hub_menu_manual_host", false);
    const startupMode = Services.prefs.getStringPref("nora.startup.mode", "");
    const trigger = mainWindow.document.getElementById("PanelUI-menu-button");
    const popup = mainWindow.document.getElementById("appMenu-popup");
    if (!trigger || !popup) throw new Error("real app-menu trigger is unavailable");
    const openedByTest = popup.state !== "open";
    input.__check = "menu-shown";
    if (openedByTest) {
      const shown = waitForPopupEvent(popup, "popupshown", 10_000);
      try { trigger.click(); await shown.promise; } finally { shown.cancel(); }
    }
    const normalWindows = windows.filter((win) => !PrivateBrowsingUtils.isWindowPrivate(win));
    let ui = [];
    let menuReady = false;
    try {
      input.__check = "menu-ui";
      const settings = await waitFor(() => mainWindow.document.getElementById("appMenu-settings-button"), 10_000, "Settings menu anchor");
      ui = normalWindows.map((win) => {
        const buttonNodes = win.document.querySelectorAll("#appMenu-floorp-hub-button");
        const button = buttonNodes[0] ?? null;
        const settingsButton = win.document.getElementById("appMenu-settings-button");
        return { buttonPresent: !!button, buttonCount: buttonNodes.length, buttonId: button?.id ?? null, buttonEnabled: button ? !button.disabled : null, buttonLabel: button?.getAttribute("label") ?? null, parentId: button?.parentElement?.id ?? null, immediatelyAfterSettings: !!button && button.parentElement === settingsButton?.parentElement && button.previousElementSibling === settingsButton, nodeId: nodeId(state, button) };
      });
      menuReady = !!settings;
    } finally {
      if (openedByTest && popup.state === "open") {
        input.__check = "menu-hidden";
        const hidden = waitForPopupEvent(popup, "popuphidden", 10_000);
        try { popup.hidePopup(); await hidden.promise; } finally { hidden.cancel(); }
      }
    }
    const pinnedRoot = PathUtils.join(PathUtils.profileDir, "floorp-feature-hub-panel-menu");
    input.__check = "pinned-state";
    const fixtures = ChromeUtils.importESModule("resource://noraneko/modules/feature-host/HubPanelMenuFixtures.sys.mjs").HUB_PANEL_MENU_FIXTURES;
    let pinnedState = null;
    if (featureEnabled && !manualHostEnabled) {
      const expectedDigest = fixtures["hub-menu-v1"].digest;
      const statePath = PathUtils.join(pinnedRoot, "state.json");
      pinnedState = await waitFor(async () => await IOUtils.exists(statePath) ? readPinnedState(statePath, expectedDigest) : null, 20_000, "verified pinned state");
    }
    input.__check = "pinned-snapshot";
    const pinnedRootSnapshot = await snapshotDirectory(pinnedRoot);
    const pinnedRootExists = pinnedRootSnapshot.exists;
    if (pinnedState) {
      const stateEntry = pinnedRootSnapshot.entries.find((entry) => entry.path === "state.json" && entry.type === "file");
      if (!stateEntry || stateEntry.sha256 !== pinnedState.snapshotSha256 || stateEntry.size !== pinnedState.snapshotSize) throw new Error("pinned state changed while canonical root was snapshotted");
    }
    input.__check = "startup-registry";
    const registry = registryModule.getSharedAppMenuRegistry().diagnosticsForTests;
    const observedSelection = bootstrap.hubPanelMenuStartupSelectionForTests();
    return {
      startupMode, featureEnabled, manualHostEnabled, profileDir: PathUtils.profileDir,
      loaderStatus: Services.prefs.getStringPref("nora.startup.loader", ""),
      startupError: Services.prefs.getStringPref("nora.startup.error", ""),
      menuReady,
      selection: observedSelection,
      taskStateToken: state.token,
      windows: windows.map((win) => ({
        windowType: win.document.documentElement.getAttribute("windowtype"),
        private: PrivateBrowsingUtils.isWindowPrivate(win),
        delayedStartupFinished: win.gBrowserInit?.delayedStartupFinished === true,
        candidate: bootstrap.usesExperimentalHubPanelMenu(win),
      })),
      mainWindowCandidate: bootstrap.usesExperimentalHubPanelMenu(mainWindow),
      pinnedRootExists,
      pinnedRootSnapshot,
      pinnedState,
      sharedRegistry: registry,
      ui,
    };
  }
  if (input.kind === "post-start-immutability") {
    const bootstrap = ChromeUtils.importESModule("resource://noraneko/modules/feature-host/HubPanelMenuBootstrap.sys.mjs");
    const registryModule = ChromeUtils.importESModule(REGISTRY_URI);
    const state = getTaskState(input.testId, input.rowId);
    const before = input.before;
    const prefNames = ["nora.startup.mode", "nora.dev.hub_menu_feature", "nora.dev.hub_menu_manual_host"];
    const originals = prefNames.map((name) => ({ name, hasUserValue: Services.prefs.prefHasUserValue(name), type: name === "nora.startup.mode" ? "string" : "boolean", value: name === "nora.startup.mode" ? Services.prefs.getStringPref(name, "") : Services.prefs.getBoolPref(name, false) }));
    let observed;
    try {
      const latchedBefore = bootstrap.hubPanelMenuStartupSelectionForTests();
      Services.prefs.setBoolPref("nora.dev.hub_menu_manual_host", !Services.prefs.getBoolPref("nora.dev.hub_menu_manual_host", false));
      const latchedAfterManualOnly = bootstrap.hubPanelMenuStartupSelectionForTests();
      if (latchedBefore !== input.expectedSelection || latchedAfterManualOnly !== latchedBefore) throw new Error("latched selection changed");
      Services.prefs.setStringPref("nora.startup.mode", input.toggleMode);
      Services.prefs.setBoolPref("nora.dev.hub_menu_feature", input.toggleFeature);
      Services.prefs.setBoolPref("nora.dev.hub_menu_manual_host", input.toggleManual);
      const windows = [];
      const enumerator = Services.wm.getEnumerator("navigator:browser");
      while (enumerator.hasMoreElements()) windows.push(enumerator.getNext());
      const mainWindow = windows.find((win) => !PrivateBrowsingUtils.isWindowPrivate(win));
      const rootPath = PathUtils.join(PathUtils.profileDir, "floorp-feature-hub-panel-menu");
      const rootSnapshot = await snapshotDirectory(rootPath);
      const rootExists = rootSnapshot.exists;
      const registry = registryModule.getSharedAppMenuRegistry().diagnosticsForTests;
      const normalWindows = windows.filter((win) => !PrivateBrowsingUtils.isWindowPrivate(win));
      const ui = normalWindows.map((win) => {
        const buttonNodes = win.document.querySelectorAll("#appMenu-floorp-hub-button");
        const button = buttonNodes[0] ?? null;
        const settings = win.document.getElementById("appMenu-settings-button");
        return { buttonPresent: !!button, buttonCount: buttonNodes.length, nodeId: nodeId(state, button), buttonId: button?.id ?? null, buttonEnabled: button ? !button.disabled : null, buttonLabel: button?.getAttribute("label") ?? null, immediatelyAfterSettings: !!button && button.parentElement === settings?.parentElement && button.previousElementSibling === settings };
      });
      const button = mainWindow?.document.getElementById("appMenu-floorp-hub-button") ?? null;
      observed = { taskStateToken: state.token, selection: bootstrap.hubPanelMenuStartupSelectionForTests(), latchedBefore, latchedAfterManualOnly, rootExists, rootSnapshot, registry, buttonPresent: !!button, buttonNodeId: nodeId(state, button), ui };
    } finally {
      for (const original of originals) {
        if (!original.hasUserValue) Services.prefs.clearUserPref(original.name);
        else if (original.type === "string") Services.prefs.setStringPref(original.name, original.value);
        else Services.prefs.setBoolPref(original.name, original.value);
      }
    }
    const preferencesRestored = originals.every((original) => Services.prefs.prefHasUserValue(original.name) === original.hasUserValue && (original.type === "string" ? Services.prefs.getStringPref(original.name, "") : Services.prefs.getBoolPref(original.name, false)) === original.value);
    return { ...observed, preferencesRestored, selectedModeUnchanged: observed.selection === input.expectedSelection };
  }
  if (input.kind === "policy-binding") {
    const hostModule = ChromeUtils.importESModule(FEATURE_HOST_URI);
    return hostModule.m0SignedPolicyBindingForTests();
  }
  if (input.kind === "capture-inertness") {
    const hostModule = ChromeUtils.importESModule(FEATURE_HOST_URI);
    const registryModule = ChromeUtils.importESModule(REGISTRY_URI);
    const state = getTaskState(input.testId, input.rowId ?? "C");
    const host = state.hosts["hub-panel-menu"] ?? null;
    const windows = [];
    const enumerator = Services.wm.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) windows.push(enumerator.getNext());
    const ui = windows.map((win) => {
      const button = win.document.getElementById("appMenu-floorp-hub-button");
      return {
        private: PrivateBrowsingUtils.isWindowPrivate(win),
        buttonPresent: !!button,
        nodeId: nodeId(state, button),
        buttonDisabled: button?.disabled ?? null,
        buttonLabel: button?.getAttribute("label") ?? null,
      };
    });
    const root = PathUtils.profileDir;
    const canonicalRoot = PathUtils.join(root, "floorp-feature-hub-panel-menu");
    const journalRoot = PathUtils.join(root, "floorp-hotfix-signed-tests-v1", input.testId, "hub-panel-menu");
    const cachePath = PathUtils.join(root, "floorp-signed-package-cache-v1", "tests", input.testId, "hub-panel-menu");
    const [canonicalRootSnapshot, journalRootSnapshot, packageCacheSnapshot] = await Promise.all([snapshotDirectory(canonicalRoot), snapshotDirectory(journalRoot), snapshotDirectory(cachePath)]);
    return {
      profileDir: root,
      taskStateToken: state.token,
      evaluationCount: hostModule.m0PayloadEvaluationsForTests(),
      sharedRegistry: registryModule.getSharedAppMenuRegistry().diagnosticsForTests,
      registryOwners: state.registryOwners ?? [],
      hostDiagnostics: host ? host.diagnostics() : null,
      hostMetadata: host ? host.metadata() : null,
      canonicalRootSnapshot,
      journalRootSnapshot,
      packageCacheSnapshot,
      ui,
    };
  }
  if (input.kind === "download-package") {
    const downloader = ChromeUtils.importESModule(DOWNLOADER_URI);
    const bytes = await downloader.downloadFeaturePackageBytes(input.request);
    let binary = "";
    for (const byte of bytes) binary += String.fromCharCode(byte);
    return { packageBase64: btoa(binary), byteLength: bytes.length };
  }
  if (input.kind === "create-host" || input.kind === "reopen-host") {
    const signedModule = ChromeUtils.importESModule(SIGNED_HOST_URI);
    const state = getTaskState(input.testId, input.rowId ?? "C");
    const hooks = {
      afterDurableWriteForTests(snapshot) {
        return reachM3eGate(state.barriers[input.testId], snapshot);
      },
    };
    const open = input.kind === "reopen-host" ? signedModule.reopenSignedFeatureHostForTests : signedModule.createSignedFeatureHostForTests;
    const signatureVerification = input.verificationDigest ? await signatureControl(input.testId, input.verificationDigest, input.policy) : null;
    const reopen = () => open(
      input.testId,
      "hub-panel-menu",
      true,
      { trustKind: "core-system-principal", policy: input.policy },
      hooks,
    );
    const actual = input.verificationDigest ? await observeReopenVerification(reopen, input.verificationDigest) : { host: await reopen(), observed: null };
    const host = actual.host;
    if (signatureVerification) signatureVerification.reopenVerification = actual.observed;
    state.hosts["hub-panel-menu"] = host;
    return { diagnostics: host.diagnostics(), metadata: host.metadata(), signatureVerification };
  }
  if (input.kind === "arm-barrier") {
    const state = getTaskState(input.testId, input.rowId ?? "C");
    const previous = state.barriers[input.testId];
    if (previous && !previous.released) throw new Error("M3e previous gate is not released");
    state.barriers[input.testId] = armM3eGate(input);
    return { armed: true, txId: input.txId, operation: input.operation, targetVersion: input.targetVersion ?? null, targetDigest: input.targetDigest ?? null };
  }
  if (input.kind === "host-operation") {
    const state = getTaskState(input.testId, input.rowId ?? "C");
    const host = state.hosts["hub-panel-menu"] ?? state.closedHosts["hub-panel-menu"];
    if (!host) throw new Error("M3e host is not open");
    const bytes = input.packageBase64 ? Uint8Array.from(atob(input.packageBase64), (c) => c.charCodeAt(0)) : null;
    if (input.operation === "stage") {
      if (!bytes) throw new Error("M3e stage requires package bytes");
      return await host.stage(bytes);
    }
    const operation = input.operation;
    if (operation === "install" || operation === "update" || operation === "enable" || operation === "disable" || operation === "rollback") {
      return await host[operation](input.txId, input.expectedRevision, input.packageSha256);
    }
    if (operation === "query") return host.queryOutcome(input.txId);
    if (operation === "observe") return { diagnostics: host.diagnostics(), metadata: host.metadata() };
    throw new Error("unknown M3e host operation");
  }
  if (input.kind === "start-gated-operation") {
    const task = getTaskState(input.testId, input.rowId ?? "C");
    const host = task.hosts["hub-panel-menu"] ?? task.closedHosts["hub-panel-menu"];
    const barrierState = task.barriers[input.testId];
    if (!host) throw new Error("M3e gated host is unavailable");
    return await startM3eGate(barrierState, input, host);
  }
  if (input.kind === "release-barrier") {
    const task = getTaskState(input.testId, input.rowId ?? "C");
    const state = task.barriers[input.testId];
    return { released: releaseM3eGate(state) };
  }
  if (input.kind === "await-operation") {
    const task = getTaskState(input.testId, input.rowId ?? "C");
    const host = task.hosts["hub-panel-menu"] ?? task.closedHosts["hub-panel-menu"];
    const state = task.barriers[input.testId];
    if (!host || !state?.operationPromise) throw new Error("M3e operation is not running");
    return await awaitM3eGate(state, host, input.timeoutMs);
  }
  if (input.kind === "close-host") {
    const task = getTaskState(input.testId, input.rowId ?? "C");
    const host = task.hosts["hub-panel-menu"];
    if (!host) throw new Error("M3e host is not open");
    await host.close();
    task.registryObserver?.restore();
    delete task.registryObserver;
    task.closedHosts["hub-panel-menu"] = host;
    delete task.hosts["hub-panel-menu"];
    return host.cleanupDiagnosticsForTests();
  }
  throw new Error("unknown M3e browser script kind");
}`;

export function buildHubPanelMenuM3eBrowserScript(
  input: HubPanelMenuM3eBrowserScriptInput,
): string {
  const payload = JSON.stringify({ ...input.payload, kind: input.kind });
  if (payload === undefined) {
    throw new Error("M3e browser script input is not serializable");
  }
  return `const done = arguments[arguments.length - 1];\nconst input = ${payload};\ninput.__stage = input.kind;\n(${scriptBody})(input).then(done, () => done({ scriptError: { stage: input.__stage, check: input.__check } }));`;
}

export function buildHubPanelMenuM3eBrowserScriptSuite(): readonly string[] {
  const kinds: readonly HubPanelMenuM3eBrowserScriptKind[] = [
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
  ];
  return kinds.map((kind) =>
    buildHubPanelMenuM3eBrowserScript({
      kind,
      payload: { expectedRevision: 1, packageSha256: "0".repeat(64) },
    })
  );
}
