// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { PLATFORM, PROJECT_ROOT } from "./src/defines.ts";
import {
  hashDirectoryContents,
  isHubDocumentUrl,
  withExactSourceRestoration,
} from "./src/hub_menu_acceptance.ts";
import type {
  AcceptanceMode,
  AttachCompletionGateSnapshot,
  CommandProbeSnapshot,
  FailureStateScenario,
  FocusMovementItemSnapshot,
  FocusSnapshot,
  HostOperationSnapshot,
  HostRecord,
  HostShutdownSnapshot,
  HostSnapshot,
  HostWindowIdentitySnapshot,
  MenuSnapshot,
  RegistrySnapshot,
  ResilienceSnapshot,
  Scenario,
  ScenarioPhaseVerdict,
  StartupSnapshot,
  StateFileSnapshot,
  ViteProbeSnapshot,
  ViteUpdateLogSnapshot,
} from "./hub_menu_startup_acceptance/types.ts";
import { MarionetteClient } from "./src/browser_connector.ts";
import { listDevProcesses, readDevProcesses } from "./src/dev_processes.ts";
import {
  matchesDevIdentity,
  selectDevProcesses,
} from "./src/dev_process_selection.ts";

const PROFILE_ROOT = path.join(
  PROJECT_ROOT,
  "_dist",
  "fdh",
  "profiles",
);
const RUN_ID = `stage2-hub-menu-startup-${Date.now()}-${Deno.pid}`;
const ARTIFACT_ROOT = path.join(PROFILE_ROOT, RUN_ID);
const SELECTED_PREF = "nora.dev.hub_menu_feature";
const HUB_ROOT_NAME = "floorp-feature-hub-panel-menu";
const BUTTON_ID = "appMenu-floorp-hub-button";
const ROLE_KEY = `__${RUN_ID.replaceAll("-", "_")}_window_role`;
const BOOTSTRAP_ERROR_OBSERVER_KEY = `${ROLE_KEY}_bootstrap_error_observer`;
const HOST_KEY = `${ROLE_KEY}_host`;
const OPERATION_KEY = `${ROLE_KEY}_operation`;
const ATTACH_COMPLETION_GATE_KEY = `${ROLE_KEY}_attach_completion_gate`;
const HOST_SHUTDOWN_KEY = `${ROLE_KEY}_host_shutdown`;
const PANEL_READY_KEY = `${ROLE_KEY}_panel_ready`;
const PROBE_KEY = `${ROLE_KEY}_command_probe`;
const RETAINED_BUTTON_KEY = `${ROLE_KEY}_retained_button`;
const VITE_PROBE_KEY = `${ROLE_KEY}_vite_probe`;
const HUB_VIEW_SOURCE_PATH = path.join(
  PROJECT_ROOT,
  "browser-features",
  "chrome",
  "common",
  "hub-panel-menu",
  "HubPanelMenuView.tsx",
);
const REGISTRY_URI =
  "resource://noraneko/modules/feature-host/AppMenuRegistry.sys.mjs";
const FEATURE_HOST_URI =
  "resource://noraneko/modules/feature-host/FeatureHost.sys.mjs";
const MARIONETTE_PORT_FILE = path.join(
  PROJECT_ROOT,
  "_dist",
  "marionette-port.txt",
);
const DEV_SERVER_PID_FILE = path.join(PROJECT_ROOT, "_dist", "dev-server.pid");
const DEV_PROCESS_FILE = path.join(PROJECT_ROOT, "_dist", "dev-processes.json");
const DEV_SERVER_LOG_FILE = path.join(PROJECT_ROOT, "_dist", "dev-server.log");
const DEV_TOOL_STATE_FILES = [
  DEV_SERVER_PID_FILE,
  DEV_PROCESS_FILE,
  MARIONETTE_PORT_FILE,
];

const decoder = new TextDecoder();
let logPath = "";
let observationPath = "";

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

function parseAcceptanceMode(args: readonly string[]): AcceptanceMode {
  if (args.length === 0) return "startup";
  if (args.length === 1 && args[0] === "--lifecycle") return "lifecycle";
  if (args.length === 1 && args[0] === "--hmr-only") return "hmr-only";
  if (args.length === 1 && args[0] === "--failure-states") {
    return "failure-states";
  }
  throw new Error(
    "usage: deno run -A tools/hub_menu_startup_acceptance.ts [--lifecycle|--hmr-only|--failure-states]",
  );
}

function log(message: string): void {
  const line = `${new Date().toISOString()} ${message}\n`;
  Deno.writeTextFileSync(logPath, line, { append: true });
  console.log(message);
}

function observe(label: string, value: unknown): void {
  const entry = { at: new Date().toISOString(), label, value };
  Deno.writeTextFileSync(
    observationPath,
    `${JSON.stringify(entry)}\n`,
    { append: true },
  );
  log(`${label}: ${JSON.stringify(value)}`);
}

async function preserveDevServerLog(label: string): Promise<{
  path: string;
  sha256: string;
}> {
  const artifactPath = path.join(ARTIFACT_ROOT, `${label}-dev-server.log`);
  await Deno.copyFile(DEV_SERVER_LOG_FILE, artifactPath);
  const digest = await crypto.subtle.digest(
    "SHA-256",
    await Deno.readFile(artifactPath),
  );
  return {
    path: artifactPath,
    sha256: Array.from(
      new Uint8Array(digest),
      (byte) => byte.toString(16).padStart(2, "0"),
    ).join(""),
  };
}

function errorSummary(error: unknown): unknown {
  if (error instanceof AggregateError) {
    return {
      name: error.name,
      message: error.message,
      errors: Array.from(error.errors, errorSummary),
    };
  }
  if (error instanceof Error) {
    return { name: error.name, message: error.message, stack: error.stack };
  }
  return String(error);
}

function childEnvironment(profileDir: string): Record<string, string> {
  return {
    ...Deno.env.toObject(),
    FLOORP_TEST_PROFILE_DIR: profileDir,
    FLOORP_DEV_RUNTIME_DISTRIBUTION: "release",
    FLOORP_SKIP_PATCHER: "1",
  };
}

async function runDevTool(
  profileDir: string,
  args: string[],
): Promise<string> {
  const command = new Deno.Command(Deno.execPath(), {
    args: ["task", "dev-tool", ...args],
    cwd: PROJECT_ROOT,
    env: childEnvironment(profileDir),
    stdout: "piped",
    stderr: "piped",
  });
  const result = await command.output();
  const stdout = decoder.decode(result.stdout);
  const stderr = decoder.decode(result.stderr);
  log(`command: deno task dev-tool ${args.join(" ")} [profile=${profileDir}]`);
  if (stdout) log(`stdout:\n${stdout.trimEnd()}`);
  if (stderr) log(`stderr:\n${stderr.trimEnd()}`);
  if (!result.success) {
    throw new Error(
      `dev-tool ${args[0]} failed (exit ${result.code}): ${stderr || stdout}`,
    );
  }
  return stdout;
}

async function pathExists(value: string): Promise<boolean> {
  try {
    await Deno.stat(value);
    return true;
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return false;
    throw error;
  }
}

async function snapshotDevToolState(): Promise<StateFileSnapshot[]> {
  return await Promise.all(DEV_TOOL_STATE_FILES.map(async (file) => {
    try {
      return { path: file, contents: await Deno.readTextFile(file) };
    } catch (error) {
      if (error instanceof Deno.errors.NotFound) {
        return { path: file, contents: null };
      }
      throw error;
    }
  }));
}

async function restoreDevToolState(
  snapshots: readonly StateFileSnapshot[],
): Promise<void> {
  await assertNoConflictingDevProcesses();
  const processes = await listDevProcesses();
  const liveTracked = readDevProcesses().filter((identity) =>
    processes.some((process) => matchesDevIdentity(process, identity))
  );
  assert(
    liveTracked.length === 0,
    `refusing to restore dev-tool state while recorded processes are live: ${
      liveTracked.map((identity) => identity.pid).join(",")
    }`,
  );
  for (const snapshot of snapshots) {
    if (snapshot.contents === null) {
      try {
        await Deno.remove(snapshot.path);
      } catch (error) {
        if (!(error instanceof Deno.errors.NotFound)) throw error;
      }
    } else {
      await Deno.writeTextFile(snapshot.path, snapshot.contents);
    }
  }
}

async function readHubRecord(profileDir: string): Promise<unknown | null> {
  const recordPath = path.join(profileDir, HUB_ROOT_NAME, "state.json");
  try {
    return JSON.parse(await Deno.readTextFile(recordPath)) as unknown;
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return null;
    throw error;
  }
}

function isInitialHubRecord(value: unknown): boolean {
  if (typeof value !== "object" || value === null) return false;
  const record = value as Record<string, unknown>;
  return record.featureId === "hub-panel-menu" &&
    record.selectedVersion === "hub-menu-v1" &&
    record.desiredEnabled === true && record.pending === null &&
    record.blockedReason === null;
}

async function waitFor<T>(
  read: () => Promise<T>,
  ready: (value: T) => boolean,
  description: string,
  timeoutMs = 180_000,
): Promise<T> {
  const deadline = Date.now() + timeoutMs;
  let last: T | undefined;
  while (Date.now() < deadline) {
    last = await read();
    if (ready(last)) return last;
    await new Promise((resolve) => setTimeout(resolve, 500));
  }
  throw new Error(
    `${description} timed out; last observation: ${JSON.stringify(last)}`,
  );
}

async function evaluateJson<T>(
  client: MarionetteClient,
  body: string,
): Promise<T> {
  const result: unknown = await client.executeScript(
    `return JSON.stringify((() => { ${body} })())`,
  );
  if (typeof result !== "string") {
    throw new Error(`browser observation was not JSON text: ${String(result)}`);
  }
  return JSON.parse(result) as T;
}

async function evaluateAsyncJson<T>(
  client: MarionetteClient,
  body: string,
): Promise<T> {
  const response: unknown = await client.send("WebDriver:ExecuteAsyncScript", {
    script: `
      const callback = arguments[arguments.length - 1];
      Promise.resolve((async () => { ${body} })()).then(
        (value) => callback(JSON.stringify({ value })),
        (error) => callback(JSON.stringify({
          error: error instanceof Error ? error.message : String(error),
        })),
      );
    `,
    args: [],
  });
  if (
    typeof response !== "object" || response === null ||
    !("value" in response) || typeof response.value !== "string"
  ) throw new Error("browser async observation was not JSON text");
  const payload = JSON.parse(response.value) as {
    error?: string;
    value?: unknown;
  };
  if (typeof payload.error === "string") throw new Error(payload.error);
  return payload.value as T;
}

function roleHelpers(): string {
  return `
    const roleKey = ${JSON.stringify(ROLE_KEY)};
    const privateUtils = ChromeUtils.importESModule(
      "resource://gre/modules/PrivateBrowsingUtils.sys.mjs"
    ).PrivateBrowsingUtils;
    const allBrowserWindows = () => {
      const windows = [];
      const enumerator = Services.wm.getEnumerator("navigator:browser");
      while (enumerator.hasMoreElements()) {
        const candidate = enumerator.getNext();
        if (!candidate.closed) windows.push(candidate);
      }
      return windows;
    };
    const findRoleWindow = (role) => {
      const windows = allBrowserWindows();
      const marked = windows.find((candidate) => candidate[roleKey] === role);
      if (marked) return marked;
      if (role === "initial") {
        const candidate = windows.find((item) =>
          !privateUtils.isWindowPrivate(item)
        );
        if (candidate) candidate[roleKey] = role;
        return candidate ?? null;
      }
      if (role === "private") {
        const candidate = windows.find((item) =>
          !item[roleKey] && privateUtils.isWindowPrivate(item)
        );
        if (candidate) candidate[roleKey] = role;
        return candidate ?? null;
      }
      if (role === "second-normal") {
        const candidate = windows.find((item) =>
          !item[roleKey] && !privateUtils.isWindowPrivate(item)
        );
        if (candidate) candidate[roleKey] = role;
        return candidate ?? null;
      }
      return null;
    };
  `;
}

async function startupSnapshot(
  client: MarionetteClient,
): Promise<StartupSnapshot | null> {
  return await evaluateJson<StartupSnapshot | null>(
    client,
    `
    ${roleHelpers()}
    const win = findRoleWindow("initial");
    if (!win) return null;
    const document = win.document;
    const trigger = document.getElementById("PanelUI-menu-button");
    return {
      delayedStartupFinished: Boolean(win.gBrowserInit?.delayedStartupFinished),
      mode: Services.prefs.getStringPref("nora.startup.mode", ""),
      selected: Services.prefs.getBoolPref("${SELECTED_PREF}", false),
      isPrivate: privateUtils.isWindowPrivate(win),
      windowType: document.documentElement?.getAttribute("windowtype") ?? "",
      popupOpen: trigger?.getAttribute("open") === "true",
      buttonPresent: Boolean(document.getElementById("${BUTTON_ID}")),
      settingsPresent: Boolean(
        document.getElementById("appMenu-settings-button")
      ),
    };
  `,
  );
}

async function registrySnapshot(
  client: MarionetteClient,
): Promise<RegistrySnapshot> {
  return await evaluateJson<RegistrySnapshot>(
    client,
    `
    const registryModule = ChromeUtils.importESModule(${
      JSON.stringify(REGISTRY_URI)
    });
    return registryModule.getSharedAppMenuRegistry().diagnosticsForTests;
  `,
  );
}

async function openMenu(
  client: MarionetteClient,
  role: string,
): Promise<void> {
  const result = await evaluateJson<{ opened: boolean }>(
    client,
    `
    ${roleHelpers()}
    const win = findRoleWindow(${JSON.stringify(role)});
    if (!win || !win.PanelUI || typeof win.PanelUI.show !== "function") {
      return { opened: false };
    }
    const panel = win.PanelUI.panel;
    if (!panel) return { opened: false };
    const state = {
      popupShown: panel.state === "open",
      showResolved: false,
      showError: null,
    };
    win[${JSON.stringify(PANEL_READY_KEY)}] = state;
    if (!state.popupShown) {
      panel.addEventListener("popupshown", () => { state.popupShown = true; }, {
        once: true,
      });
    }
    try {
      Promise.resolve(win.PanelUI.show()).then(
        () => { state.showResolved = true; },
        (error) => {
          state.showError = error instanceof Error ? error.message : String(error);
          state.showResolved = true;
        },
      );
    } catch (error) {
      state.showError = error instanceof Error ? error.message : String(error);
      state.showResolved = true;
    }
    return { opened: true };
  `,
  );
  assert(result.opened, `could not open app menu in ${role} window`);
  const readiness = await waitFor(
    () =>
      evaluateJson<
        {
          panelState: string | null;
          popupShown: boolean;
          showResolved: boolean;
          showError: string | null;
        } | null
      >(
        client,
        `
      ${roleHelpers()}
      const win = findRoleWindow(${JSON.stringify(role)});
      const state = win?.[${JSON.stringify(PANEL_READY_KEY)}];
      return win && state ? {
        panelState: win.PanelUI?.panel?.state ?? null,
        popupShown: state.popupShown,
        showResolved: state.showResolved,
        showError: state.showError,
      } : null;
    `,
      ),
    (value) =>
      value?.panelState === "open" && value.popupShown &&
      value.showResolved,
    `${role} PanelUI.show/popupshown readiness`,
    3_000,
  );
  assert(readiness !== null, `${role} app-menu readiness state disappeared`);
  assert(
    readiness.showError === null,
    `${role} PanelUI.show failed: ${readiness.showError}`,
  );
  await evaluateJson<boolean>(
    client,
    `
    ${roleHelpers()}
    const win = findRoleWindow(${JSON.stringify(role)});
    if (!win) return false;
    delete win[${JSON.stringify(PANEL_READY_KEY)}];
    return true;
  `,
  );
}

async function menuSnapshot(
  client: MarionetteClient,
  role: string,
): Promise<MenuSnapshot | null> {
  return await evaluateJson<MenuSnapshot | null>(
    client,
    `
    ${roleHelpers()}
    const win = findRoleWindow(${JSON.stringify(role)});
    if (!win) return null;
    const document = win.document;
    const trigger = document.getElementById("PanelUI-menu-button");
    const settings = document.getElementById("appMenu-settings-button");
    const button = document.getElementById("${BUTTON_ID}");
    return {
      role: ${JSON.stringify(role)},
      isPrivate: privateUtils.isWindowPrivate(win),
      delayedStartupFinished: Boolean(win.gBrowserInit?.delayedStartupFinished),
      popupOpen: trigger?.getAttribute("open") === "true",
      settingsPresent: Boolean(settings),
      buttonMatchCount: document.querySelectorAll("#${BUTTON_ID}").length,
      button: button ? {
        id: button.id,
        tag: button.localName,
        className: button.getAttribute("class") ?? "",
        label: button.getAttribute("label") ?? "",
        disabled: typeof button.disabled === "boolean" ? button.disabled : null,
        immediateSettingsSibling: settings?.nextElementSibling === button,
        matchCount: document.querySelectorAll("#${BUTTON_ID}").length,
      } : null,
    };
  `,
  );
}

function assertMenu(
  snapshot: MenuSnapshot | null,
  role: string,
  isPrivate: boolean,
): asserts snapshot is MenuSnapshot & {
  button: NonNullable<MenuSnapshot["button"]>;
} {
  assert(snapshot !== null, `${role} browser window was not found`);
  assert(snapshot.delayedStartupFinished, `${role} startup is incomplete`);
  assert(snapshot.isPrivate === isPrivate, `${role} privacy mode mismatch`);
  assert(snapshot.popupOpen, `${role} app menu did not open`);
  assert(snapshot.settingsPresent, `${role} Settings button is missing`);
  assert(snapshot.button !== null, `${role} HUB menu button is missing`);
  assert(snapshot.button.id === BUTTON_ID, `${role} button ID mismatch`);
  assert(
    snapshot.button.tag === "toolbarbutton",
    `${role} button is not XUL toolbarbutton`,
  );
  assert(
    snapshot.button.className === "subviewbutton",
    `${role} button class mismatch: ${snapshot.button.className}`,
  );
  assert(
    snapshot.button.immediateSettingsSibling,
    `${role} HUB button is not immediately after Settings`,
  );
  assert(snapshot.button.label.trim().length > 0, `${role} label is blank`);
  assert(
    typeof snapshot.button.disabled === "boolean" &&
      !snapshot.button.disabled,
    `${role} XUL disabled property is not boolean false`,
  );
  assert(snapshot.button.matchCount === 1, `${role} button is duplicated`);
  assert(
    snapshot.buttonMatchCount === 1,
    `${role} has duplicate HUB button IDs`,
  );
}

function assertMenuAbsent(
  snapshot: MenuSnapshot | null,
  role: string,
  isPrivate: boolean,
): asserts snapshot is MenuSnapshot {
  assert(snapshot !== null, `${role} browser window was not found`);
  assert(snapshot.delayedStartupFinished, `${role} startup is incomplete`);
  assert(snapshot.isPrivate === isPrivate, `${role} privacy mode mismatch`);
  assert(snapshot.popupOpen, `${role} app menu did not open`);
  assert(
    snapshot.button === null,
    `${role} unexpectedly rendered a HUB menu item`,
  );
  assert(
    snapshot.buttonMatchCount === 0,
    `${role} has a HUB button ID in disabled state`,
  );
}

async function beginCanonicalHostAcquisition(
  client: MarionetteClient,
): Promise<void> {
  const result = await evaluateJson<{ started: boolean }>(
    client,
    `
    ${roleHelpers()}
    const hostKey = ${JSON.stringify(HOST_KEY)};
    const storage = findRoleWindow("initial");
    if (!storage || storage[hostKey]) return { started: false };
    const state = { ready: false, error: null, host: null };
    storage[hostKey] = state;
    try {
      const hostModule = ChromeUtils.importESModule(${
      JSON.stringify(FEATURE_HOST_URI)
    });
      Promise.resolve(hostModule.M0FeatureHost.openHubPanelMenu(true)).then(
        (host) => { state.host = host; state.ready = true; },
        (error) => {
          state.error = error instanceof Error ? error.message : String(error);
          state.ready = true;
        },
      );
      return { started: true };
    } catch (error) {
      state.error = error instanceof Error ? error.message : String(error);
      state.ready = true;
      return { started: true };
    }
  `,
  );
  assert(result.started, "canonical host acquisition was already started");
}

async function beginIsolatedHubHostAcquisition(
  client: MarionetteClient,
): Promise<{ root: string }> {
  const result = await evaluateJson<{ started: boolean; root: string | null }>(
    client,
    `
    ${roleHelpers()}
    const win = findRoleWindow("initial");
    const hostKey = ${JSON.stringify(HOST_KEY)};
    if (!win || win[hostKey]) return { started: false, root: null };
    const suffix = Services.uuid.generateUUID().toString()
      .replace(/[{}]/g, "").toLowerCase();
    const rootFile = Services.dirsvc.get("ProfD", Ci.nsIFile).clone();
    rootFile.append("floorp-hotfix-m0-tests");
    rootFile.append("feature-" + suffix);
    const root = rootFile.path;
    const state = { ready: false, error: null, host: null, root };
    win[hostKey] = state;
    try {
      const hostModule = ChromeUtils.importESModule(
        ${JSON.stringify(FEATURE_HOST_URI)}
      );
      Promise.resolve(
        hostModule.M0FeatureHost.openHubPanelMenuForTests(root, false)
      ).then(
        (host) => { state.host = host; state.ready = true; },
        (error) => {
          state.error = error instanceof Error ? error.message : String(error);
          state.ready = true;
        },
      );
      return { started: true, root };
    } catch (error) {
      state.error = error instanceof Error ? error.message : String(error);
      state.ready = true;
      return { started: true, root };
    }
  `,
  );
  assert(
    result.started && result.root !== null,
    "isolated HUB host acquisition could not start",
  );
  return { root: result.root };
}

async function hostShutdownSnapshot(
  client: MarionetteClient,
): Promise<HostShutdownSnapshot | null> {
  return await evaluateJson<HostShutdownSnapshot | null>(
    client,
    `
    ${roleHelpers()}
    const win = findRoleWindow("initial");
    const state = win?.[${JSON.stringify(HOST_SHUTDOWN_KEY)}];
    return state?.ready ? state : null;
  `,
  );
}

async function shutdownHostForLifecyclePhase(
  client: MarionetteClient,
  label: string,
): Promise<HostShutdownSnapshot> {
  const started = await evaluateJson<{ started: boolean }>(
    client,
    `
    ${roleHelpers()}
    const win = findRoleWindow("initial");
    const state = win?.[${JSON.stringify(HOST_KEY)}];
    if (!win || !state?.ready || state.error || !state.host) {
      return { started: false };
    }
    const host = state.host;
    const registryModule = ChromeUtils.importESModule(
      ${JSON.stringify(REGISTRY_URI)}
    );
    const shutdown = {
      ready: false,
      error: null,
      root: host.root,
      errors: [],
      timedOut: false,
      windowIds: [],
      windowLifecycle: null,
      scope: null,
      registryBefore:
        registryModule.getSharedAppMenuRegistry().diagnosticsForTests,
      registry: null,
    };
    win[${JSON.stringify(HOST_SHUTDOWN_KEY)}] = shutdown;
    Promise.resolve(host.shutdown()).then(
      (report) => {
        const registryModule = ChromeUtils.importESModule(
          ${JSON.stringify(REGISTRY_URI)}
        );
        shutdown.errors = report.errors.map((error) =>
          error instanceof Error ? error.message : String(error)
        );
        shutdown.timedOut = report.timedOut;
        shutdown.windowIds = host.windowIdsForTests;
        shutdown.windowLifecycle = host.windowLifecycleDiagnosticsForTests;
        shutdown.scope = host.runningScopeDiagnosticsForTests;
        shutdown.registry =
          registryModule.getSharedAppMenuRegistry().diagnosticsForTests;
        shutdown.ready = true;
      },
      (error) => {
        shutdown.error = error instanceof Error ? error.message : String(error);
        shutdown.ready = true;
      },
    );
    return { started: true };
  `,
  );
  assert(started.started, `${label} host shutdown did not start`);
  const snapshot = await waitFor(
    () => hostShutdownSnapshot(client),
    (value) => value?.ready === true,
    `${label} host shutdown`,
  );
  assert(snapshot !== null, `${label} host shutdown state was not observed`);
  observe(`${label}.host-shutdown`, snapshot);
  assert(
    snapshot.error === null && snapshot.errors.length === 0 &&
      !snapshot.timedOut,
    `${label} host shutdown reported cleanup errors`,
  );
  assert(
    snapshot.windowIds.length === 0 &&
      !snapshot.windowLifecycle.observers &&
      snapshot.windowLifecycle.tracked === 0 &&
      snapshot.windowLifecycle.pendingStartup === 0 &&
      snapshot.windowLifecycle.readyWaiters === 0 &&
      snapshot.windowLifecycle.admissionTasks === 0 &&
      snapshot.windowLifecycle.pendingDetaches === 0 &&
      snapshot.windowLifecycle.cleanupFailures === 0,
    `${label} host window cleanup was incomplete`,
  );
  assert(
    snapshot.scope.resources === 0 && snapshot.scope.childScopes === 0 &&
      snapshot.scope.tasks === 0 && snapshot.scope.pendingTasks === 0 &&
      snapshot.scope.observedTaskRejections === 0 &&
      snapshot.scope.taskFailures === 0,
    `${label} host scope cleanup was incomplete`,
  );
  assert(
    snapshot.registry.connections === snapshot.registryBefore.connections &&
      !snapshot.registry.staged && !snapshot.registry.published &&
      !snapshot.registry.enabled &&
      snapshot.registry.pendingAcknowledgements === 0,
    `${label} host did not release its AppMenuRegistry lease cleanly`,
  );
  const cleared = await evaluateJson<boolean>(
    client,
    `
    ${roleHelpers()}
    const win = findRoleWindow("initial");
    const state = win?.[${JSON.stringify(HOST_KEY)}];
    if (!win || !state?.host || !state.host.root) return false;
    delete win[${JSON.stringify(HOST_KEY)}];
    delete win[${JSON.stringify(HOST_SHUTDOWN_KEY)}];
    return true;
  `,
  );
  assert(cleared, `${label} host state was not cleared after shutdown`);
  return snapshot;
}

async function hostSnapshot(
  client: MarionetteClient,
): Promise<HostSnapshot | null> {
  return await evaluateJson<HostSnapshot | null>(
    client,
    `
    ${roleHelpers()}
    const state = findRoleWindow("initial")?.[${JSON.stringify(HOST_KEY)}];
    if (!state) return null;
    if (!state.ready) {
      return { ready: false, error: null, diagnostics: null, windowIds: [], trackedWindows: 0 };
    }
    if (state.error) {
      return { ready: true, error: state.error, diagnostics: null, windowIds: [], trackedWindows: 0 };
    }
    const host = state.host;
    const diagnostics = host.diagnostics;
    return {
      ready: true,
      error: null,
      diagnostics: {
        featureId: diagnostics.featureId,
        desiredEnabled: diagnostics.desiredEnabled,
        selectedVersion: diagnostics.selectedVersion,
        rollbackVersion: diagnostics.rollbackVersion,
        stateRevision: diagnostics.stateRevision,
        blockedReason: diagnostics.blockedReason,
        lastOutcome: diagnostics.lastOutcome
          ? { txId: diagnostics.lastOutcome.txId, status: diagnostics.lastOutcome.status }
          : null,
      },
      windowIds: host.windowIdsForTests,
      trackedWindows: host.windowIdsForTests.length,
    };
  `,
  );
}

async function startHostOperation(
  client: MarionetteClient,
  operation: "update" | "rollback" | "disable" | "enable",
  target: string | null,
): Promise<{ txId: string; expectedRevision: number }> {
  return await evaluateJson<{ txId: string; expectedRevision: number }>(
    client,
    `
    ${roleHelpers()}
    const storage = findRoleWindow("initial");
    const hostState = storage?.[${JSON.stringify(HOST_KEY)}];
    if (!hostState?.ready || hostState.error || !hostState.host) {
      throw new Error("canonical HUB host is unavailable");
    }
    const host = hostState.host;
    const before = host.diagnostics;
    const txId = Services.uuid.generateUUID().toString().replace(/[{}]/g, "");
    const expectedRevision = before.stateRevision;
    const state = {
      done: false,
      error: null,
      txId,
      expectedRevision,
      operation: ${JSON.stringify(operation)},
      target: ${JSON.stringify(target)},
      before,
      outcome: null,
      queriedOutcome: null,
      after: null,
    };
    storage[${JSON.stringify(OPERATION_KEY)}] = state;
    let result;
    try {
      result = ${JSON.stringify(operation)} === "update"
        ? host.update(txId, expectedRevision, ${JSON.stringify(target)})
        : ${JSON.stringify(operation)} === "rollback"
        ? host.rollback(txId, expectedRevision)
        : ${JSON.stringify(operation)} === "disable"
        ? host.disable(txId, expectedRevision)
        : host.enable(txId, expectedRevision);
    } catch (error) {
      state.error = error instanceof Error ? error.message : String(error);
      state.done = true;
      return { txId, expectedRevision };
    }
    Promise.resolve(result).then(
      (outcome) => {
        state.outcome = outcome;
        state.queriedOutcome = host.queryOutcome(txId);
        state.after = host.diagnostics;
        state.done = true;
      },
      (error) => {
        state.error = error instanceof Error ? error.message : String(error);
        state.after = host.diagnostics;
        state.done = true;
      },
    );
    return { txId, expectedRevision };
  `,
  );
}

async function hostOperationSnapshot(
  client: MarionetteClient,
): Promise<HostOperationSnapshot | null> {
  return await evaluateJson<HostOperationSnapshot | null>(
    client,
    `
    ${roleHelpers()}
    const state = findRoleWindow("initial")?.[${JSON.stringify(OPERATION_KEY)}];
    if (!state) return null;
    const record = (value) => value ? ({
      featureId: value.featureId,
      desiredEnabled: value.desiredEnabled,
      selectedVersion: value.selectedVersion,
      rollbackVersion: value.rollbackVersion,
      stateRevision: value.stateRevision,
      blockedReason: value.blockedReason,
      lastOutcome: value.lastOutcome
        ? { txId: value.lastOutcome.txId, status: value.lastOutcome.status }
        : null,
    }) : null;
    return {
      done: state.done,
      error: state.error,
      txId: state.txId,
      expectedRevision: state.expectedRevision,
      operation: state.operation,
      target: state.target,
      outcome: state.outcome,
      queriedOutcome: state.queriedOutcome,
      before: record(state.before),
      after: record(state.after),
    };
  `,
  );
}

function commandProbeHelpers(): string {
  return `
    const probeKey = ${JSON.stringify(PROBE_KEY)};
    const retainedButtonKey = ${JSON.stringify(RETAINED_BUTTON_KEY)};
    function installCommandProbe(win) {
      const probe = {
        calls: [],
        hideCalls: 0,
        addInstalled: false,
        hideInstalled: false,
        addDescriptor: Object.getOwnPropertyDescriptor(win.gBrowser, "addTab"),
        hideDescriptor: Object.getOwnPropertyDescriptor(win.PanelUI, "hide"),
        originalAddTab: win.gBrowser.addTab,
        originalHide: win.PanelUI.hide,
      };
      win[probeKey] = probe;
      const addWrapper = function(url, options) {
        const call = { url, options, tab: null };
        probe.calls.push(call);
        call.tab = probe.originalAddTab.call(this, url, options);
        return call.tab;
      };
      Object.defineProperty(win.gBrowser, "addTab", {
        configurable: true, writable: true, value: addWrapper,
      });
      probe.addInstalled = true;
      const hideWrapper = function(...args) {
        probe.hideCalls += 1;
        return probe.originalHide.apply(this, args);
      };
      Object.defineProperty(win.PanelUI, "hide", {
        configurable: true, writable: true, value: hideWrapper,
      });
      probe.hideInstalled = true;
      return probe;
    }
    function probeSnapshot(win, role) {
      const probe = win[probeKey];
      if (!probe) return null;
      return {
        role,
        tabCount: win.gBrowser.tabs.length,
        popupOpen: win.document.getElementById("PanelUI-menu-button")?.getAttribute("open") === "true",
        hideCalls: probe.hideCalls,
        calls: probe.calls.map((call) => {
          const browser = call.tab ? win.gBrowser.getBrowserForTab(call.tab) : null;
          const principal = call.options?.triggeringPrincipal ?? null;
          return {
            url: call.url,
            relatedToCurrent: call.options?.relatedToCurrent === true,
            systemPrincipal: Boolean(principal && principal === Services.scriptSecurityManager.getSystemPrincipal()),
            hasReturnedTab: Boolean(call.tab),
            returnedTabSelected: Boolean(call.tab && win.gBrowser.selectedTab === call.tab),
            currentUrl: browser?.currentURI?.spec ?? "",
          };
        }),
        retainedConnected: Boolean(win[retainedButtonKey]?.isConnected),
      };
    }
    function finishCommandProbe(win) {
      const probe = win[probeKey];
      if (!probe) return { removedOwnedTabs: 0, errors: [] };
      const errors = [];
      let removedOwnedTabs = 0;
      try {
        if (probe.addInstalled) {
          if (probe.addDescriptor) Object.defineProperty(win.gBrowser, "addTab", probe.addDescriptor);
          else delete win.gBrowser.addTab;
        }
      } catch (error) { errors.push(String(error)); }
      try {
        if (probe.hideInstalled) {
          if (probe.hideDescriptor) Object.defineProperty(win.PanelUI, "hide", probe.hideDescriptor);
          else delete win.PanelUI.hide;
        }
      } catch (error) { errors.push(String(error)); }
      for (const call of probe.calls) {
        if (!call.tab) continue;
        try {
          if ([...win.gBrowser.tabs].includes(call.tab)) {
            win.gBrowser.removeTab(call.tab, { animate: false });
            removedOwnedTabs += 1;
          }
        } catch (error) { errors.push(String(error)); }
      }
      delete win[probeKey];
      return { removedOwnedTabs, errors };
    }
  `;
}

async function beginCommandProbe(
  client: MarionetteClient,
  role: string,
  retained: boolean,
): Promise<{ started: boolean; error: string | null }> {
  return await evaluateJson<{ started: boolean; error: string | null }>(
    client,
    `
    ${roleHelpers()}
    ${commandProbeHelpers()}
    const win = findRoleWindow(${JSON.stringify(role)});
    if (!win || !win.PanelUI || typeof win.PanelUI.hide !== "function") {
      return { started: false, error: "window or PanelUI.hide is missing" };
    }
    const button = ${
      retained
        ? `win[${JSON.stringify(RETAINED_BUTTON_KEY)}]`
        : `win.document.getElementById(${JSON.stringify(BUTTON_ID)})`
    };
    if (!button || typeof button.doCommand !== "function") {
      return { started: false, error: "button or doCommand is missing" };
    }
    let probe;
    try {
      probe = installCommandProbe(win);
      button.doCommand();
      return { started: true, error: null };
    } catch (error) {
      return { started: Boolean(probe), error: error instanceof Error ? error.message : String(error) };
    }
  `,
  );
}

async function commandProbeSnapshot(
  client: MarionetteClient,
  role: string,
): Promise<CommandProbeSnapshot | null> {
  return await evaluateJson<CommandProbeSnapshot | null>(
    client,
    `
    ${roleHelpers()}
    ${commandProbeHelpers()}
    const win = findRoleWindow(${JSON.stringify(role)});
    return win ? probeSnapshot(win, ${JSON.stringify(role)}) : null;
  `,
  );
}

async function finishCommandProbe(
  client: MarionetteClient,
  role: string,
): Promise<{ removedOwnedTabs: number; errors: string[] }> {
  return await evaluateJson<{ removedOwnedTabs: number; errors: string[] }>(
    client,
    `
    ${roleHelpers()}
    ${commandProbeHelpers()}
    const win = findRoleWindow(${JSON.stringify(role)});
    return win ? finishCommandProbe(win) : { removedOwnedTabs: 0, errors: ["window closed during command probe"] };
  `,
  );
}

async function roleTabCounts(
  client: MarionetteClient,
  roles: readonly string[],
): Promise<Record<string, number>> {
  return await evaluateJson<Record<string, number>>(
    client,
    `
    ${roleHelpers()}
    const counts = {};
    for (const role of ${JSON.stringify(roles)}) {
      const win = findRoleWindow(role);
      counts[role] = win?.gBrowser?.tabs?.length ?? -1;
    }
    return counts;
  `,
  );
}

async function runLifecycleCase(
  name: string,
  verdicts: string[],
  run: () => Promise<unknown>,
): Promise<void> {
  try {
    const detail = await run();
    verdicts.push(name);
    observe(`lifecycle.${name}.verdict`, { status: "PASS", detail });
    log(`[PASS] lifecycle.${name}`);
  } catch (error) {
    observe(`lifecycle.${name}.verdict`, {
      status: "FAIL",
      error: error instanceof Error ? error.message : String(error),
      stack: error instanceof Error ? error.stack : null,
      details: errorSummary(error),
    });
    throw error;
  }
}

async function withCommandProbe<T>(
  client: MarionetteClient,
  role: string,
  retained: boolean,
  caseName: string,
  run: () => Promise<T>,
): Promise<T> {
  let result: T | undefined;
  let completed = false;
  let operationError: unknown;
  let operationFailed = false;
  let cleanupError: unknown;
  let cleanupFailed = false;
  try {
    const started = await beginCommandProbe(client, role, retained);
    assert(
      started.started,
      `${caseName} ${role} command probe could not start: ${started.error}`,
    );
    assert(
      started.error === null,
      `${caseName} ${role} command threw: ${started.error}`,
    );
    result = await run();
    completed = true;
  } catch (error) {
    operationError = error;
    operationFailed = true;
  } finally {
    try {
      const cleanup = await finishCommandProbe(client, role);
      observe(`lifecycle.command-cleanup.${caseName}.${role}`, cleanup);
      if (cleanup.errors.length > 0) {
        cleanupError = new AggregateError(
          cleanup.errors.map((error) => new Error(error)),
          `${caseName} ${role} command cleanup failed`,
        );
        cleanupFailed = true;
      }
    } catch (error) {
      cleanupError = error;
      cleanupFailed = true;
    }
  }
  if (operationFailed && cleanupFailed) {
    throw new AggregateError(
      [operationError, cleanupError],
      `${caseName} ${role} command and cleanup both failed`,
    );
  }
  if (operationFailed) throw operationError;
  if (cleanupFailed) throw cleanupError;
  assert(completed, `${caseName} ${role} command probe did not complete`);
  return result as T;
}

async function verifyRealButtonCommand(
  client: MarionetteClient,
  role: string,
  roles: readonly string[],
): Promise<unknown> {
  const before = await roleTabCounts(client, roles);
  return await withCommandProbe(
    client,
    role,
    false,
    "button-command",
    async () => {
      const result = await waitFor(
        () => commandProbeSnapshot(client, role),
        (value) =>
          Boolean(
            value?.calls.length === 1 &&
              isHubDocumentUrl(value.calls[0]?.currentUrl ?? "") &&
              value.calls[0]?.returnedTabSelected && value.hideCalls > 0 &&
              !value.popupOpen,
          ),
        `${role} actual HUB button command`,
      );
      assert(result !== null, `${role} command probe disappeared`);
      assert(
        result.calls.length === 1,
        `${role} command added unexpected tabs`,
      );
      assert(
        result.calls[0]?.url === "about:hub",
        `${role} command URL mismatch`,
      );
      assert(
        result.calls[0]?.relatedToCurrent,
        `${role} addTab omitted relatedToCurrent`,
      );
      assert(
        result.calls[0]?.systemPrincipal,
        `${role} addTab omitted system principal`,
      );
      assert(
        result.calls[0]?.hasReturnedTab,
        `${role} addTab returned no owned tab`,
      );
      assert(
        result.calls[0]?.returnedTabSelected,
        `${role} did not select the returned tab`,
      );
      const after = await roleTabCounts(client, roles);
      for (const otherRole of roles) {
        assert(
          after[otherRole] ===
            (before[otherRole] ?? -1) + (otherRole === role ? 1 : 0),
          `${role} command changed tabs in ${otherRole}`,
        );
      }
      return { before, command: result, after };
    },
  );
}

async function captureAndCloseNormalMenus(
  client: MarionetteClient,
  caseName: string,
): Promise<void> {
  for (const role of ["initial", "second-normal"]) {
    await openMenu(client, role);
    const menu = await waitFor(
      () => menuSnapshot(client, role),
      (value) =>
        value?.popupOpen === true &&
        (caseName === "re-enable" || value.button !== null),
      `${caseName} ${role} pre-operation open menu`,
    );
    assert(menu !== null, `${caseName} ${role} menu disappeared`);
    if (menu.button) {
      assertMenu(menu, role, false);
      const captured = await evaluateJson<{ captured: boolean }>(
        client,
        `
      ${roleHelpers()}
      const win = findRoleWindow(${JSON.stringify(role)});
      const button = win?.document.getElementById(${JSON.stringify(BUTTON_ID)});
      if (!win || !button) return { captured: false };
      win[${JSON.stringify(RETAINED_BUTTON_KEY)}] = button;
      win.PanelUI.hide();
      return { captured: true };
    `,
      );
      assert(captured.captured, `${caseName} could not retain ${role} button`);
    } else {
      const hidden = await evaluateJson<boolean>(
        client,
        `
        ${roleHelpers()}
        const win = findRoleWindow(${JSON.stringify(role)});
        if (!win) return false;
        win.PanelUI.hide();
        return true;
      `,
      );
      assert(hidden, `${caseName} could not close ${role} menu`);
    }
    const closed = await waitFor(
      () => menuSnapshot(client, role),
      (value) => value?.popupOpen === false,
      `${caseName} ${role} closed-menu state`,
    );
    observe(`lifecycle.${caseName}.${role}.closed-menu-before`, closed);
  }
}

async function verifyDetachedButton(
  client: MarionetteClient,
  role: string,
  caseName: string,
): Promise<void> {
  await withCommandProbe(
    client,
    role,
    true,
    `${caseName}.detached-button`,
    async () => {
      await new Promise((resolve) => setTimeout(resolve, 500));
      const snapshot = await commandProbeSnapshot(client, role);
      assert(snapshot !== null, `${caseName} stale button probe disappeared`);
      assert(
        !snapshot.retainedConnected,
        `${caseName} retained button is still connected`,
      );
      assert(
        snapshot.calls.length === 0,
        `${caseName} detached ${role} button opened a tab`,
      );
      assert(
        snapshot.hideCalls === 0,
        `${caseName} detached ${role} button hid the menu`,
      );
      observe(`lifecycle.${caseName}.${role}.detached-button`, snapshot);
    },
  );
}

async function focusSnapshot(
  client: MarionetteClient,
  role: string,
): Promise<FocusSnapshot | null> {
  return await evaluateJson<FocusSnapshot | null>(
    client,
    `
    ${roleHelpers()}
    const win = findRoleWindow(${JSON.stringify(role)});
    if (!win) return null;
    const document = win.document;
    const button = document.getElementById("${BUTTON_ID}");
    const settings = document.getElementById("appMenu-settings-button");
    const trigger = document.getElementById("PanelUI-menu-button");
    const visible = (element) => {
      if (!element?.isConnected || element.disabled) return false;
      const style = win.getComputedStyle(element);
      return style.display !== "none" && style.visibility !== "hidden" &&
        element.getClientRects().length > 0;
    };
    const active = document.activeElement;
    const dispatcherFocused = document.commandDispatcher?.focusedElement ?? null;
    const serviceFocused = Services.focus.focusedElement ?? null;
    return {
      role: ${JSON.stringify(role)},
      windowFocused: document.hasFocus(),
      activeWindowIsTarget: Services.focus.activeWindow === win,
      activeWindowTitle: Services.focus.activeWindow?.document?.title ?? "",
      popupOpen: trigger?.getAttribute("open") === "true",
      activeElementId: active?.id ?? "",
      activeElementTag: active?.localName ?? "",
      activeButton: active === button,
      buttonMatchesFocusPseudo: button?.matches(":focus") ?? false,
      buttonTabIndex: button && typeof button.tabIndex === "number"
        ? button.tabIndex
        : null,
      buttonTabIndexAttribute: button?.getAttribute("tabindex") ?? null,
      dispatcherFocusedId: dispatcherFocused?.id ?? "",
      dispatcherFocusedTag: dispatcherFocused?.localName ?? "",
      serviceFocusedId: serviceFocused?.id ?? "",
      serviceFocusedTag: serviceFocused?.localName ?? "",
      activeSettings: active === settings,
      activeTrigger: active === trigger,
      buttonMatchCount: document.querySelectorAll("#${BUTTON_ID}").length,
      buttonEnabled: button && typeof button.disabled === "boolean"
        ? button.disabled === false
        : null,
      settingsVisible: visible(settings),
      triggerVisible: visible(trigger),
    };
  `,
  );
}

async function sendNativeTab(
  client: MarionetteClient,
  reverse = false,
): Promise<void> {
  let actionError: unknown = null;
  try {
    await client.send("WebDriver:PerformActions", {
      actions: [{
        type: "key",
        id: "hub-menu-focus-navigation",
        actions: [
          ...(reverse ? [{ type: "keyDown", value: "\uE008" }] : []),
          { type: "keyDown", value: "\uE004" },
          { type: "keyUp", value: "\uE004" },
          ...(reverse ? [{ type: "keyUp", value: "\uE008" }] : []),
        ],
      }],
    });
  } catch (error) {
    actionError = error;
  }
  try {
    await client.send("WebDriver:ReleaseActions", {});
  } catch (error) {
    if (actionError !== null) {
      throw new AggregateError(
        [actionError, error],
        "Tab action cleanup failed",
      );
    }
    throw error;
  }
  if (actionError !== null) throw actionError;
}

async function focusHubButton(
  client: MarionetteClient,
  role: string,
  profileDir: string,
  focusEntry = true,
): Promise<FocusSnapshot> {
  const initial = await focusSnapshot(client, role);
  observe(`lifecycle.focus.${role}.starting-state`, initial);
  assert(initial !== null, `${role} browser window is missing`);
  await evaluateJson<boolean>(
    client,
    `
    ${roleHelpers()}
    const win = findRoleWindow(${JSON.stringify(role)});
    if (!win) return false;
    win.focus();
    return true;
  `,
  );
  let foreground: FocusSnapshot | null = null;
  let windowFocusWaitError: string | null = null;
  try {
    foreground = await waitFor(
      () => focusSnapshot(client, role),
      (value) => value?.windowFocused === true,
      `${role} window.focus foreground`,
      3_000,
    );
  } catch (error) {
    windowFocusWaitError = String(error);
  }
  let activation: unknown = null;
  if (foreground === null) {
    const exactBrowserProcesses = (await listDevProcesses()).filter(
      (process) =>
        /floorp/i.test(process.name) &&
        hasExactProfileArgument(process, profileDir),
    );
    const process = exactBrowserProcesses.length === 1
      ? exactBrowserProcesses[0]
      : null;
    assert(
      process !== null,
      `${role} foreground activation could not identify one exact-profile Floorp PID: ${
        JSON.stringify(exactBrowserProcesses)
      }`,
    );
    const listed = await new Deno.Command("wmctrl", {
      args: ["-lp"],
      stdout: "piped",
      stderr: "piped",
    }).output();
    const nativeWindows = decoder.decode(listed.stdout).split("\n").flatMap(
      (line) => {
        const [windowId, _desktop, pidText, ...titleParts] = line.trim()
          .split(/\s+/);
        const pid = Number(pidText);
        return windowId && Number.isInteger(pid) && pid === process.pid
          ? [{ windowId, pid, title: titleParts.join(" ") }]
          : [];
      },
    );
    observe(`lifecycle.focus.${role}.wm-candidates`, {
      exactProfilePid: process.pid,
      nativeWindows,
      wmctrlExitCode: listed.code,
      stderr: decoder.decode(listed.stderr),
    });
    const attempts: { windowId: string; activated: boolean; focus: unknown }[] =
      [];
    for (const nativeWindow of nativeWindows) {
      const result = await new Deno.Command("wmctrl", {
        args: ["-ia", nativeWindow.windowId],
        stdout: "piped",
        stderr: "piped",
      }).output();
      if (result.code !== 0) {
        attempts.push({
          windowId: nativeWindow.windowId,
          activated: false,
          focus: decoder.decode(result.stderr),
        });
        continue;
      }
      try {
        foreground = await waitFor(
          () => focusSnapshot(client, role),
          (value) => value?.windowFocused === true,
          `${role} exact-owned WM window activation`,
          3_000,
        );
        attempts.push({
          windowId: nativeWindow.windowId,
          activated: true,
          focus: foreground,
        });
        break;
      } catch (error) {
        attempts.push({
          windowId: nativeWindow.windowId,
          activated: true,
          focus: String(error),
        });
      }
    }
    activation = { exactProfilePid: process.pid, attempts };
  }
  observe(`lifecycle.focus.${role}.foreground-result`, {
    focus: foreground,
    windowFocusWaitError,
    nativeActivation: activation,
  });
  assert(
    foreground?.windowFocused === true,
    `${role} did not become the real foreground window`,
  );

  await openMenu(client, role);
  const readyMenu = await waitFor(
    () => menuSnapshot(client, role),
    (value) => value?.popupOpen === true && value.button !== null,
    `${role} popup and HUB button readiness`,
    3_000,
  );
  assertMenu(readyMenu, role, false);
  if (!focusEntry) {
    const menuState = await focusSnapshot(client, role);
    observe(`lifecycle.focus.${role}.menu-ready`, menuState);
    assert(
      menuState !== null && menuState.windowFocused && menuState.popupOpen,
      `${role} menu did not remain foreground and open`,
    );
    return menuState;
  }
  const focusDeadline = Date.now() + 3_000;
  let tabCount = 0;
  let focused = await focusSnapshot(client, role);
  while (
    Date.now() < focusDeadline && tabCount < 24 &&
    !(focused?.activeButton && focused.buttonMatchCount === 1)
  ) {
    assert(
      focused?.windowFocused === true && focused.popupOpen,
      `${role} lost foreground/open-menu state during keyboard navigation`,
    );
    await sendNativeTab(client);
    tabCount++;
    focused = await focusSnapshot(client, role);
  }
  observe(`lifecycle.focus.${role}.native-keyboard-navigation`, {
    tabCount,
    focused,
  });
  observe(`lifecycle.focus.${role}.hub-entry-focused`, focused);
  assert(focused !== null, `${role} HUB focus state was not observed`);
  assert(focused.popupOpen, `${role} app menu is not open`);
  assert(focused.activeButton, `${role} HUB entry did not receive focus`);
  assert(focused.buttonEnabled, `${role} HUB entry is not enabled`);
  assert(focused.buttonMatchCount === 1, `${role} HUB entry is duplicated`);
  return focused;
}

async function retainCurrentButtons(
  client: MarionetteClient,
  roles: readonly string[],
): Promise<void> {
  const retained = await evaluateJson<{ role: string; retained: boolean }[]>(
    client,
    `
    ${roleHelpers()}
    const retainedKey = ${JSON.stringify(RETAINED_BUTTON_KEY)};
    return ${JSON.stringify(roles)}.map((role) => {
      const win = findRoleWindow(role);
      const button = win?.document.getElementById("${BUTTON_ID}");
      if (win && button) win[retainedKey] = button;
      return { role, retained: Boolean(button) };
    });
  `,
  );
  for (const item of retained) {
    assert(item.retained, `could not retain ${item.role} HUB button`);
  }
}

async function focusedMenuItemSnapshot(
  client: MarionetteClient,
  role: string,
  itemId?: string,
): Promise<FocusMovementItemSnapshot | null> {
  const targetExpression = itemId === undefined
    ? "document?.activeElement"
    : `document?.getElementById(${JSON.stringify(itemId)})`;
  return await evaluateJson<FocusMovementItemSnapshot | null>(
    client,
    `
    ${roleHelpers()}
    const win = findRoleWindow(${JSON.stringify(role)});
    const document = win?.document;
    const target = ${targetExpression};
    const panelView = document?.getElementById("appMenu-mainView");
    if (!win || !document || !target || !panelView) return null;
    const style = win.getComputedStyle(target);
    return {
      id: target.id,
      tag: target.localName,
      active: document.activeElement === target,
      connected: target.isConnected,
      enabled: typeof target.disabled === "boolean" ? !target.disabled : true,
      insideMenu: panelView.contains(target),
      visible: style.display !== "none" && style.visibility !== "hidden" &&
        target.getClientRects().length > 0,
      tabIndex: target.tabIndex,
      tabIndexAttribute: target.getAttribute("tabindex"),
    };
  `,
  );
}

async function armAttachCompletionGate(
  client: MarionetteClient,
  role: string,
): Promise<{ windowId: string; armedAt: number } | null> {
  return await evaluateJson<{ windowId: string; armedAt: number } | null>(
    client,
    `
    ${roleHelpers()}
    const win = findRoleWindow(${JSON.stringify(role)});
    const hostState = win?.[${JSON.stringify(HOST_KEY)}];
    if (!win || !hostState?.ready || hostState.error || !hostState.host) {
      return null;
    }
    const host = hostState.host;
    const windowId = host.windowIdForTests(win);
    if (!windowId) return null;
    const gate = host.deferWindowAttachCompletionForTests(windowId);
    const state = {
      windowId,
      armedAt: Date.now(),
      entered: false,
      enteredAt: null,
      releasedAt: null,
      release: gate.release,
    };
    gate.entered.then(() => {
      state.entered = true;
      state.enteredAt = Date.now();
    });
    win[${JSON.stringify(ATTACH_COMPLETION_GATE_KEY)}] = state;
    return { windowId, armedAt: state.armedAt };
  `,
  );
}

async function attachCompletionGateSnapshot(
  client: MarionetteClient,
  role: string,
): Promise<AttachCompletionGateSnapshot | null> {
  return await evaluateJson<AttachCompletionGateSnapshot | null>(
    client,
    `
    ${roleHelpers()}
    const win = findRoleWindow(${JSON.stringify(role)});
    const state = win?.[${JSON.stringify(ATTACH_COMPLETION_GATE_KEY)}];
    if (!state) return null;
    return {
      windowId: state.windowId,
      armedAt: state.armedAt,
      entered: state.entered,
      enteredAt: state.enteredAt,
      releasedAt: state.releasedAt,
    };
  `,
  );
}

async function releaseAttachCompletionGate(
  client: MarionetteClient,
  role: string,
): Promise<AttachCompletionGateSnapshot | null> {
  return await evaluateJson<AttachCompletionGateSnapshot | null>(
    client,
    `
    ${roleHelpers()}
    const win = findRoleWindow(${JSON.stringify(role)});
    const key = ${JSON.stringify(ATTACH_COMPLETION_GATE_KEY)};
    const state = win?.[key];
    if (!state) return null;
    if (state.releasedAt === null) {
      state.releasedAt = Date.now();
      state.release();
    }
    const snapshot = {
      windowId: state.windowId,
      armedAt: state.armedAt,
      entered: state.entered,
      enteredAt: state.enteredAt,
      releasedAt: state.releasedAt,
    };
    delete win[key];
    return snapshot;
  `,
  );
}

async function performResilienceHostOperation(
  client: MarionetteClient,
  caseName: string,
  operation: "update" | "rollback" | "disable" | "enable",
  target: string | null,
  status: string,
  version: string,
  enabled: boolean,
): Promise<HostOperationSnapshot> {
  const started = await startHostOperation(client, operation, target);
  const result = await waitFor(
    () => hostOperationSnapshot(client),
    (value) => value?.done === true,
    `${caseName} host transaction`,
  );
  assert(
    result !== null && result.error === null && result.outcome !== null,
    `${caseName} host operation errored: ${result?.error}`,
  );
  assert(
    result.txId === started.txId &&
      result.expectedRevision === started.expectedRevision,
    `${caseName} request identity changed`,
  );
  assert(
    result.outcome.status === status,
    `${caseName} outcome status mismatch`,
  );
  assert(
    result.outcome.selectedVersion === version &&
      result.after?.selectedVersion === version &&
      result.after.desiredEnabled === enabled,
    `${caseName} host state mismatch`,
  );
  assert(
    result.after.stateRevision > result.expectedRevision,
    `${caseName} state revision did not advance`,
  );
  assert(
    JSON.stringify(result.queriedOutcome) === JSON.stringify(result.outcome),
    `${caseName} outcome query mismatch`,
  );
  observe(`lifecycle.${caseName}.host-outcome`, result);
  return result;
}

async function hostWindowIdentitySnapshot(
  client: MarionetteClient,
  roles: readonly string[],
): Promise<HostWindowIdentitySnapshot | null> {
  return await evaluateJson<HostWindowIdentitySnapshot | null>(
    client,
    `
    ${roleHelpers()}
    const initial = findRoleWindow("initial");
    const host = initial?.[${JSON.stringify(HOST_KEY)}]?.host;
    if (!host) return null;
    const roleWindowIds = {};
    for (const role of ${JSON.stringify(roles)}) {
      const win = findRoleWindow(role);
      roleWindowIds[role] = win ? host.windowIdForTests(win) : null;
    }
    return { hostWindowIds: host.windowIdsForTests, roleWindowIds };
  `,
  );
}

async function resilienceSnapshot(
  client: MarionetteClient,
  profileDir: string,
  roles: readonly string[],
): Promise<ResilienceSnapshot> {
  const registry = await registrySnapshot(client);
  const host = await hostSnapshot(client);
  const identity = await hostWindowIdentitySnapshot(client, roles);
  const canonical = await readHubRecord(profileDir) as HostRecord | null;
  return { registry, host, identity, canonical };
}

function assertHealthyResilienceSnapshot(
  snapshot: ResilienceSnapshot,
  caseName: string,
): asserts snapshot is ResilienceSnapshot & {
  host: HostSnapshot & { diagnostics: HostRecord };
  identity: HostWindowIdentitySnapshot;
  canonical: HostRecord & {
    lastOutcome: { txId: string; status: string };
  };
} {
  assert(
    snapshot.registry.connections === 2 && snapshot.registry.published &&
      snapshot.registry.enabled &&
      snapshot.registry.pendingAcknowledgements === 0,
    `${caseName} registry is not healthy: ${JSON.stringify(snapshot.registry)}`,
  );
  assert(
    snapshot.host?.ready === true && snapshot.host.error === null &&
      snapshot.host.diagnostics !== null && snapshot.host.trackedWindows === 2,
    `${caseName} host/window state is not healthy`,
  );
  assert(
    snapshot.identity !== null &&
      snapshot.identity.hostWindowIds.length === 2 &&
      snapshot.identity.roleWindowIds.initial !== null &&
      snapshot.identity.roleWindowIds["second-normal"] !== null,
    `${caseName} selected normal-window identities are missing`,
  );
  assert(
    snapshot.canonical !== null &&
      snapshot.canonical.desiredEnabled &&
      snapshot.canonical.selectedVersion === "hub-menu-v1" &&
      snapshot.canonical.selectedHealth === "verified" &&
      snapshot.canonical.blockedReason === null &&
      snapshot.canonical.lastOutcome?.status === "succeeded",
    `${caseName} canonical HUB record is not healthy`,
  );
}

function assertResilienceStateUnchanged(
  before: ResilienceSnapshot,
  after: ResilienceSnapshot,
  caseName: string,
): void {
  assertHealthyResilienceSnapshot(after, caseName);
  assert(
    before.canonical?.stateRevision === after.canonical?.stateRevision &&
      before.canonical?.lastOutcome?.txId ===
        after.canonical?.lastOutcome?.txId,
    `${caseName} changed canonical generation outcome`,
  );
  assert(
    before.host?.windowIds.join("|") === after.host?.windowIds.join("|") &&
      before.identity !== null && after.identity !== null &&
      JSON.stringify(before.identity) === JSON.stringify(after.identity),
    `${caseName} changed host/window identity`,
  );
}

function viteProbeHelpers(): string {
  return `
    const probeKey = ${JSON.stringify(VITE_PROBE_KEY)};
    const retainedKey = ${JSON.stringify(RETAINED_BUTTON_KEY)};
    function viteProbeSnapshot(win, role, host, recordMutationButton) {
      const probe = win[probeKey];
      if (!probe) return null;
      const document = win.document;
      const button = document.getElementById("${BUTTON_ID}");
      if (
        recordMutationButton && button && button !== probe.initialButton
      ) probe.mutationButton = button;
      const settings = document.getElementById("appMenu-settings-button");
      const trigger = document.getElementById("PanelUI-menu-button");
      return {
        role,
        windowId: host ? host.windowIdForTests(win) : null,
        windowFocused: document.hasFocus(),
        popupOpen: trigger?.getAttribute("open") === "true",
        buttonMatchCount: document.querySelectorAll("#${BUTTON_ID}").length,
        button: button ? {
          id: button.id,
          className: button.getAttribute("class") ?? "",
          disabled: typeof button.disabled === "boolean" ? button.disabled : null,
          immediateSettingsSibling: settings?.nextElementSibling === button,
        } : null,
        initialButtonConnected: Boolean(probe.initialButton?.isConnected),
        mutationButtonConnected: Boolean(probe.mutationButton?.isConnected),
        replacedInitialButton: Boolean(button && button !== probe.initialButton),
        replacedMutationButton: Boolean(
          button && probe.mutationButton && button !== probe.mutationButton
        ),
      };
    }
  `;
}

async function readViteUpdateLog(): Promise<ViteUpdateLogSnapshot> {
  const content = await Deno.readTextFile(DEV_SERVER_LOG_FILE);
  const lines = content.split(/\r?\n/);
  return {
    updates: lines.filter((line) =>
      line.includes("[vite]") && line.includes("hot updated:") &&
      /\/hub-panel-menu\/[^\s"]+\.tsx?(?:["\s]|$)/.test(line)
    ),
    reloads: lines.filter((line) =>
      line.includes("[vite]") && /page reload|full reload/i.test(line)
    ),
  };
}

async function installViteProbe(
  client: MarionetteClient,
  roles: readonly string[],
): Promise<{ role: string; installed: boolean; buttonPresent: boolean }[]> {
  return await evaluateJson(
    client,
    `
    ${roleHelpers()}
    ${viteProbeHelpers()}
    return ${JSON.stringify(roles)}.map((role) => {
      const win = findRoleWindow(role);
      const button = win?.document.getElementById("${BUTTON_ID}");
      if (!win || !button || win[probeKey]) {
        return { role, installed: false, buttonPresent: Boolean(button) };
      }
      const probe = {
        initialButton: button,
        mutationButton: null,
      };
      win[retainedKey] = button;
      win[probeKey] = probe;
      return { role, installed: true, buttonPresent: true };
    });
  `,
  );
}

async function viteProbeSnapshot(
  client: MarionetteClient,
  roles: readonly string[],
  recordMutationButton: boolean,
): Promise<ViteProbeSnapshot[] | null> {
  return await evaluateJson<ViteProbeSnapshot[] | null>(
    client,
    `
    ${roleHelpers()}
    ${viteProbeHelpers()}
    const initial = findRoleWindow("initial");
    const host = initial?.[${JSON.stringify(HOST_KEY)}]?.host;
    const values = ${JSON.stringify(roles)}.map((role) => {
      const win = findRoleWindow(role);
      return win
        ? viteProbeSnapshot(win, role, host, ${
      JSON.stringify(recordMutationButton)
    })
        : null;
    });
    return values.every((value) => value !== null) ? values : null;
  `,
  );
}

async function removeViteProbe(
  client: MarionetteClient,
  roles: readonly string[],
): Promise<{ errors: string[] }> {
  return await evaluateJson<{ errors: string[] }>(
    client,
    `
    ${roleHelpers()}
    const probeKey = ${JSON.stringify(VITE_PROBE_KEY)};
    const retainedKey = ${JSON.stringify(RETAINED_BUTTON_KEY)};
    const errors = [];
    for (const role of ${JSON.stringify(roles)}) {
      const win = findRoleWindow(role);
      const probe = win?.[probeKey];
      if (!win || !probe) {
        errors.push(role + ": HMR probe was unavailable during cleanup");
        continue;
      }
      try {
        delete win[probeKey];
        delete win[retainedKey];
      } catch (error) {
        errors.push(String(error));
      }
    }
    return { errors };
  `,
  );
}

async function runFocusAcceptance(
  client: MarionetteClient,
  profileDir: string,
  verdicts: string[],
  phase: "canonical" | "isolated-movement" = "canonical",
): Promise<void> {
  if (phase === "canonical") {
    await runLifecycleCase(
      "focus-generation-restores-entry",
      verdicts,
      async () => {
        const beforeFocus = await focusHubButton(client, "initial", profileDir);
        assert(
          beforeFocus.buttonTabIndexAttribute === "-1",
          "old focused HUB button lacks explicit tabindex=-1",
        );
        await retainCurrentButtons(client, ["initial", "second-normal"]);
        const operation = await performResilienceHostOperation(
          client,
          "focus-generation-restores-entry",
          "update",
          "hub-menu-v2",
          "succeeded",
          "hub-menu-v2",
          true,
        );
        const afterFocus = await waitFor(
          () => focusSnapshot(client, "initial"),
          (value) =>
            value?.windowFocused === true && value.popupOpen &&
            value.activeButton && value.buttonMatchCount === 1,
          "focus restoration to replacement HUB entry",
          3_000,
        );
        assert(
          afterFocus !== null,
          "replacement HUB focus state was not observed",
        );
        assert(
          afterFocus.buttonTabIndexAttribute === "-1",
          "replacement HUB button lacks explicit tabindex=-1",
        );
        await openMenu(client, "second-normal");
        const secondMenu = await menuSnapshot(client, "second-normal");
        assertMenu(secondMenu, "second-normal", false);
        for (const role of ["initial", "second-normal"]) {
          await verifyDetachedButton(
            client,
            role,
            "focus-generation-restores-entry",
          );
        }
        await verifyPrivateLegacy(client, "focus-generation-restores-entry");
        return { beforeFocus, operation, afterFocus, secondMenu };
      },
    );
  }

  if (phase === "canonical") {
    await runLifecycleCase(
      "focus-generation-return-to-v1",
      verdicts,
      async () => {
        await captureAndCloseNormalMenus(
          client,
          "focus-generation-return-to-v1",
        );
        const operation = await performResilienceHostOperation(
          client,
          "focus-generation-return-to-v1",
          "rollback",
          null,
          "succeeded",
          "hub-menu-v1",
          true,
        );
        await openMenu(client, "initial");
        await openMenu(client, "second-normal");
        return operation;
      },
    );
  }

  if (phase === "isolated-movement") {
    await runLifecycleCase(
      "isolated-host.pending-movement-during-replacement",
      verdicts,
      async () => {
        const beforeFocus = await focusHubButton(client, "initial", profileDir);
        assert(
          beforeFocus.buttonTabIndexAttribute === "-1",
          "focused HUB button lacks explicit tabindex=-1 before movement test",
        );
        let settingsFocus = await focusSnapshot(client, "initial");
        let settingsTabs = 0;
        const settingsTraversal = [settingsFocus?.activeElementId ?? ""];
        const settingsDeadline = Date.now() + 3_000;
        while (
          Date.now() < settingsDeadline && settingsTabs < 24 &&
          !settingsFocus?.activeSettings
        ) {
          await sendNativeTab(client);
          settingsTabs++;
          settingsFocus = await focusSnapshot(client, "initial");
          settingsTraversal.push(settingsFocus?.activeElementId ?? "");
        }
        observe(
          "isolated-host.pending-movement.settings-focus",
          {
            tabCount: settingsTabs,
            traversal: settingsTraversal,
            focus: settingsFocus,
          },
        );
        assert(
          settingsFocus?.activeSettings && settingsFocus.popupOpen &&
            settingsFocus.settingsVisible,
          "native Tab traversal from HUB did not focus visible Settings",
        );
        const movementTargetId = "appMenu-quit-button2";
        assert(
          settingsTraversal.includes(movementTargetId),
          "native Tab traversal did not previously reach the movement target",
        );
        const movementTarget = await focusedMenuItemSnapshot(
          client,
          "initial",
          movementTargetId,
        );
        assert(
          movementTarget !== null &&
            movementTarget.id === movementTargetId &&
            !movementTarget.active && movementTarget.connected &&
            movementTarget.enabled && movementTarget.insideMenu &&
            movementTarget.visible && movementTarget.tabIndex >= -1,
          "native keyboard target is not a reachable menu item",
        );
        observe(
          "isolated-host.pending-movement.target-details",
          { traversal: settingsTraversal, movementTarget },
        );
        await retainCurrentButtons(client, ["initial", "second-normal"]);
        const returnDeadline = Date.now() + 3_000;
        let focusedBeforeUpdate = await focusSnapshot(client, "initial");
        let returnTabs = 0;
        while (
          Date.now() < returnDeadline && returnTabs < 24 &&
          !focusedBeforeUpdate?.activeButton
        ) {
          await sendNativeTab(client);
          returnTabs++;
          focusedBeforeUpdate = await focusSnapshot(client, "initial");
        }
        assert(
          focusedBeforeUpdate?.activeButton === true &&
            focusedBeforeUpdate.popupOpen,
          "native Tab could not return focus to HUB before replacement",
        );
        const gateArmed = await armAttachCompletionGate(client, "initial");
        assert(gateArmed !== null, "attach completion gate was not armed");
        observe(
          "isolated-host.pending-movement.gate-armed",
          gateArmed,
        );
        let started: { txId: string; expectedRevision: number } | null = null;
        let gateEntered: AttachCompletionGateSnapshot | null = null;
        let focusAtGate: FocusSnapshot | null = null;
        let operationAtGate: HostOperationSnapshot | null = null;
        let movedFocus: FocusSnapshot | null = null;
        let operationAtMovement: HostOperationSnapshot | null = null;
        let nativeMoveAt: number | null = null;
        let gateReleased: AttachCompletionGateSnapshot | null = null;
        let movementError: unknown;
        let movementFailed = false;
        let gateReleaseError: unknown = null;
        try {
          started = await startHostOperation(client, "rollback", null);
          gateEntered = await waitFor(
            () => attachCompletionGateSnapshot(client, "initial"),
            (value) => value?.entered === true,
            "initial-window attach completion gate entry",
            3_000,
          );
          assert(
            gateEntered !== null &&
              gateEntered.windowId === gateArmed.windowId &&
              gateEntered.enteredAt !== null,
            "initial-window attach completion gate did not enter",
          );
          focusAtGate = await focusSnapshot(client, "initial");
          operationAtGate = await hostOperationSnapshot(client);
          observe("isolated-host.pending-movement.gate-entered", {
            gate: gateEntered,
            focus: focusAtGate,
            operation: operationAtGate,
          });
          assert(
            focusAtGate?.windowFocused === true &&
              focusAtGate.activeWindowIsTarget && focusAtGate.popupOpen &&
              focusAtGate.activeSettings && focusAtGate.settingsVisible,
            "attach gate did not leave the foreground open menu on visible Settings",
          );
          assert(
            started !== null && operationAtGate?.done === false &&
              operationAtGate.outcome === null &&
              operationAtGate.error === null &&
              operationAtGate.txId === started.txId,
            "rollback was not pending at attach gate entry",
          );
          await sendNativeTab(client, true);
          nativeMoveAt = Date.now();
          movedFocus = await focusSnapshot(client, "initial");
          operationAtMovement = await hostOperationSnapshot(client);
          assert(
            movedFocus?.activeElementId === movementTarget.id &&
              movedFocus.windowFocused && movedFocus.activeWindowIsTarget &&
              movedFocus.popupOpen && operationAtMovement?.done === false &&
              operationAtMovement.outcome === null &&
              operationAtMovement.error === null,
            "native Shift+Tab did not move focus to the verified target while rollback was pending",
          );
          assert(
            nativeMoveAt - gateEntered.enteredAt <= 3_000,
            "movement probe exceeded the existing focus-claim deadline",
          );
        } catch (error) {
          movementError = error;
          movementFailed = true;
        } finally {
          try {
            gateReleased = await releaseAttachCompletionGate(
              client,
              "initial",
            );
          } catch (error) {
            gateReleaseError = error;
          }
        }
        observe("isolated-host.pending-movement.gate-released", {
          armed: gateArmed,
          entered: gateEntered,
          nativeMoveAt,
          released: gateReleased,
          movementError: movementError instanceof Error
            ? movementError.message
            : movementError === undefined
            ? null
            : String(movementError),
          releaseError: gateReleaseError instanceof Error
            ? gateReleaseError.message
            : gateReleaseError === null
            ? null
            : String(gateReleaseError),
        });
        if (gateReleaseError !== null) {
          throw movementFailed
            ? new AggregateError(
              [movementError, gateReleaseError],
              "focus movement probe and gate release failed",
            )
            : gateReleaseError;
        }
        assert(started !== null, "rollback transaction did not start");
        const operation = await waitFor(
          () => hostOperationSnapshot(client),
          (value) => value?.done === true,
          "focus movement rollback transaction",
          3_000,
        );
        assert(
          operation !== null && operation.error === null &&
            operation.outcome?.status === "succeeded" &&
            operation.outcome.selectedVersion === "hub-menu-v1" &&
            operation.after?.selectedVersion === "hub-menu-v1" &&
            operation.after.desiredEnabled === true &&
            operation.txId === started.txId &&
            operation.expectedRevision === started.expectedRevision &&
            operation.after.stateRevision > operation.expectedRevision &&
            JSON.stringify(operation.outcome) ===
              JSON.stringify(operation.queriedOutcome),
          "focus movement rollback failed",
        );
        observe(
          "isolated-host.pending-movement.host-outcome",
          operation,
        );
        const gateHoldMs = gateEntered?.enteredAt !== null &&
            gateEntered?.enteredAt !== undefined &&
            gateReleased?.releasedAt !== null &&
            gateReleased?.releasedAt !== undefined
          ? gateReleased.releasedAt - gateEntered.enteredAt
          : null;
        assert(
          gateEntered?.entered === true && gateReleased?.entered === true &&
            gateHoldMs !== null && gateHoldMs <= 3_000,
          "attach completion gate was not released within the focus-claim deadline",
        );
        const afterFocus = movementFailed
          ? await focusSnapshot(client, "initial")
          : await waitFor(
            () => focusSnapshot(client, "initial"),
            (value) =>
              value?.windowFocused === true && value.popupOpen &&
              value.activeElementId === movementTarget.id &&
              value.buttonMatchCount === 1,
            "focus replacement respecting intervening movement",
            3_000,
          );
        observe("isolated-host.pending-movement.result", {
          focusAtGate,
          operationAtGate,
          movedFocus,
          operationAtMovement,
          nativeMoveAt,
          operation,
          afterFocus,
          gateHoldMs,
        });
        if (movementFailed) throw movementError;
        assert(
          afterFocus !== null,
          "movement-test focus state was not observed",
        );
        assert(
          afterFocus.buttonTabIndexAttribute === "-1",
          "movement-test replacement HUB button lacks tabindex=-1",
        );
        for (const role of ["initial", "second-normal"]) {
          await verifyDetachedButton(
            client,
            role,
            "isolated-host.pending-movement-during-replacement",
          );
        }
        await verifyPrivateLegacy(
          client,
          "isolated-host.pending-movement-during-replacement",
        );
        return {
          beforeFocus,
          settingsFocus,
          settingsTraversal,
          movementTarget,
          gateArmed,
          gateEntered,
          gateReleased,
          focusAtGate,
          operationAtGate,
          movedFocus,
          operationAtMovement,
          operation,
          afterFocus,
        };
      },
    );
  }

  if (phase === "canonical") {
    await runLifecycleCase("focus-disable-fallback", verdicts, async () => {
      const beforeFocus = await focusHubButton(client, "initial", profileDir);
      await retainCurrentButtons(client, ["initial", "second-normal"]);
      const operation = await performResilienceHostOperation(
        client,
        "focus-disable-fallback",
        "disable",
        null,
        "succeeded",
        "hub-menu-v1",
        false,
      );
      const afterFocus = await waitFor(
        () => focusSnapshot(client, "initial"),
        (value) =>
          value?.windowFocused === true && value.popupOpen &&
          (value.activeSettings || value.activeTrigger),
        "disable focus fallback",
        3_000,
      );
      assert(afterFocus !== null, "disable focus fallback was not observed");
      assert(
        (afterFocus.activeSettings && afterFocus.settingsVisible) ||
          (afterFocus.activeTrigger && afterFocus.triggerVisible),
        "disable did not leave focus on a visible Settings/trigger target",
      );
      const initialMenu = await menuSnapshot(client, "initial");
      assertMenuAbsent(initialMenu, "initial", false);
      for (const role of ["initial", "second-normal"]) {
        await verifyDetachedButton(client, role, "focus-disable-fallback");
      }
      return { beforeFocus, operation, afterFocus, initialMenu };
    });

    await runLifecycleCase("focus-reenable-no-theft", verdicts, async () => {
      const beforeFocus = await focusSnapshot(client, "initial");
      assert(
        beforeFocus?.windowFocused && beforeFocus.popupOpen &&
          (beforeFocus.activeSettings || beforeFocus.activeTrigger),
        "foreground focus did not remain on its disable fallback before re-enable",
      );
      const operation = await performResilienceHostOperation(
        client,
        "focus-reenable-no-theft",
        "enable",
        null,
        "succeeded",
        "hub-menu-v1",
        true,
      );
      const afterFocus = await waitFor(
        () => focusSnapshot(client, "initial"),
        (value) =>
          value?.windowFocused === true && value.popupOpen &&
          value.buttonMatchCount === 1,
        "re-enabled menu focus readiness",
        3_000,
      );
      assert(afterFocus !== null, "re-enable focus state was not observed");
      assert(
        !afterFocus.activeButton &&
          ((afterFocus.activeSettings && afterFocus.settingsVisible) ||
            (afterFocus.activeTrigger && afterFocus.triggerVisible)),
        "re-enable stole focus from the disable fallback",
      );
      assert(
        afterFocus.buttonTabIndexAttribute === "-1",
        "re-enabled HUB button lacks explicit tabindex=-1",
      );
      for (const role of ["initial", "second-normal"]) {
        const menu = await menuSnapshot(client, role);
        assertMenu(menu, role, false);
      }
      await verifyPrivateLegacy(client, "focus-reenable-no-theft");
      return { beforeFocus, operation, afterFocus };
    });
  }
}

async function runIsolatedPendingMovementAcceptance(
  client: MarionetteClient,
  profileDir: string,
  verdicts: string[],
): Promise<void> {
  await runLifecycleCase(
    "canonical-host.shutdown-before-isolated-movement",
    verdicts,
    () => shutdownHostForLifecyclePhase(client, "canonical-host"),
  );

  let isolatedHostStarted = false;
  let phaseError: unknown = null;
  let cleanupError: unknown = null;
  try {
    await runLifecycleCase(
      "isolated-host.open-and-prepare-v2",
      verdicts,
      async () => {
        const { root } = await beginIsolatedHubHostAcquisition(client);
        isolatedHostStarted = true;
        const host = await waitFor(
          () => hostSnapshot(client),
          (value) => value?.ready === true,
          "isolated HUB host acquisition",
        );
        assert(
          host !== null && host.error === null &&
            host.diagnostics?.selectedVersion === "hub-menu-v1" &&
            host.diagnostics.desiredEnabled &&
            host.windowIds.length === 2,
          "isolated HUB host did not start as enabled v1 in both normal windows",
        );
        const identity = await evaluateJson<HostWindowIdentitySnapshot | null>(
          client,
          `
          ${roleHelpers()}
          const host = findRoleWindow("initial")?.[${JSON.stringify(HOST_KEY)}]
            ?.host;
          const initial = findRoleWindow("initial");
          const second = findRoleWindow("second-normal");
          if (!host || !initial || !second) return null;
          return {
            hostWindowIds: host.windowIdsForTests,
            roleWindowIds: {
              initial: host.windowIdForTests(initial),
              "second-normal": host.windowIdForTests(second),
            },
          };
        `,
        );
        const registry = await registrySnapshot(client);
        assert(
          identity !== null && registry.connections === 2 &&
            registry.published && registry.enabled,
          "isolated host did not use the shared production window adapters",
        );
        observe("isolated-host.native-ui.identity", {
          root,
          host: "M0FeatureHost.openHubPanelMenuForTests",
          testSeam: "deferWindowAttachCompletionForTests",
          testOptionsSupplied: false,
          appMenuRegistry: "default shared AppMenuRegistry",
          hostSnapshot: host,
          identity,
          registry,
        });
        await captureAndCloseNormalMenus(client, "isolated-host-prepare-v2");
        const update = await performResilienceHostOperation(
          client,
          "isolated-host-prepare-v2",
          "update",
          "hub-menu-v2",
          "succeeded",
          "hub-menu-v2",
          true,
        );
        const updatedHost = await hostSnapshot(client);
        assert(
          updatedHost?.diagnostics?.selectedVersion === "hub-menu-v2" &&
            updatedHost.diagnostics.desiredEnabled &&
            updatedHost.windowIds.length === 2,
          "isolated host did not prepare enabled v2 for rollback movement",
        );
        await openMenu(client, "initial");
        await openMenu(client, "second-normal");
        return { root, host, identity, registry, update, updatedHost };
      },
    );
    await runFocusAcceptance(
      client,
      profileDir,
      verdicts,
      "isolated-movement",
    );
  } catch (error) {
    phaseError = error;
  } finally {
    if (isolatedHostStarted) {
      try {
        await runLifecycleCase(
          "isolated-host.shutdown",
          verdicts,
          () => shutdownHostForLifecyclePhase(client, "isolated-host"),
        );
      } catch (error) {
        cleanupError = error;
      }
    }
  }
  if (phaseError !== null && cleanupError !== null) {
    throw new AggregateError(
      [phaseError, cleanupError],
      "isolated pending-movement phase and host cleanup failed",
    );
  }
  if (phaseError !== null) throw phaseError;
  if (cleanupError !== null) throw cleanupError;
}

function assertViteWindowUpdate(
  snapshot: ViteProbeSnapshot,
  caseName: string,
  stage: "mutation" | "restored",
): void {
  assert(
    snapshot.buttonMatchCount === 1 && snapshot.button !== null &&
      snapshot.button.id === BUTTON_ID &&
      snapshot.button.className === "subviewbutton" &&
      snapshot.button.disabled === false &&
      snapshot.button.immediateSettingsSibling,
    `${caseName} HUB DOM is invalid in ${snapshot.role}`,
  );
  assert(
    stage === "mutation"
      ? snapshot.replacedInitialButton && !snapshot.initialButtonConnected
      : snapshot.replacedMutationButton &&
        !snapshot.mutationButtonConnected,
    `${caseName} did not dispose and replace the ${stage} node in ${snapshot.role}`,
  );
}

async function runViteHmrAcceptance(
  client: MarionetteClient,
  profileDir: string,
  verdicts: string[],
): Promise<void> {
  const roles = ["initial", "second-normal"] as const;
  await runLifecycleCase("hmr-vite-reconnect", verdicts, async () => {
    await verifyPrivateLegacy(client, "hmr-before");
    await evaluateJson<boolean>(
      client,
      `
      ${roleHelpers()}
      const win = findRoleWindow("private");
      if (!win?.PanelUI) return false;
      win.PanelUI.hide();
      return win.document.getElementById("PanelUI-menu-button")?.getAttribute("open") !== "true";
    `,
    );
    const focused = await focusHubButton(client, "initial", profileDir, false);
    const before = await resilienceSnapshot(client, profileDir, [
      "initial",
      "second-normal",
    ]);
    assertHealthyResilienceSnapshot(before, "hmr before");
    const installed = await installViteProbe(client, roles);
    assert(
      installed.length === roles.length &&
        installed.every((value) => value.installed),
      `could not observe Vite HMR in every window: ${
        JSON.stringify(installed)
      }`,
    );
    const beforeUpdateLog = await readViteUpdateLog();
    let mutated: ViteProbeSnapshot[] | null = null;
    let afterRestore: ViteProbeSnapshot[] | null = null;
    let mutationUpdateLog: ViteUpdateLogSnapshot | null = null;
    let operationError: unknown;
    let operationFailed = false;
    let cleanupError: unknown;
    let cleanupFailed = false;
    try {
      const sourceBefore = await Deno.readFile(HUB_VIEW_SOURCE_PATH);
      const mutationResult = await withExactSourceRestoration(
        HUB_VIEW_SOURCE_PATH,
        (source) => {
          const original =
            'export const HUB_MENU_LABEL_FALLBACK = "Floorp Hub";';
          assert(
            source.includes(original),
            "HMR mutation anchor was not found",
          );
          return source.replace(
            original,
            'export const HUB_MENU_LABEL_FALLBACK = "Floorp Hub HMR probe";',
          );
        },
        async () => {
          const snapshot = await waitFor(
            () => viteProbeSnapshot(client, roles, true),
            (value) =>
              value !== null && value.length === roles.length &&
              value.every((item) =>
                item.replacedInitialButton && !item.initialButtonConnected &&
                item.buttonMatchCount === 1
              ),
            "actual Vite HMR component update",
          );
          assert(snapshot !== null, "Vite HMR probe disappeared during update");
          for (const item of snapshot) {
            assertViteWindowUpdate(item, "HMR mutation", "mutation");
          }
          mutated = snapshot;
          mutationUpdateLog = await waitFor(
            () => readViteUpdateLog(),
            (value) =>
              value.updates.length >= beforeUpdateLog.updates.length +
                  roles.length ||
              value.reloads.length > beforeUpdateLog.reloads.length,
            "Vite log evidence for the HMR source mutation",
          );
          assert(
            mutationUpdateLog.reloads.length === beforeUpdateLog.reloads.length,
            `HMR mutation caused full reloads: ${
              JSON.stringify(mutationUpdateLog.reloads.slice(
                beforeUpdateLog.reloads.length,
              ))
            }`,
          );
          observe("lifecycle.hmr.vite-update-log.mutation", {
            count: mutationUpdateLog.updates.length -
              beforeUpdateLog.updates.length,
            lines: mutationUpdateLog.updates.slice(
              beforeUpdateLog.updates.length,
            ),
          });
          const during = await resilienceSnapshot(client, profileDir, [
            "initial",
            "second-normal",
          ]);
          assertResilienceStateUnchanged(before, during, "HMR mutation");
          const initialFocus = await focusSnapshot(client, "initial");
          assert(
            initialFocus?.windowFocused && initialFocus.popupOpen,
            "HMR mutation did not retain the foreground open menu",
          );
          observe("lifecycle.hmr.mutated-state", {
            connections: during.registry.connections,
            host: during.host,
            identity: during.identity,
            canonical: during.canonical,
            windows: snapshot,
          });
          return { state: during, windows: snapshot };
        },
        async () => {
          observe("lifecycle.hmr.source-restored", {
            sourcePath: HUB_VIEW_SOURCE_PATH,
            originalByteLength: sourceBefore.length,
            exactBytesVerified: true,
          });
          const snapshot = await waitFor(
            () => viteProbeSnapshot(client, roles, false),
            (value) =>
              value !== null && value.length === roles.length &&
              value.every((item) =>
                item.replacedMutationButton && !item.mutationButtonConnected &&
                item.buttonMatchCount === 1
              ),
            "Vite HMR after exact source restoration",
          );
          assert(
            snapshot !== null,
            "Vite HMR probe disappeared after restoration",
          );
          for (const item of snapshot) {
            assertViteWindowUpdate(item, "HMR restoration", "restored");
          }
          afterRestore = snapshot;
          const mutationUpdateCount = mutationUpdateLog?.updates.length ??
            beforeUpdateLog.updates.length;
          const mutationReloadCount = mutationUpdateLog?.reloads.length ??
            beforeUpdateLog.reloads.length;
          const restorationUpdateLog = await waitFor(
            () => readViteUpdateLog(),
            (value) =>
              value.updates.length >= mutationUpdateCount + roles.length ||
              value.reloads.length > mutationReloadCount,
            "Vite log evidence for source restoration",
          );
          assert(
            restorationUpdateLog.reloads.length === mutationReloadCount,
            `HMR restoration caused full reloads: ${
              JSON.stringify(restorationUpdateLog.reloads.slice(
                mutationReloadCount,
              ))
            }`,
          );
          observe("lifecycle.hmr.vite-update-log.restored", {
            count: restorationUpdateLog.updates.length - mutationUpdateCount,
            lines: restorationUpdateLog.updates.slice(mutationUpdateCount),
          });
          const restored = await Deno.readFile(HUB_VIEW_SOURCE_PATH);
          assert(
            sourceBefore.length === restored.length &&
              sourceBefore.every((byte, index) => byte === restored[index]),
            "HubPanelMenuView.tsx bytes changed after restoration",
          );
          const finalState = await resilienceSnapshot(client, profileDir, [
            "initial",
            "second-normal",
          ]);
          assertResilienceStateUnchanged(before, finalState, "HMR restoration");
          observe("lifecycle.hmr.restored-state", {
            connections: finalState.registry.connections,
            host: finalState.host,
            identity: finalState.identity,
            canonical: finalState.canonical,
            windows: snapshot,
          });
        },
      );
      const initialMenu = await menuSnapshot(client, "initial");
      assertMenu(initialMenu, "initial", false);
      await verifyPrivateLegacy(client, "hmr-after");
      for (const role of ["initial", "second-normal"]) {
        await verifyDetachedButton(client, role, "hmr-vite-reconnect");
      }
      return {
        focused,
        mutationResult,
        mutated,
        afterRestore,
      };
    } catch (error) {
      operationError = error;
      operationFailed = true;
    } finally {
      try {
        const cleanup = await removeViteProbe(client, roles);
        observe("lifecycle.hmr-observer-cleanup", cleanup);
        if (cleanup.errors.length > 0) {
          cleanupError = new AggregateError(
            cleanup.errors.map((message) => new Error(message)),
            "Vite HMR observer cleanup failed",
          );
          cleanupFailed = true;
        }
      } catch (error) {
        cleanupError = error;
        cleanupFailed = true;
      }
    }
    if (operationFailed && cleanupFailed) {
      throw new AggregateError(
        [operationError, cleanupError],
        "HMR acceptance and observer cleanup both failed",
      );
    }
    if (operationFailed) throw operationError;
    if (cleanupFailed) throw cleanupError;
  });
}

async function verifyLifecycleDom(
  client: MarionetteClient,
  caseName: string,
  enabled: boolean,
): Promise<void> {
  for (const role of ["initial", "second-normal"]) {
    const closed = await waitFor(
      () => menuSnapshot(client, role),
      (value) => value?.popupOpen === false,
      `${caseName} ${role} closed-menu observation`,
    );
    observe(`lifecycle.${caseName}.${role}.closed-menu-after`, closed);
    await openMenu(client, role);
    const open = await waitFor(
      () => menuSnapshot(client, role),
      (value) =>
        value?.popupOpen === true && (!enabled || value.button !== null),
      `${caseName} ${role} open-menu readiness`,
    );
    if (enabled) assertMenu(open, role, false);
    else assertMenuAbsent(open, role, false);
    observe(`lifecycle.${caseName}.${role}.open-menu-after`, open);
  }
}

async function verifyPrivateLegacy(
  client: MarionetteClient,
  caseName: string,
): Promise<void> {
  await openMenu(client, "private");
  const menu = await waitFor(
    () => menuSnapshot(client, "private"),
    (value) => value?.popupOpen === true && value.button !== null,
    `${caseName} private legacy menu`,
  );
  assertMenu(menu, "private", true);
  const registry = await registrySnapshot(client);
  assert(
    registry.connections === 2,
    `${caseName} private window registered an adapter`,
  );
  observe(`lifecycle.${caseName}.private-legacy`, { menu, registry });
}

async function runHostLifecycle(
  client: MarionetteClient,
  profileDir: string,
  verdicts: string[],
  runFocus: boolean,
): Promise<void> {
  const automatic = await readHubRecord(profileDir) as HostRecord | null;
  assert(
    automatic?.selectedVersion === "hub-menu-v1",
    "automatic v1 state is missing before host acquisition",
  );
  await beginCanonicalHostAcquisition(client);
  const host = await waitFor(
    () => hostSnapshot(client),
    (value) => value?.ready === true,
    "canonical host after automatic startup proof",
  );
  assert(
    host !== null && host.error === null && host.diagnostics !== null,
    "canonical host acquisition failed",
  );
  assert(
    host.diagnostics.selectedVersion === "hub-menu-v1",
    "canonical host did not reuse automatic v1 state",
  );
  assert(
    host.windowIds.length === 2,
    `expected two selected normal host windows, got ${host.windowIds.length}`,
  );
  observe("lifecycle.canonical-host-after-startup-proof", host);

  const roles = ["initial", "second-normal", "private"] as const;
  for (const role of roles) {
    await runLifecycleCase(
      `button-command.${role}`,
      verdicts,
      () => verifyRealButtonCommand(client, role, roles),
    );
  }

  const operations = [
    {
      name: "update-v1-to-v2",
      operation: "update" as const,
      target: "hub-menu-v2",
      status: "succeeded",
      version: "hub-menu-v2",
      enabled: true,
    },
    {
      name: "rollback-v2-to-v1",
      operation: "rollback" as const,
      target: null,
      status: "succeeded",
      version: "hub-menu-v1",
      enabled: true,
    },
    {
      name: "broken-candidate-recovery",
      operation: "update" as const,
      target: "hub-menu-broken",
      status: "failed_restored",
      version: "hub-menu-v1",
      enabled: true,
    },
    {
      name: "disable",
      operation: "disable" as const,
      target: null,
      status: "succeeded",
      version: "hub-menu-v1",
      enabled: false,
    },
    {
      name: "re-enable",
      operation: "enable" as const,
      target: null,
      status: "succeeded",
      version: "hub-menu-v1",
      enabled: true,
    },
  ];
  for (const item of operations) {
    await runLifecycleCase(item.name, verdicts, async () => {
      await captureAndCloseNormalMenus(client, item.name);
      const started = await startHostOperation(
        client,
        item.operation,
        item.target,
      );
      const result = await waitFor(
        () => hostOperationSnapshot(client),
        (value) => value?.done === true,
        `${item.name} host transaction`,
      );
      assert(
        result !== null && result.error === null && result.outcome !== null,
        `${item.name} host operation errored: ${result?.error}`,
      );
      assert(
        result.txId === started.txId &&
          result.expectedRevision === started.expectedRevision,
        `${item.name} request identity changed`,
      );
      assert(
        result.outcome.status === item.status,
        `${item.name} status: ${result.outcome.status}`,
      );
      assert(
        result.outcome.selectedVersion === item.version,
        `${item.name} outcome selected version mismatch`,
      );
      assert(
        result.after?.desiredEnabled === item.enabled &&
          result.after.selectedVersion === item.version,
        `${item.name} host state mismatch`,
      );
      assert(
        result.after.stateRevision > result.expectedRevision,
        `${item.name} state revision did not advance`,
      );
      assert(
        JSON.stringify(result.queriedOutcome) ===
          JSON.stringify(result.outcome),
        `${item.name} outcome query mismatch`,
      );
      observe(`lifecycle.${item.name}.host-outcome`, result);
      for (const role of ["initial", "second-normal"]) {
        await verifyDetachedButton(client, role, item.name);
      }
      await verifyLifecycleDom(client, item.name, item.enabled);
      await verifyPrivateLegacy(client, item.name);
      return result;
    });
  }

  await runLifecycleCase("late-window-and-closure", verdicts, async () => {
    const opened = await evaluateJson<{ opened: boolean }>(
      client,
      `
      ${roleHelpers()}
      const initial = findRoleWindow("initial");
      const opened = initial?.OpenBrowserWindow?.();
      if (!opened) return { opened: false };
      opened[roleKey] = "late-normal";
      return { opened: true };
    `,
    );
    assert(opened.opened, "late normal window could not be opened");
    const lateHost = await waitFor(
      () => hostSnapshot(client),
      (value) => value?.ready === true && value.windowIds.length === 3,
      "late normal window host admission",
    );
    assert(lateHost !== null, "late window host state disappeared");
    assert(
      lateHost.diagnostics?.selectedVersion === "hub-menu-v1" &&
        lateHost.diagnostics.desiredEnabled,
      "late window did not join current enabled v1 generation",
    );
    await openMenu(client, "late-normal");
    const lateMenu = await waitFor(
      () => menuSnapshot(client, "late-normal"),
      (value) => value?.popupOpen === true && value.button !== null,
      "late normal menu",
    );
    assertMenu(lateMenu, "late normal", false);
    const lateId = await evaluateJson<string | null>(
      client,
      `
      ${roleHelpers()}
      const host = findRoleWindow("initial")?.[${
        JSON.stringify(HOST_KEY)
      }]?.host;
      const win = findRoleWindow("late-normal");
      return host && win ? host.windowIdForTests(win) : null;
    `,
    );
    assert(
      lateId !== null && lateHost.windowIds.includes(lateId),
      "late normal window is not attached to the canonical host",
    );
    observe("lifecycle.late-window.current-v1", {
      host: lateHost,
      menu: lateMenu,
      windowId: lateId,
    });
    const beforeClose = await registrySnapshot(client);
    const closed = await evaluateJson<boolean>(
      client,
      `
      ${roleHelpers()}
      const win = findRoleWindow("late-normal");
      if (!win) return false;
      win.close();
      return true;
    `,
    );
    assert(closed, "late normal window disappeared before closure");
    const afterClose = await waitFor(
      async () => ({
        host: await hostSnapshot(client),
        registry: await registrySnapshot(client),
      }),
      (value) =>
        value.host?.windowIds.length === 2 &&
        value.registry.connections === beforeClose.connections - 1,
      "late window true closure detachment",
    );
    assert(
      afterClose.host?.windowIds.length === 2,
      "host did not forget closed late window",
    );
    assert(
      afterClose.registry.connections === beforeClose.connections - 1,
      "registry did not forget closed late window",
    );
    await runLifecycleCase(
      "surviving-window-command-after-close",
      verdicts,
      () => verifyRealButtonCommand(client, "initial", roles),
    );
    observe("lifecycle.late-window-closed", { beforeClose, afterClose });
    return { lateId, beforeClose, afterClose };
  });

  let focusError: unknown;
  let focusFailed = false;
  if (runFocus) {
    try {
      await runFocusAcceptance(client, profileDir, verdicts);
    } catch (error) {
      focusError = error;
      focusFailed = true;
    }
  } else {
    log("[SKIP] lifecycle.focus-continuity (--hmr-only mode)");
    verdicts.push("lifecycle.focus-continuity: SKIPPED (--hmr-only)");
    observe("lifecycle.focus-continuity.verdict", {
      status: "SKIPPED",
      mode: "hmr-only",
    });
  }
  let hmrError: unknown;
  let hmrFailed = false;
  try {
    await runViteHmrAcceptance(client, profileDir, verdicts);
  } catch (error) {
    hmrError = error;
    hmrFailed = true;
  }
  if (focusFailed && hmrFailed) {
    throw new AggregateError(
      [focusError, hmrError],
      "focus continuity and HMR acceptance both failed",
    );
  }
  if (focusFailed) throw focusError;
  if (hmrFailed) throw hmrError;
}

function hasExactProfileArgument(
  process: { argv?: string[]; commandLine: string },
  profileDir: string,
): boolean {
  if (process.argv) {
    return process.argv.some((argument, index) =>
      argument === "--profile" && process.argv?.[index + 1] === profileDir
    );
  }
  return process.commandLine.includes(`--profile ${profileDir}`) ||
    process.commandLine.includes(`--profile=${profileDir}`);
}

async function assertNoConflictingDevProcesses(): Promise<void> {
  const processes = await listDevProcesses();
  const conflicts = processes.filter((process) => {
    if (!process.commandLine.includes(PROJECT_ROOT)) return false;
    const command = process.commandLine;
    return command.includes("tools/feles-build.ts dev") ||
      command.includes("tools/feles-build.ts test") ||
      command.includes("tools/dev-tool.ts start") ||
      (process.name.toLowerCase().includes("floorp") &&
        command.includes("--profile"));
  });
  assert(
    conflicts.length === 0,
    `existing Floorp/dev-tool processes must be resolved first: ${
      conflicts.map((process) => `${process.pid}:${process.commandLine}`).join(
        " | ",
      )
    }`,
  );
}

async function assertNoProfileProcesses(profileDir: string): Promise<void> {
  const processes = await listDevProcesses();
  const selected = selectDevProcesses(processes, {
    platform: PLATFORM,
    projectRoot: PROJECT_ROOT,
    profilePath: profileDir,
    recordedProcesses: readDevProcesses(),
  });
  const exactProfile = processes.filter((process) =>
    hasExactProfileArgument(process, profileDir)
  );
  assert(
    selected.length === 0 && exactProfile.length === 0,
    `profile-owned process remains after stop: ${
      [
        ...selected.map((pid) => String(pid)),
        ...exactProfile.map((process) => String(process.pid)),
      ].join(",")
    }`,
  );
}

async function tcpPortOpen(port: number): Promise<boolean> {
  try {
    const connection = await Deno.connect({ hostname: "127.0.0.1", port });
    connection.close();
    return true;
  } catch {
    return false;
  }
}

async function sha256Text(value: string): Promise<string> {
  const bytes = await crypto.subtle.digest(
    "SHA-256",
    new TextEncoder().encode(value),
  );
  return Array.from(
    new Uint8Array(bytes),
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("");
}

async function runFailureStateAcceptance(
  client: MarionetteClient,
  profileDir: string,
  scenario: FailureStateScenario,
  verdicts: string[],
): Promise<void> {
  const statePath = path.join(profileDir, HUB_ROOT_NAME, "state.json");
  const originalBytes = scenario.stateBytes;
  let quarantineRecord: HostRecord | null = null;
  if (scenario.name === "persisted-quarantine") {
    quarantineRecord = JSON.parse(
      new TextDecoder().decode(originalBytes),
    ) as HostRecord;
    assert(
      quarantineRecord.featureId === "hub-panel-menu" &&
        quarantineRecord.blockedReason !== null &&
        quarantineRecord.desiredEnabled,
      "seeded quarantine record is not a valid blocked HUB record",
    );
  }

  await openMenu(client, "initial");
  const initialMenu = await waitFor(
    () => menuSnapshot(client, "initial"),
    (value) => value?.popupOpen === true,
    `${scenario.name} initial normal menu`,
  );
  assertMenuAbsent(initialMenu, "initial normal", false);
  observe(`${scenario.name}.initial-normal-menu`, initialMenu);

  const bootstrapReport = await evaluateJson<{
    consoleApiMessages: string[];
    serviceMessages: string[];
  }>(
    client,
    `
      const key = ${JSON.stringify(BOOTSTRAP_ERROR_OBSERVER_KEY)};
      const state = window[key];
      const stringifyArgument = (value) => {
        if (value instanceof Error) {
          return { name: value.name, message: value.message, stack: value.stack };
        }
        try {
          const json = JSON.parse(JSON.stringify(value));
          if (
            json && typeof json === "object" &&
            Object.keys(json).length === 0
          ) return { type: Object.prototype.toString.call(value), string: String(value) };
          return json;
        } catch {
          return String(value);
        }
      };
      const storage = Cc["@mozilla.org/consoleAPI-storage;1"]
        .getService(Ci.nsIConsoleAPIStorage);
      const cachedMessages = Array.from(storage.getEvents() ?? [], (event) =>
        JSON.stringify({
          level: event.level ?? null,
          arguments: Array.from(event.arguments ?? [], stringifyArgument),
        })
      ).filter((message) =>
        message.includes("[HubPanelMenuBootstrap] feature startup failed")
      );
      const serviceMessages = Services.console.getMessageArray().map((message) =>
        message instanceof Ci.nsIScriptError
          ? message.errorMessage
          : message.message || String(message)
      );
      const consoleApiMessages = [
        ...(state?.messages ?? []),
        ...cachedMessages,
      ];
      if (state?.observer) {
        try { Services.obs.removeObserver(state.observer, "console-api-log-event"); }
        catch {}
        delete window[key];
      }
      return { consoleApiMessages, serviceMessages };
    `,
  );
  const bootstrapFailure =
    [...bootstrapReport.consoleApiMessages, ...bootstrapReport.serviceMessages]
      .find((message) =>
        message.includes("[HubPanelMenuBootstrap] feature startup failed")
      ) ?? null;
  observe(`${scenario.name}.automatic-bootstrap-report`, {
    message: bootstrapFailure,
    consoleApiMessages: bootstrapReport.consoleApiMessages,
    serviceMessages: bootstrapReport.serviceMessages,
    manualHostImportOrAcquisition: false,
  });
  if (scenario.name === "invalid-canonical-state") {
    assert(
      bootstrapFailure !== null,
      "automatic bootstrap error was not reported before manual host acquisition",
    );
  }

  const afterInitial = await Deno.readFile(statePath);
  assert(
    afterInitial.length === originalBytes.length &&
      afterInitial.every((byte, index) => byte === originalBytes[index]),
    `${scenario.name} state bytes changed during bootstrap`,
  );

  const openedSecond = await evaluateJson<{ opened: boolean }>(
    client,
    `
      ${roleHelpers()}
      const initial = findRoleWindow("initial");
      const opened = initial?.OpenBrowserWindow?.();
      if (opened) opened[roleKey] = "late-normal";
      return { opened: Boolean(opened) };
    `,
  );
  assert(
    openedSecond.opened,
    `${scenario.name} could not open a late normal window`,
  );
  const lateStartup = await waitFor(
    () =>
      evaluateJson<boolean>(
        client,
        `${roleHelpers()} return Boolean(findRoleWindow("late-normal")?.gBrowserInit?.delayedStartupFinished);`,
      ),
    Boolean,
    `${scenario.name} late normal window startup`,
  );
  assert(lateStartup, `${scenario.name} late normal window is not ready`);
  await openMenu(client, "late-normal");
  const lateMenu = await waitFor(
    () => menuSnapshot(client, "late-normal"),
    (value) => value?.popupOpen === true,
    `${scenario.name} late normal menu`,
  );
  assertMenuAbsent(lateMenu, "late normal", false);
  observe(`${scenario.name}.late-normal-menu`, lateMenu);

  const openedPrivate = await evaluateJson<{ opened: boolean }>(
    client,
    `
      ${roleHelpers()}
      const initial = findRoleWindow("initial");
      const opened = initial?.OpenBrowserWindow?.({ private: true });
      if (opened) opened[roleKey] = "private";
      return { opened: Boolean(opened) };
    `,
  );
  assert(
    openedPrivate.opened,
    `${scenario.name} could not open a private window`,
  );
  await waitFor(
    () =>
      evaluateJson<boolean>(
        client,
        `${roleHelpers()} const win = findRoleWindow("private"); return Boolean(win?.gBrowserInit?.delayedStartupFinished && privateUtils.isWindowPrivate(win));`,
      ),
    Boolean,
    `${scenario.name} private window startup`,
  );
  await openMenu(client, "private");
  const privateMenu = await waitFor(
    () => menuSnapshot(client, "private"),
    (value) => value?.popupOpen === true && value.button !== null,
    `${scenario.name} private legacy menu`,
  );
  assertMenu(privateMenu, "private legacy", true);
  const command = await verifyRealButtonCommand(
    client,
    "private",
    ["initial", "late-normal", "private"],
  );
  observe(`${scenario.name}.private-legacy-command`, { privateMenu, command });

  if (quarantineRecord) {
    await runIsolatedLiveQuarantine(client, profileDir);
    verdicts.push("isolated-live-quarantine: PASS");
  }
  const finalBytes = await Deno.readFile(statePath);
  assert(
    finalBytes.length === originalBytes.length &&
      finalBytes.every((byte, index) => byte === originalBytes[index]),
    `${scenario.name} state bytes changed during window checks`,
  );
  if (quarantineRecord) {
    const finalRecord = JSON.parse(
      new TextDecoder().decode(finalBytes),
    ) as HostRecord;
    assert(
      finalRecord.blockedReason === quarantineRecord.blockedReason &&
        finalRecord.desiredEnabled,
      "persisted quarantine state did not remain blocked",
    );
  }
  verdicts.push(`${scenario.name}: PASS`);
  log(
    `[PASS] ${scenario.name}: blocked startup, no normal HUB fallback, private legacy command works`,
  );
}

async function generateQuarantinedState(): Promise<Uint8Array> {
  const profileDir = path.join(ARTIFACT_ROOT, "quarantine-seed-profile");
  const profileRoot = path.join(profileDir, "floorp-hotfix-m0-tests");
  const testRoot = path.join(
    profileRoot,
    `feature-${crypto.randomUUID()}`,
  );
  const prefsPath = path.join(profileDir, "prefs.js");
  let startAttempted = false;
  let client: MarionetteClient | null = null;
  let port: number | null = null;
  let failure: unknown;
  let failed = false;
  let generatedBytes: Uint8Array | null = null;
  let cleanupFailure: AggregateError | null = null;
  assert(
    !(await pathExists(profileDir)),
    `quarantine seed profile already exists: ${profileDir}`,
  );
  await Deno.mkdir(profileDir, { recursive: true });
  await Deno.writeTextFile(
    prefsPath,
    `user_pref("${SELECTED_PREF}", true);\n`,
    { createNew: true },
  );
  try {
    startAttempted = true;
    const startOutput = await runDevTool(profileDir, [
      "start",
      "--port-offset",
      "auto",
    ]);
    assert(
      startOutput.includes("Browser ready!"),
      "quarantine seed browser did not start",
    );
    const userPath = path.join(profileDir, "user.js");
    assert(
      await pathExists(userPath),
      "quarantine seed launcher did not write user.js",
    );
    const generatedUserJs = await Deno.readTextFile(userPath);
    assert(
      !generatedUserJs.includes(SELECTED_PREF) &&
        (await Deno.readTextFile(prefsPath)).includes(
          `user_pref("${SELECTED_PREF}", true);`,
        ),
      "quarantine seed startup selection was not preserved",
    );
    port = Number((await Deno.readTextFile(MARIONETTE_PORT_FILE)).trim());
    assert(
      Number.isInteger(port) && port > 0,
      `invalid quarantine seed Marionette port: ${port}`,
    );
    const activeProcesses = await listDevProcesses();
    assert(
      activeProcesses.some((process) =>
        hasExactProfileArgument(process, profileDir)
      ),
      "quarantine seed browser does not use its dedicated profile",
    );
    observe("failure-states.quarantine-seed-launcher", {
      profileDir,
      port,
      startOutput,
      browserUsesExactProfile: true,
    });
    client = await MarionetteClient.connect(port);
    await client.setContext("chrome");
    await startupSnapshot(client);
    await openMenu(client, "initial");
    const openedSecond = await evaluateJson<{ opened: boolean }>(
      client,
      `
        ${roleHelpers()}
        const initial = findRoleWindow("initial");
        const opened = initial?.OpenBrowserWindow?.();
        if (opened) opened[roleKey] = "quarantine-seed-second";
        return { opened: Boolean(opened) };
      `,
    );
    assert(openedSecond.opened, "could not open second quarantine seed window");
    await waitFor(
      () =>
        evaluateJson<boolean>(
          client!,
          `${roleHelpers()} const win = findRoleWindow("quarantine-seed-second"); return Boolean(win?.gBrowserInit?.delayedStartupFinished && win.PanelUI?.panel);`,
        ),
      (ready) => ready,
      "quarantine seed second-window startup",
    );
    await openMenu(client, "quarantine-seed-second");
    const seedHostKey = `${ROLE_KEY}_quarantine_seed_host`;
    const openedHost = await evaluateAsyncJson<{
      initial: HostRecord;
      canonicalShutdown: { errors: string[]; timedOut: boolean };
      windowIds: string[];
    }>(
      client,
      `
        const { M0FeatureHost } = ChromeUtils.importESModule(${
        JSON.stringify(FEATURE_HOST_URI)
      });
        const canonical = await M0FeatureHost.openHubPanelMenu(true);
        const canonicalShutdown = await canonical.shutdown();
        if (canonicalShutdown.errors.length || canonicalShutdown.timedOut) {
          throw new Error("seed canonical host did not shut down cleanly");
        }
        const host = await M0FeatureHost.openHubPanelMenuForTests(${
        JSON.stringify(testRoot)
      }, false);
        window[${JSON.stringify(seedHostKey)}] = host;
        return { initial: host.diagnostics, canonicalShutdown, windowIds: host.windowIdsForTests };
      `,
    );
    assert(
      openedHost.initial.desiredEnabled && openedHost.windowIds.length >= 2,
      `isolated seed host did not attach both normal windows: ${
        JSON.stringify(openedHost)
      }`,
    );
    const seedRegistry = await registrySnapshot(client);
    assert(
      seedRegistry.connections >= 2 && seedRegistry.published &&
        seedRegistry.enabled,
      "quarantine seed host did not use the shared production adapters",
    );
    await evaluateJson<boolean>(
      client,
      `window[${
        JSON.stringify(seedHostKey)
      }].failWindowAttachForTests("*"); return true;`,
    );
    const openedFaultWindow = await evaluateJson<{ opened: boolean }>(
      client,
      `
        ${roleHelpers()}
        const initial = findRoleWindow("initial");
        const opened = initial?.OpenBrowserWindow?.();
        if (opened) opened[roleKey] = "quarantine-seed-fault";
        return { opened: Boolean(opened) };
      `,
    );
    assert(
      openedFaultWindow.opened,
      "could not open a native window to trigger quarantine",
    );
    await waitFor(
      () =>
        evaluateJson<boolean>(
          client!,
          `${roleHelpers()} const win = findRoleWindow("quarantine-seed-fault"); return Boolean(win?.gBrowserInit?.delayedStartupFinished && win.PanelUI?.panel);`,
        ),
      (ready) => ready,
      "quarantine fault-window startup",
    );
    const seeded = await waitFor(
      () =>
        evaluateJson<{ record: HostRecord; windowIds: string[] }>(
          client!,
          `const host = window[${
            JSON.stringify(seedHostKey)
          }]; return { record: host.diagnostics, windowIds: host.windowIdsForTests };`,
        ),
      (value) => value.record.blockedReason !== null,
      "post-commit quarantine after native window attach failure",
      30_000,
    );
    assert(
      seeded.record.blockedReason !== null && seeded.record.desiredEnabled &&
        seeded.record.lastOutcome !== null,
      `native fail seam did not persist quarantine: ${JSON.stringify(seeded)}`,
    );
    observe("failure-states.isolated-quarantine-seed", {
      storage:
        "isolated feature-* test root; native HUB definition and shared real adapters",
      testRoot,
      hostRecord: seeded.record,
      failure: "post-commit native-window attach failure",
      windowIds: seeded.windowIds,
      openedHost,
      openedFaultWindow,
      registry: seedRegistry,
    });
    const shutdown = await evaluateAsyncJson<{
      errors: string[];
      timedOut: boolean;
    }>(
      client,
      `
        const host = window[${JSON.stringify(seedHostKey)}];
        delete window[${JSON.stringify(seedHostKey)}];
        return await host.shutdown();
      `,
    );
    assert(
      shutdown.errors.length === 0 && !shutdown.timedOut,
      "isolated quarantine seed host did not shut down cleanly",
    );
    observe("failure-states.quarantine-seed-shutdown", {
      testRoot,
      shutdown,
    });
    const statePath = path.join(testRoot, "state.json");
    const bytes = await Deno.readFile(statePath);
    const record = JSON.parse(new TextDecoder().decode(bytes)) as HostRecord;
    assert(
      record.featureId === "hub-panel-menu" &&
        record.blockedReason === seeded.record.blockedReason &&
        record.desiredEnabled,
      "native persisted quarantine bytes differ from host diagnostics",
    );
    generatedBytes = bytes;
  } catch (error) {
    failed = true;
    failure = error;
    observe("failure-states.quarantine-seed-failure", {
      error: error instanceof Error ? error.message : String(error),
      stack: error instanceof Error ? error.stack : null,
      testRoot,
    });
  } finally {
    const cleanupErrors: unknown[] = [];
    try {
      await client?.close();
    } catch (error) {
      cleanupErrors.push(error);
    }
    if (startAttempted) {
      try {
        await runDevTool(profileDir, ["stop"]);
      } catch (error) {
        cleanupErrors.push(error);
      }
      try {
        observe(
          "failure-states.quarantine-seed-dev-server-log",
          await preserveDevServerLog("quarantine-seed"),
        );
      } catch (error) {
        cleanupErrors.push(error);
      }
    }
    try {
      await assertNoProfileProcesses(profileDir);
    } catch (error) {
      cleanupErrors.push(error);
    }
    if (port !== null) {
      try {
        assert(
          !(await tcpPortOpen(port)),
          `quarantine seed listener remains on ${port}`,
        );
      } catch (error) {
        cleanupErrors.push(error);
      }
    }
    try {
      assert(
        !(await pathExists(MARIONETTE_PORT_FILE)),
        "quarantine seed left the shared Marionette port file behind",
      );
    } catch (error) {
      cleanupErrors.push(error);
    }
    observe("failure-states.quarantine-seed-cleanup", {
      profileDir,
      port,
      failed,
      failure: failure instanceof Error ? failure.message : failure,
      cleanupChecksPassed: cleanupErrors.length === 0,
      errors: cleanupErrors.map((error) =>
        error instanceof Error ? error.message : String(error)
      ),
    });
    if (cleanupErrors.length > 0) {
      cleanupFailure = new AggregateError(
        cleanupErrors,
        "quarantine seed cleanup failed",
      );
    }
  }
  if (failed && cleanupFailure) {
    throw new AggregateError(
      [failure, cleanupFailure],
      "quarantine seed and cleanup failed",
    );
  }
  if (failed) throw failure;
  if (cleanupFailure) throw cleanupFailure;
  assert(
    generatedBytes !== null,
    "quarantine seed did not produce state bytes",
  );
  return generatedBytes;
}

async function runIsolatedLiveQuarantine(
  client: MarionetteClient,
  profileDir: string,
): Promise<void> {
  const testRoot = path.join(
    profileDir,
    "floorp-hotfix-m0-tests",
    `feature-${crypto.randomUUID()}`,
  );
  const hostKey = `${ROLE_KEY}_live_quarantine_host`;
  const opened = await evaluateAsyncJson<{
    record: HostRecord;
    windowIds: string[];
  }>(
    client,
    `
      const { M0FeatureHost } = ChromeUtils.importESModule(${
      JSON.stringify(FEATURE_HOST_URI)
    });
      const host = await M0FeatureHost.openHubPanelMenuForTests(${
      JSON.stringify(testRoot)
    }, false);
      window[${JSON.stringify(hostKey)}] = host;
      return { record: host.diagnostics, windowIds: host.windowIdsForTests };
    `,
  );
  assert(
    opened.record.featureId === "hub-panel-menu" &&
      opened.record.desiredEnabled && opened.record.blockedReason === null &&
      opened.windowIds.length > 0,
    `isolated live host was not initially active: ${JSON.stringify(opened)}`,
  );
  await openMenu(client, "initial");
  const activeMenu = await waitFor(
    () => menuSnapshot(client, "initial"),
    (value) => value?.popupOpen === true && value.button !== null,
    "isolated live host initial HUB menu",
  );
  assertMenu(activeMenu, "isolated normal", false);
  await openMenu(client, "late-normal");
  const lateActiveMenu = await waitFor(
    () => menuSnapshot(client, "late-normal"),
    (value) => value?.popupOpen === true && value.button !== null,
    "isolated live host late-normal HUB menu",
  );
  assertMenu(lateActiveMenu, "isolated late normal", false);
  const registryBeforeFault = await registrySnapshot(client);
  assert(
    registryBeforeFault.connections >= 2 && registryBeforeFault.published &&
      registryBeforeFault.enabled,
    `isolated live host lacks shared real adapters: ${
      JSON.stringify(registryBeforeFault)
    }`,
  );
  await evaluateJson<boolean>(
    client,
    `window[${
      JSON.stringify(hostKey)
    }].failWindowAttachForTests("*"); return true;`,
  );
  const openedFaultWindow = await evaluateJson<{ opened: boolean }>(
    client,
    `
      ${roleHelpers()}
      const initial = findRoleWindow("initial");
      const opened = initial?.OpenBrowserWindow?.();
      if (opened) opened[roleKey] = "isolated-quarantine-fault";
      return { opened: Boolean(opened) };
    `,
  );
  assert(
    openedFaultWindow.opened,
    "could not open live quarantine trigger window",
  );
  await waitFor(
    () =>
      evaluateJson<boolean>(
        client,
        `${roleHelpers()} const win = findRoleWindow("isolated-quarantine-fault"); return Boolean(win?.gBrowserInit?.delayedStartupFinished && win.PanelUI?.panel);`,
      ),
    (ready) => ready,
    "isolated live-quarantine trigger startup",
  );
  const transition = await waitFor(
    () =>
      evaluateJson<{ record: HostRecord; windowIds: string[] }>(
        client,
        `const host = window[${
          JSON.stringify(hostKey)
        }]; return { record: host.diagnostics, windowIds: host.windowIdsForTests };`,
      ),
    (value) => value.record.blockedReason !== null,
    "isolated live post-commit attach quarantine",
    30_000,
  );
  assert(
    transition.record.blockedReason !== null &&
      transition.record.desiredEnabled &&
      transition.record.lastOutcome !== null,
    `isolated host fault did not quarantine the live generation: ${
      JSON.stringify(transition)
    }`,
  );
  await openMenu(client, "initial");
  const afterFailure = await waitFor(
    () => menuSnapshot(client, "initial"),
    (value) => value?.popupOpen === true,
    "isolated live quarantine cleanup menu",
  );
  assertMenuAbsent(afterFailure, "isolated normal", false);
  await openMenu(client, "late-normal");
  const lateAfterFailure = await waitFor(
    () => menuSnapshot(client, "late-normal"),
    (value) => value?.popupOpen === true,
    "isolated live quarantine late-normal cleanup menu",
  );
  assertMenuAbsent(lateAfterFailure, "isolated late normal", false);
  const registry = await registrySnapshot(client);
  assert(
    !registry.published && !registry.enabled,
    `isolated quarantined generation remained published: ${
      JSON.stringify(registry)
    }`,
  );
  const persisted = JSON.parse(
    await Deno.readTextFile(path.join(testRoot, "state.json")),
  ) as HostRecord;
  assert(
    persisted.blockedReason === transition.record.blockedReason &&
      persisted.desiredEnabled,
    "isolated live quarantine was not durably recorded",
  );
  const shutdown = await evaluateAsyncJson<{
    errors: string[];
    timedOut: boolean;
  }>(
    client,
    `
      const host = window[${JSON.stringify(hostKey)}];
      delete window[${JSON.stringify(hostKey)}];
      return await host.shutdown();
    `,
  );
  assert(
    shutdown.errors.length === 0 && !shutdown.timedOut,
    "isolated live quarantine host did not shut down cleanly",
  );
  observe("failure-states.isolated-live-quarantine", {
    storage: "unique feature-* test root",
    testRoot,
    initialRecord: opened,
    registryBeforeFault,
    openedFaultWindow,
    transition,
    menuAfterCleanup: afterFailure,
    lateMenuAfterCleanup: lateAfterFailure,
    registryAfterCleanup: registry,
    shutdown,
  });
}

async function runScenario(
  scenario: Scenario,
  acceptanceMode: AcceptanceMode,
  caseVerdicts: string[],
  phaseVerdicts: ScenarioPhaseVerdict[],
  failureState?: FailureStateScenario,
): Promise<void> {
  const phaseVerdict: ScenarioPhaseVerdict = {
    scenario: scenario.name,
    canonicalHostCases: scenario.selected && acceptanceMode !== "startup" &&
        acceptanceMode !== "failure-states"
      ? "NOT_RUN"
      : "NOT_APPLICABLE",
    isolatedPendingMovement: scenario.selected &&
        acceptanceMode === "lifecycle"
      ? "NOT_RUN"
      : "NOT_APPLICABLE",
  };
  phaseVerdicts.push(phaseVerdict);
  const profileDir = path.join(
    ARTIFACT_ROOT,
    failureState?.profileName ?? `${scenario.name}-profile`,
  );
  const profileRoot = path.join(profileDir, HUB_ROOT_NAME);
  const prefsPath = path.join(profileDir, "prefs.js");
  const userPath = path.join(profileDir, "user.js");
  const seededPrefs = `user_pref("${SELECTED_PREF}", ${scenario.selected});\n`;
  let startAttempted = false;
  let client: MarionetteClient | null = null;
  let marionettePort: number | null = null;
  let scenarioError: unknown;
  let scenarioFailed = false;
  let cleanupFailure: AggregateError | null = null;

  assert(
    !(await pathExists(profileDir)),
    `${scenario.name} profile path already exists: ${profileDir}`,
  );
  await Deno.mkdir(profileDir, { recursive: true });
  await Deno.writeTextFile(prefsPath, seededPrefs, { createNew: true });
  if (failureState) {
    await Deno.mkdir(profileRoot, { recursive: true });
    await Deno.writeFile(
      path.join(profileRoot, "state.json"),
      failureState.stateBytes,
      { createNew: true },
    );
  } else {
    assert(
      !(await pathExists(profileRoot)),
      "fresh profile already has HUB root",
    );
  }
  observe(`${scenario.name}.profile`, {
    profileDir,
    prefsPath,
    seededPrefs: seededPrefs.trim(),
    hubRootAbsentBeforeStartup: failureState === undefined,
    seededFailureState: failureState?.name ?? null,
  });

  try {
    startAttempted = true;
    const startOutput = await runDevTool(profileDir, [
      "start",
      "--port-offset",
      "auto",
    ]);
    assert(
      startOutput.includes("Browser ready!"),
      "dev-tool did not report browser ready",
    );
    assert(
      await pathExists(userPath),
      "dev launcher did not write dedicated user.js",
    );
    const generatedUserJs = await Deno.readTextFile(userPath);
    assert(
      !generatedUserJs.includes(SELECTED_PREF),
      "generated user.js overrides the seeded startup selection",
    );
    assert(
      (await Deno.readTextFile(prefsPath)).includes(seededPrefs.trim()),
      "seeded startup selection was removed from prefs.js",
    );
    const portText = await Deno.readTextFile(MARIONETTE_PORT_FILE);
    marionettePort = Number(portText.trim());
    assert(
      Number.isInteger(marionettePort) && marionettePort > 0,
      `invalid Marionette port: ${portText}`,
    );
    const activeProcesses = await listDevProcesses();
    assert(
      activeProcesses.some((process) =>
        hasExactProfileArgument(process, profileDir)
      ),
      "started browser does not use the dedicated profile argument",
    );
    observe(`${scenario.name}.launcher`, {
      profileDir,
      userJs: generatedUserJs,
      marionettePort,
      startOutput,
      browserUsesExactProfile: true,
    });

    client = await MarionetteClient.connect(marionettePort);
    await client.setContext("chrome");
    if (failureState) {
      const installed = await evaluateJson<boolean>(
        client,
        `
        const key = ${JSON.stringify(BOOTSTRAP_ERROR_OBSERVER_KEY)};
        if (window[key]) return false;
        const state = { messages: [], observer: null };
        const observer = {
          observe(subject, topic, data) {
            if (topic !== "console-api-log-event") return;
            let event = subject?.wrappedJSObject ?? null;
            if (!event && data) {
              try { event = JSON.parse(data); } catch {}
            }
            const stringifyArgument = (value) => {
              if (value instanceof Error) {
                return { name: value.name, message: value.message, stack: value.stack };
              }
              try {
                const json = JSON.parse(JSON.stringify(value));
                if (
                  json && typeof json === "object" &&
                  Object.keys(json).length === 0
                ) return { type: Object.prototype.toString.call(value), string: String(value) };
                return json;
              }
              catch { return String(value); }
            };
            const message = JSON.stringify({
              level: event?.level ?? null,
              arguments: Array.from(event?.arguments ?? [], stringifyArgument),
            });
            if (message.includes("[HubPanelMenuBootstrap] feature startup failed")) {
              state.messages.push(message);
            }
          },
        };
        state.observer = observer;
        window[key] = state;
        Services.obs.addObserver(observer, "console-api-log-event");
        return true;
      `,
      );
      assert(installed, "bootstrap console observer could not be installed");
    }
    const startup = await waitFor(
      () => startupSnapshot(client!),
      (value) => value?.delayedStartupFinished === true,
      `${scenario.name} browser delayed startup`,
    );
    assert(startup !== null, "initial normal browser window was not found");
    assert(!startup.isPrivate, "startup window is unexpectedly private");
    assert(
      startup.windowType === "navigator:browser",
      `unexpected startup window type: ${startup.windowType}`,
    );
    assert(
      startup.mode === "dev" || startup.mode === "test",
      `unexpected startup mode: ${startup.mode}`,
    );
    assert(
      startup.selected === scenario.selected,
      `startup preference mismatch: expected ${scenario.selected}, got ${startup.selected}`,
    );
    assert(
      !startup.popupOpen,
      "app menu was open before the driver interaction",
    );
    observe(`${scenario.name}.pre-interaction-dom`, startup);

    if (failureState) {
      await runFailureStateAcceptance(
        client,
        profileDir,
        failureState,
        caseVerdicts,
      );
      return;
    }

    const initialRecord = await readHubRecord(profileDir);
    observe(`${scenario.name}.canonical-record-before-menu`, initialRecord);
    if (scenario.selected) {
      const record = await waitFor(
        () => readHubRecord(profileDir),
        (value) => isInitialHubRecord(value),
        "opt-in canonical initial record",
      );
      observe(`${scenario.name}.canonical-record-ready`, record);
    } else {
      assert(
        !(await pathExists(profileRoot)) && initialRecord === null,
        "opt-out startup created the canonical HUB root",
      );
    }

    let registry = await registrySnapshot(client);
    if (scenario.selected) {
      registry = await waitFor(
        () => registrySnapshot(client!),
        (value) => value.published && value.enabled && value.connections >= 1,
        "opt-in shared registry publication",
      );
      assert(
        registry.connections === 1,
        `expected one selected normal adapter before opening more windows, got ${registry.connections}`,
      );
    } else {
      assert(
        !registry.published && !registry.enabled && registry.connections === 0,
        `opt-out startup unexpectedly published registry state: ${
          JSON.stringify(registry)
        }`,
      );
    }
    observe(`${scenario.name}.shared-registry-before-menu`, registry);

    await openMenu(client, "initial");
    const initialMenu = await waitFor(
      () => menuSnapshot(client!, "initial"),
      (value) => value?.popupOpen === true && value.button !== null,
      `${scenario.name} initial menu rendering`,
    );
    assertMenu(initialMenu, "initial normal", false);
    observe(`${scenario.name}.initial-normal-menu`, initialMenu);
    log(
      `[PASS] ${scenario.name}: startup-selected normal window rendered enabled HUB button`,
    );

    const beforePrivate = await registrySnapshot(client);
    const openedPrivate = await evaluateJson<{ opened: boolean }>(
      client,
      `
      ${roleHelpers()}
      const initial = findRoleWindow("initial");
      if (!initial || typeof initial.OpenBrowserWindow !== "function") {
        return { opened: false };
      }
      const opened = initial.OpenBrowserWindow({ private: true });
      if (opened) opened[roleKey] = "private";
      return { opened: Boolean(opened) };
    `,
    );
    assert(openedPrivate.opened, "could not open a private browser window");
    const privateWindow = await waitFor(
      () =>
        evaluateJson<{
          found: boolean;
          delayedStartupFinished: boolean;
          isPrivate: boolean;
        }>(
          client!,
          `
        ${roleHelpers()}
        const win = findRoleWindow("private");
        return {
          found: Boolean(win),
          delayedStartupFinished: Boolean(win?.gBrowserInit?.delayedStartupFinished),
          isPrivate: win ? privateUtils.isWindowPrivate(win) : false,
        };
      `,
        ),
      (value) => value.found && value.delayedStartupFinished && value.isPrivate,
      `${scenario.name} private window startup`,
    );
    assert(privateWindow.isPrivate, "opened window is not private");
    const afterPrivateStartup = await registrySnapshot(client);
    assert(
      afterPrivateStartup.connections === beforePrivate.connections,
      "private window registered an experimental app-menu adapter",
    );
    await openMenu(client, "private");
    const privateMenu = await waitFor(
      () => menuSnapshot(client!, "private"),
      (value) => value?.popupOpen === true && value.button !== null,
      `${scenario.name} private legacy menu rendering`,
    );
    assertMenu(privateMenu, "private legacy", true);
    const afterPrivateMenu = await registrySnapshot(client);
    assert(
      afterPrivateMenu.connections === beforePrivate.connections,
      "private legacy menu opened an experimental adapter connection",
    );
    observe(`${scenario.name}.private-legacy-menu`, {
      menu: privateMenu,
      registryBefore: beforePrivate,
      registryAfter: afterPrivateMenu,
    });
    log(
      `[PASS] ${scenario.name}: private window used legacy menu without a registry connection`,
    );

    const flipped = !scenario.selected;
    const prefAfterFlip = await evaluateJson<boolean>(
      client,
      `
      Services.prefs.setBoolPref("${SELECTED_PREF}", ${flipped});
      return Services.prefs.getBoolPref("${SELECTED_PREF}", false);
    `,
    );
    assert(
      prefAfterFlip === flipped,
      "startup selection preference did not flip",
    );
    observe(`${scenario.name}.preference-flipped-after-startup`, {
      before: scenario.selected,
      after: prefAfterFlip,
    });

    const openedSecond = await evaluateJson<{ opened: boolean }>(
      client,
      `
      ${roleHelpers()}
      const initial = findRoleWindow("initial");
      if (!initial || typeof initial.OpenBrowserWindow !== "function") {
        return { opened: false };
      }
      const opened = initial.OpenBrowserWindow();
      if (opened) opened[roleKey] = "second-normal";
      return { opened: Boolean(opened) };
    `,
    );
    assert(
      openedSecond.opened,
      "could not open a second normal browser window",
    );
    const secondStartup = await waitFor(
      () =>
        evaluateJson<StartupSnapshot | null>(
          client!,
          `
        ${roleHelpers()}
        const win = findRoleWindow("second-normal");
        if (!win) return null;
        const trigger = win.document.getElementById("PanelUI-menu-button");
        return {
          delayedStartupFinished: Boolean(win.gBrowserInit?.delayedStartupFinished),
          mode: Services.prefs.getStringPref("nora.startup.mode", ""),
          selected: Services.prefs.getBoolPref("${SELECTED_PREF}", false),
          isPrivate: privateUtils.isWindowPrivate(win),
          windowType: win.document.documentElement?.getAttribute("windowtype") ?? "",
          popupOpen: trigger?.getAttribute("open") === "true",
          buttonPresent: Boolean(win.document.getElementById("${BUTTON_ID}")),
          settingsPresent: Boolean(win.document.getElementById("appMenu-settings-button")),
        };
      `,
        ),
      (value) => value?.delayedStartupFinished === true,
      `${scenario.name} second normal window startup`,
    );
    assert(
      secondStartup !== null && !secondStartup.isPrivate,
      "second window is not normal",
    );
    assert(
      secondStartup.selected === flipped,
      "second window did not observe the live preference value",
    );
    const secondRegistryBeforeMenu = await registrySnapshot(client);
    if (scenario.selected) {
      assert(
        secondRegistryBeforeMenu.published &&
          secondRegistryBeforeMenu.enabled &&
          secondRegistryBeforeMenu.connections === 2,
        `captured opt-in did not attach the second normal adapter: ${
          JSON.stringify(secondRegistryBeforeMenu)
        }`,
      );
    } else {
      assert(
        !secondRegistryBeforeMenu.published &&
          !secondRegistryBeforeMenu.enabled &&
          secondRegistryBeforeMenu.connections === 0,
        `captured opt-out changed after the live pref flip: ${
          JSON.stringify(secondRegistryBeforeMenu)
        }`,
      );
    }
    observe(`${scenario.name}.second-normal-before-menu`, {
      startup: secondStartup,
      registry: secondRegistryBeforeMenu,
    });

    await openMenu(client, "second-normal");
    const secondMenu = await waitFor(
      () => menuSnapshot(client!, "second-normal"),
      (value) => value?.popupOpen === true && value.button !== null,
      `${scenario.name} second normal menu rendering`,
    );
    assertMenu(secondMenu, "second normal", false);
    const secondRegistryAfterMenu = await registrySnapshot(client);
    if (scenario.selected) {
      assert(
        secondRegistryAfterMenu.published &&
          secondRegistryAfterMenu.enabled &&
          secondRegistryAfterMenu.connections === 2,
        "captured opt-in second menu lost its adapter",
      );
    } else {
      assert(
        !secondRegistryAfterMenu.published &&
          !secondRegistryAfterMenu.enabled &&
          secondRegistryAfterMenu.connections === 0,
        "captured opt-out created registry publication after the live pref flip",
      );
      assert(
        !(await pathExists(profileRoot)),
        "opt-out created the canonical HUB root after the live pref flip",
      );
    }
    observe(`${scenario.name}.second-normal-after-pref-flip`, {
      menu: secondMenu,
      registry: secondRegistryAfterMenu,
      canonicalRecord: await readHubRecord(profileDir),
    });
    log(
      `[PASS] ${scenario.name}: preference flip did not change captured startup selection`,
    );
    if (acceptanceMode !== "startup" && scenario.selected) {
      try {
        await runHostLifecycle(
          client,
          profileDir,
          caseVerdicts,
          acceptanceMode === "lifecycle",
        );
        phaseVerdict.canonicalHostCases = "PASS";
      } catch (error) {
        phaseVerdict.canonicalHostCases = "FAIL";
        throw error;
      }
      if (acceptanceMode === "lifecycle") {
        try {
          await runIsolatedPendingMovementAcceptance(
            client,
            profileDir,
            caseVerdicts,
          );
          phaseVerdict.isolatedPendingMovement = "PASS";
        } catch (error) {
          phaseVerdict.isolatedPendingMovement = "FAIL";
          throw error;
        }
      }
    }
  } catch (error) {
    scenarioFailed = true;
    scenarioError = error;
    observe(`${scenario.name}.failure`, {
      error: error instanceof Error ? error.message : String(error),
      stack: error instanceof Error ? error.stack : null,
    });
  } finally {
    const cleanupErrors: unknown[] = [];
    try {
      await client?.close();
    } catch (error) {
      cleanupErrors.push(error);
    }
    if (startAttempted) {
      try {
        const stopOutput = await runDevTool(profileDir, ["stop"]);
        assert(
          /Stopped \(\d+ processes\)\./.test(stopOutput),
          "dev-tool did not report completed owned-process cleanup",
        );
      } catch (stopError) {
        cleanupErrors.push(stopError);
      }
      try {
        observe(
          `${scenario.name}.dev-server-log`,
          await preserveDevServerLog(
            `${scenario.name}-${path.basename(profileDir)}`,
          ),
        );
      } catch (error) {
        cleanupErrors.push(error);
      }
    }
    try {
      await assertNoProfileProcesses(profileDir);
    } catch (error) {
      cleanupErrors.push(error);
    }
    let marionetteListenerAbsent = marionettePort === null;
    if (marionettePort !== null) {
      try {
        marionetteListenerAbsent = !(await tcpPortOpen(marionettePort));
        assert(
          marionetteListenerAbsent,
          `${scenario.name} Marionette listener remains on ${marionettePort}`,
        );
      } catch (error) {
        cleanupErrors.push(error);
      }
    }
    try {
      assert(
        !(await pathExists(MARIONETTE_PORT_FILE)),
        `${scenario.name} left the shared Marionette port file behind`,
      );
    } catch (error) {
      cleanupErrors.push(error);
    }
    observe(`${scenario.name}.cleanup`, {
      profilePreserved: await pathExists(profileDir),
      cleanupChecksPassed: cleanupErrors.length === 0,
      marionettePort,
      marionetteListenerAbsent,
      errors: cleanupErrors.map((error) =>
        error instanceof Error ? error.message : String(error)
      ),
    });
    if (cleanupErrors.length > 0) {
      cleanupFailure = new AggregateError(
        cleanupErrors,
        "native cleanup failed",
      );
    }
  }
  if (scenarioFailed) {
    if (cleanupFailure) {
      throw new AggregateError(
        [scenarioError, cleanupFailure],
        "startup scenario and cleanup failed",
      );
    }
    throw scenarioError;
  }
  if (cleanupFailure) throw cleanupFailure;
}

async function main(): Promise<void> {
  const acceptanceMode = parseAcceptanceMode(Deno.args);
  assert(
    path.resolve(PROFILE_ROOT) !==
      path.resolve(PROJECT_ROOT, "_dist", "profile", "test"),
    "startup driver cannot use the shared test profile directory",
  );
  await Deno.mkdir(PROFILE_ROOT, { recursive: true });
  const profileParent = await Deno.stat(PROFILE_ROOT);
  assert(
    profileParent.isDirectory,
    `profile artifact parent is missing: ${PROFILE_ROOT}`,
  );
  assert(
    !(await pathExists(ARTIFACT_ROOT)),
    "startup artifact path already exists",
  );
  await Deno.mkdir(ARTIFACT_ROOT, { recursive: true });
  logPath = path.join(ARTIFACT_ROOT, "driver.log");
  observationPath = path.join(ARTIFACT_ROOT, "observations.jsonl");
  await Deno.writeTextFile(logPath, "", { createNew: true });
  await Deno.writeTextFile(observationPath, "", { createNew: true });

  await assertNoConflictingDevProcesses();
  const baselineDevToolState = await snapshotDevToolState();
  const sharedProfilePath = path.join(
    PROJECT_ROOT,
    "_dist",
    "profile",
    "test",
  );
  const sharedProfileBeforeExists = acceptanceMode === "failure-states"
    ? await pathExists(sharedProfilePath)
    : null;
  const sharedProfileBeforeHash = sharedProfileBeforeExists
    ? await hashDirectoryContents(sharedProfilePath)
    : null;
  const sharedDevStateBeforeHash = acceptanceMode === "failure-states"
    ? await sha256Text(JSON.stringify(baselineDevToolState))
    : null;
  if (acceptanceMode === "failure-states") {
    observe("shared-baseline.before", {
      profilePath: sharedProfilePath,
      profileExists: sharedProfileBeforeExists,
      profileManifestSha256: sharedProfileBeforeHash,
      devToolStateSha256: sharedDevStateBeforeHash,
    });
  }
  const baselineProcessRecords = readDevProcesses();
  const baselineProcesses = await listDevProcesses();
  const liveBaselineRecords = baselineProcessRecords.filter((identity) =>
    baselineProcesses.some((process) => matchesDevIdentity(process, identity))
  );
  assert(
    liveBaselineRecords.length === 0,
    `dev-tool state contains live process identities: ${
      liveBaselineRecords.map((identity) => identity.pid).join(",")
    }`,
  );
  const oldPortFile = baselineDevToolState.find((state) =>
    state.path === MARIONETTE_PORT_FILE
  )?.contents;
  if (oldPortFile !== null && oldPortFile !== undefined) {
    const oldPort = Number(oldPortFile.trim());
    if (Number.isInteger(oldPort) && oldPort > 0) {
      assert(
        !(await tcpPortOpen(oldPort)),
        `pre-existing Marionette listener is active on ${oldPort}`,
      );
    }
  }
  await Deno.writeTextFile(
    path.join(ARTIFACT_ROOT, "dev-tool-state-before.json"),
    `${JSON.stringify(baselineDevToolState, null, 2)}\n`,
    { createNew: true },
  );
  observe("driver.start", {
    runId: RUN_ID,
    profileRoot: ARTIFACT_ROOT,
    runtimeDistribution: "release",
    patcherSkipped: true,
  });

  const scenarioPlans: {
    scenario: Scenario;
    failureState?: FailureStateScenario;
  }[] = acceptanceMode === "failure-states"
    ? [{
      scenario: { name: "invalid-canonical-state", selected: true },
      failureState: {
        name: "invalid-canonical-state",
        profileName: "failure-invalid-profile",
        stateBytes: new TextEncoder().encode('{"featureId":'),
      },
    }]
    : [
      { scenario: { name: "opt-in", selected: true } },
      { scenario: { name: "opt-out", selected: false } },
    ];
  const scenarioResults: {
    name: Scenario["name"];
    status: "PASS" | "FAIL";
    error: string | null;
  }[] = [];
  const phaseVerdicts: ScenarioPhaseVerdict[] = [];
  const caseVerdicts: string[] = [];
  const scenarioErrors: unknown[] = [];
  let quarantineSeedAttempted = false;
  for (const plan of scenarioPlans) {
    const { scenario, failureState } = plan;
    try {
      await assertNoConflictingDevProcesses();
      await runScenario(
        scenario,
        acceptanceMode,
        caseVerdicts,
        phaseVerdicts,
        failureState,
      );
      scenarioResults.push({
        name: scenario.name,
        status: "PASS",
        error: null,
      });
    } catch (error) {
      scenarioErrors.push(error);
      scenarioResults.push({
        name: scenario.name,
        status: "FAIL",
        error: error instanceof Error ? error.message : String(error),
      });
    }
    if (
      acceptanceMode === "failure-states" &&
      scenario.name === "invalid-canonical-state" && !quarantineSeedAttempted
    ) {
      quarantineSeedAttempted = true;
      try {
        await assertNoConflictingDevProcesses();
        const stateBytes = await generateQuarantinedState();
        scenarioPlans.push({
          scenario: { name: "persisted-quarantine", selected: true },
          failureState: {
            name: "persisted-quarantine",
            profileName: "failure-quarantine-profile",
            stateBytes,
          },
        });
      } catch (error) {
        scenarioErrors.push(error);
        scenarioResults.push({
          name: "persisted-quarantine",
          status: "FAIL",
          error: error instanceof Error ? error.message : String(error),
        });
      }
    }
  }
  const runFailed = scenarioErrors.length > 0;
  const runError = runFailed
    ? new AggregateError(scenarioErrors, "one or more scenarios failed")
    : null;
  let restoreError: unknown;
  let restoreFailed = false;
  let sharedProfileAfterHash: string | null = null;
  let sharedDevStateAfterHash: string | null = null;
  try {
    await restoreDevToolState(baselineDevToolState);
    const restoredFiles: { path: string; restored: boolean }[] = [];
    for (const state of baselineDevToolState) {
      let restored: boolean;
      if (state.contents !== null) {
        restored = await Deno.readTextFile(state.path) === state.contents;
      } else {
        restored = !(await pathExists(state.path));
      }
      restoredFiles.push({ path: state.path, restored });
    }
    const allRestored = restoredFiles.every((state) => state.restored);
    observe("dev-tool-state-restore-check", {
      allRestored,
      files: restoredFiles,
    });
    assert(allRestored, "one or more dev-tool state files were not restored");
    if (acceptanceMode === "failure-states") {
      const sharedProfileAfterExists = await pathExists(sharedProfilePath);
      sharedProfileAfterHash = sharedProfileAfterExists
        ? await hashDirectoryContents(sharedProfilePath)
        : null;
      sharedDevStateAfterHash = await sha256Text(
        JSON.stringify(await snapshotDevToolState()),
      );
      const sharedProfileMatches = sharedProfileAfterExists ===
          sharedProfileBeforeExists &&
        sharedProfileAfterHash === sharedProfileBeforeHash;
      observe("shared-baseline.after", {
        profilePath: sharedProfilePath,
        profileExists: sharedProfileAfterExists,
        profileManifestSha256: sharedProfileAfterHash,
        devToolStateSha256: sharedDevStateAfterHash,
        profileMatches: sharedProfileMatches,
        devToolStateMatches: sharedDevStateAfterHash ===
          sharedDevStateBeforeHash,
      });
      assert(
        sharedProfileMatches,
        "shared test profile changed during failure-state acceptance",
      );
      assert(
        sharedDevStateAfterHash === sharedDevStateBeforeHash,
        "shared dev-tool state changed during acceptance",
      );
    }
    const originalPort = baselineDevToolState.find((state) =>
      state.path === MARIONETTE_PORT_FILE
    )?.contents;
    if (originalPort !== null && originalPort !== undefined) {
      const port = Number(originalPort.trim());
      if (Number.isInteger(port) && port > 0) {
        assert(
          !(await tcpPortOpen(port)),
          `restored baseline port ${port} is listening`,
        );
      }
    }
    observe("dev-tool-state-restored", { files: restoredFiles });
  } catch (error) {
    restoreFailed = true;
    restoreError = error;
    observe("dev-tool-state-restoration-failed", {
      error: error instanceof Error ? error.message : String(error),
    });
  }
  const verdict = {
    status: runFailed || restoreFailed ? "FAIL" : "PASS",
    mode: acceptanceMode,
    focusContinuity: acceptanceMode === "hmr-only"
      ? "skipped"
      : acceptanceMode === "lifecycle"
      ? "executed"
      : "not-requested",
    scenarios: scenarioResults,
    lifecyclePhases: phaseVerdicts,
    caseVerdicts,
    sharedBaseline: {
      profileBeforeSha256: sharedProfileBeforeHash,
      profileAfterSha256: sharedProfileAfterHash,
      devToolStateBeforeSha256: sharedDevStateBeforeHash,
      devToolStateAfterSha256: sharedDevStateAfterHash,
    },
    scenarioErrors: scenarioResults.filter((result) => result.error !== null),
    restoreError: restoreFailed
      ? restoreError instanceof Error
        ? restoreError.message
        : String(restoreError)
      : null,
    artifacts: ARTIFACT_ROOT,
  };
  await Deno.writeTextFile(
    path.join(ARTIFACT_ROOT, "verdict.json"),
    `${JSON.stringify(verdict, null, 2)}\n`,
    { createNew: true },
  );
  const acceptanceLabel = acceptanceMode === "startup"
    ? "STARTUP"
    : acceptanceMode === "lifecycle"
    ? "STARTUP + HUB LIFECYCLE"
    : acceptanceMode === "hmr-only"
    ? "STARTUP + HUB LIFECYCLE + HMR (FOCUS SKIPPED)"
    : "STARTUP FAILURE STATES";
  log(
    `${acceptanceLabel} ACCEPTANCE: ${verdict.status} (${
      scenarioResults.map(
        (result) =>
          result.status === "PASS" ? result.name : `${result.name}:FAIL`,
      ).join(", ")
    })`,
  );
  log(`Artifacts: ${ARTIFACT_ROOT}`);
  if (runFailed && restoreFailed) {
    throw new AggregateError(
      [runError, restoreError],
      "startup acceptance and dev-tool state restoration failed",
    );
  }
  if (runError !== null) throw runError;
  if (restoreFailed) throw restoreError;
}

if (import.meta.main) {
  try {
    await main();
  } catch (error) {
    const message = error instanceof Error
      ? error.stack ?? error.message
      : String(error);
    if (logPath) log(`STARTUP ACCEPTANCE: FAIL\n${message}`);
    console.error(message);
    Deno.exitCode = 1;
  }
}
