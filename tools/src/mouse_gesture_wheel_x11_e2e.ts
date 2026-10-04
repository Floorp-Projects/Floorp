// SPDX-License-Identifier: MPL-2.0

import { MarionetteClient } from "./browser_connector.ts";

const ENABLED_PREF = "floorp.mousegesture.enabled";
const CONFIG_PREF = "floorp.mousegesture.config";
const PREVENTION_TIMEOUT_MS = 500;
const SAFE_SCROLL_UP_ACTION = "gecko-zoom-in";
const SAFE_SCROLL_DOWN_ACTION = "gecko-zoom-out";
const FORBIDDEN_ACTION = "gecko-close-tab";
const NORMALIZED_SCROLL_UP_ACTION = "gecko-show-previous-tab";
const WINDOW_TITLE_MARKER = "Floorp wheel native X11 E2E";
const SETTINGS_ROUTE = "#/features/gesture";
const SETTINGS_URL = `http://localhost:5183/${SETTINGS_ROUTE}`;
const SETTINGS_WAIT_TIMEOUT_MS = 60_000;
const SETTINGS_WAIT_INTERVAL_MS = 100;
const STARTUP_WAIT_TIMEOUT_MS = 180_000;
const WINDOW_DISCOVERY_TIMEOUT_MS = 10_000;
const WINDOW_DISCOVERY_INTERVAL_MS = 100;
const REPEAT_SAFE_WHEEL_ACTIONS = [
  "gecko-show-previous-tab",
  "gecko-show-next-tab",
  "gecko-scroll-line-up",
  "gecko-scroll-line-down",
  "gecko-scroll-up",
  "gecko-scroll-down",
  "gecko-scroll-left",
  "gecko-scroll-right",
  "gecko-scroll-to-top",
  "gecko-scroll-to-bottom",
  "gecko-zoom-in",
  "gecko-zoom-out",
  "gecko-reset-zoom",
  "gecko-workspace-next",
  "gecko-workspace-previous",
  "gecko-show-next-search-result",
  "gecko-show-previous-search-result",
] as const;

if (Deno.build.os !== "linux") {
  throw new Error("Mouse gesture native X11 E2E requires Linux");
}

interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

interface WindowGeometry {
  id: string;
  x: number;
  y: number;
  width: number;
  height: number;
}

interface WheelActions {
  scrollUp: string;
  scrollDown: string;
}

interface OriginalState {
  originalZoom: number;
  hadEnabledUserValue: boolean;
  originalEnabled: boolean;
  hadConfigUserValue: boolean;
  originalConfig: string;
}

interface BrowserSetup {
  processId: number;
  baselineZoom: number;
  zoomInOne: number;
  zoomInTwo: number;
}

interface TargetPoint {
  x: number;
  y: number;
  devicePixelRatio: number;
  targetRect: Rect;
  contentScreenOrigin: { x: number; y: number };
}

interface PageState {
  counts: Record<string, number>;
  events: Array<{
    type: string;
    button: number;
    buttons: number;
    deltaY: number | null;
    defaultPrevented: boolean;
  }>;
}

interface WheelSettingsSnapshot {
  url: string;
  title: string;
  readyState: string;
  bodyText: string;
  rootChildCount: number;
  nrSettingsSend: string;
  nrSettingsRegisterReceiveCallback: string;
  nrSPing: string;
  allSelectCount: number;
  wheelSelectCount: number;
  selectedValues: string[];
  disabled: boolean[];
  optionValues: string[][];
  enabled: boolean | null;
  wheelEnabled: boolean | null;
}

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(message);
  }
}

function extractHandle(response: unknown): string {
  if (typeof response === "string") {
    return response;
  }
  if (response && typeof response === "object") {
    const record = response as Record<string, unknown>;
    if (typeof record.handle === "string") {
      return record.handle;
    }
    if (typeof record.value === "string") {
      return record.value;
    }
  }
  throw new Error(
    `Marionette did not return a window handle: ${JSON.stringify(response)}`,
  );
}

function extractHandles(response: unknown): string[] {
  const value = Array.isArray(response)
    ? response
    : response && typeof response === "object"
    ? (response as Record<string, unknown>).value
    : null;
  if (Array.isArray(value) && value.every((item) => typeof item === "string")) {
    return value as string[];
  }
  throw new Error(
    `Marionette did not return window handles: ${JSON.stringify(response)}`,
  );
}

const textDecoder = new TextDecoder();

async function runXdotool(args: string[]): Promise<string> {
  // Each invocation is a direct process with a fixed argument array. XTEST
  // input is global and targets the sole visible Floorp top-level.
  const output = await new Deno.Command("xdotool", {
    args,
    stdout: "piped",
    stderr: "piped",
  }).output();
  const stdout = textDecoder.decode(output.stdout).trim();
  const stderr = textDecoder.decode(output.stderr).trim();
  if (!output.success) {
    throw new Error(
      `xdotool ${JSON.stringify(args)} failed with exit ${output.code}: ${
        stderr || stdout
      }`,
    );
  }
  return stdout;
}

function parseWindowIds(output: string): string[] {
  const ids = output.split(/\r?\n/).map((line) => line.trim()).filter(Boolean);
  assert(
    ids.every((id) => /^\d+$/.test(id)),
    `xdotool returned invalid window IDs: ${JSON.stringify(ids)}`,
  );
  return [...new Set(ids)];
}

function parseWindowGeometry(id: string, output: string): WindowGeometry {
  const position = output.match(/Position:\s*(-?\d+),(-?\d+)/);
  const size = output.match(/Geometry:\s*(\d+)x(\d+)/);
  assert(
    position && size,
    `Could not parse xdotool geometry for ${id}: ${output}`,
  );
  return {
    id,
    x: Number(position[1]),
    y: Number(position[2]),
    width: Number(size[1]),
    height: Number(size[2]),
  };
}

async function inspectSoleVisibleFloorpWindow(
  processId: number,
): Promise<WindowGeometry> {
  const ids = parseWindowIds(
    await runXdotool([
      "search",
      "--all",
      "--onlyvisible",
      "--maxdepth",
      "1",
      "--pid",
      String(processId),
      "--name",
      ".*",
    ]),
  );
  assert(
    ids.length === 1,
    `Expected exactly one visible top-level Floorp window for PID ${processId}, got ${
      JSON.stringify(ids)
    }`,
  );

  const id = ids[0];
  const [reportedPid, title, geometry] = await Promise.all([
    runXdotool(["getwindowpid", id]),
    runXdotool(["getwindowname", id]),
    runXdotool(["getwindowgeometry", id]),
  ]);
  assert(
    reportedPid === String(processId),
    `X11 window ${id} belongs to PID ${reportedPid}, expected Floorp PID ${processId}`,
  );
  assert(
    title.includes(WINDOW_TITLE_MARKER),
    `X11 window ${id} does not have the Floorp test title: ${title}`,
  );
  return parseWindowGeometry(id, geometry);
}

async function findSoleVisibleFloorpWindow(
  processId: number,
): Promise<WindowGeometry> {
  const startedAt = Date.now();
  let lastError: unknown;

  while (Date.now() - startedAt < WINDOW_DISCOVERY_TIMEOUT_MS) {
    try {
      return await inspectSoleVisibleFloorpWindow(processId);
    } catch (error) {
      lastError = error;
      await new Promise((resolve) =>
        setTimeout(resolve, WINDOW_DISCOVERY_INTERVAL_MS)
      );
    }
  }

  throw new Error(
    `Timed out after ${WINDOW_DISCOVERY_TIMEOUT_MS}ms waiting for exactly one visible Floorp window for PID ${processId}: ${
      lastError instanceof Error ? lastError.message : String(lastError)
    }`,
  );
}

const client = await MarionetteClient.connect();
let originalHandle: string | null = null;
let testHandle: string | null = null;
let originalState: OriginalState | null = null;
let secondaryButtonHeld = false;

async function withContentContext<T>(
  operation: () => Promise<T>,
): Promise<T> {
  await client.setContext("content");
  return await operation();
}

async function getContentWindowHandle(): Promise<string> {
  return extractHandle(
    await withContentContext(() =>
      client.send("WebDriver:GetWindowHandle", {})
    ),
  );
}

async function getContentWindowHandles(): Promise<string[]> {
  return extractHandles(
    await withContentContext(() =>
      client.send("WebDriver:GetWindowHandles", {})
    ),
  );
}

async function newContentTab(): Promise<string> {
  return extractHandle(
    await withContentContext(() =>
      client.send("WebDriver:NewWindow", { type: "tab" })
    ),
  );
}

async function switchToContentWindow(handle: string): Promise<void> {
  await withContentContext(() =>
    client.send("WebDriver:SwitchToWindow", { handle })
  );
}

async function closeContentWindow(): Promise<void> {
  await withContentContext(() => client.send("WebDriver:CloseWindow", {}));
}

async function moveNativePointer(x: number, y: number): Promise<void> {
  await runXdotool(["mousemove", "--sync", String(x), String(y)]);
  await new Promise((resolve) => setTimeout(resolve, 50));
}

async function pressNativeSecondary(): Promise<void> {
  // Mark the intended physical state before launching the helper so the outer
  // cleanup still attempts a release if the helper reports a partial failure.
  secondaryButtonHeld = true;
  await runXdotool(["mousedown", "3"]);
  await new Promise((resolve) => setTimeout(resolve, 50));
}

async function releaseNativeSecondary(): Promise<void> {
  await runXdotool(["mouseup", "3"]);
  secondaryButtonHeld = false;
  await new Promise((resolve) => setTimeout(resolve, 100));
}

async function sendNativeWheel(button: 4 | 5): Promise<void> {
  await runXdotool(["click", String(button)]);
  await new Promise((resolve) => setTimeout(resolve, 100));
}

async function runNativeSecondaryWheelCycle(
  x: number,
  y: number,
  body: () => Promise<void>,
): Promise<void> {
  let failure: unknown = null;
  await moveNativePointer(x, y);
  await pressNativeSecondary();
  try {
    await body();
  } catch (error) {
    failure = error;
  }

  let releaseError: unknown = null;
  try {
    await releaseNativeSecondary();
  } catch (error) {
    releaseError = error;
  }

  if (failure !== null) {
    if (releaseError !== null) {
      console.error("Native RMB release error:", releaseError);
    }
    throw failure;
  }
  if (releaseError !== null) {
    throw releaseError;
  }
}

async function resetPageState(): Promise<void> {
  await client.setContext("content");
  await client.executeScript(`
    window.__floorpWheelX11E2E.counts = {
      mousedown: 0,
      mouseup: 0,
      click: 0,
      auxclick: 0,
      dblclick: 0,
      contextmenu: 0,
      wheel: 0,
    };
    window.__floorpWheelX11E2E.events = [];
  `);
}

async function readPageState(): Promise<PageState> {
  await client.setContext("content");
  const raw = await client.executeScript(`
    return JSON.stringify(window.__floorpWheelX11E2E);
  `);
  return JSON.parse(raw) as PageState;
}

async function readZoom(): Promise<number> {
  await client.setContext("chrome");
  return await client.executeScript(`return ZoomManager.zoom;`) as number;
}

async function resetZoom(): Promise<number> {
  await client.setContext("chrome");
  await client.executeScript(`FullZoom.reset();`);
  return await readZoom();
}

async function readWheelActionsFromPref(): Promise<WheelActions | null> {
  await client.setContext("chrome");
  const raw = await client.executeScript(`
    const config = JSON.parse(
      Services.prefs.getStringPref(${JSON.stringify(CONFIG_PREF)}, "{}"),
    );
    return JSON.stringify(config.wheelActions ?? null);
  `);
  const parsed: unknown = JSON.parse(raw);
  if (
    parsed === null ||
    typeof parsed !== "object" ||
    Array.isArray(parsed)
  ) {
    return null;
  }
  const candidate = parsed as Record<string, unknown>;
  return typeof candidate.scrollUp === "string" &&
      typeof candidate.scrollDown === "string"
    ? candidate as unknown as WheelActions
    : null;
}

async function waitForWheelActions(
  expected: WheelActions,
  label: string,
): Promise<void> {
  let observed: WheelActions | null = null;
  for (let attempt = 0; attempt < 40; attempt++) {
    observed = await readWheelActionsFromPref();
    if (
      observed?.scrollUp === expected.scrollUp &&
      observed?.scrollDown === expected.scrollDown
    ) {
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  throw new Error(
    `${label}: expected wheel actions ${JSON.stringify(expected)}, got ${
      JSON.stringify(observed)
    }`,
  );
}

async function readWheelSettingsSnapshot(): Promise<WheelSettingsSnapshot> {
  await client.setContext("content");
  const raw = await client.executeScript(`
    const allowed = ${JSON.stringify(REPEAT_SAFE_WHEEL_ACTIONS)};
    const sortedAllowed = [...allowed].sort();
    const selects = [...document.querySelectorAll("select")];
    const enabledToggle = document.querySelector(
      '[data-setting="mouse-gesture-enabled"]',
    );
    const wheelToggle = document.querySelector(
      '[data-setting="mouse-gesture-wheel-enabled"]',
    );
    const wheelSelects = selects.filter((select) => {
      const values = [...select.options].map((option) => option.value).sort();
      return values.length === sortedAllowed.length &&
        values.every((value, index) => value === sortedAllowed[index]);
    });
    return JSON.stringify({
      url: location.href,
      title: document.title,
      readyState: document.readyState,
      bodyText: document.body?.innerText?.slice(0, 240) ?? "",
      rootChildCount: document.querySelector("#root")?.childElementCount ?? 0,
      nrSettingsSend: typeof globalThis.NRSettingsSend,
      nrSettingsRegisterReceiveCallback:
        typeof globalThis.NRSettingsRegisterReceiveCallback,
      nrSPing: typeof globalThis.NRSPing,
      allSelectCount: selects.length,
      wheelSelectCount: wheelSelects.length,
      selectedValues: wheelSelects.map((select) => select.value),
      disabled: wheelSelects.map((select) => select.disabled),
      optionValues: wheelSelects.map((select) =>
        [...select.options].map((option) => option.value)
      ),
      enabled: enabledToggle instanceof HTMLInputElement
        ? enabledToggle.checked
        : null,
      wheelEnabled: wheelToggle instanceof HTMLInputElement
        ? wheelToggle.checked
        : null,
    });
  `);
  return JSON.parse(raw) as WheelSettingsSnapshot;
}

async function waitForSettingsTogglesEnabled(): Promise<void> {
  let observed: WheelSettingsSnapshot | null = null;
  const attempts = Math.ceil(
    SETTINGS_WAIT_TIMEOUT_MS / SETTINGS_WAIT_INTERVAL_MS,
  );
  for (let attempt = 0; attempt < attempts; attempt++) {
    observed = await readWheelSettingsSnapshot();
    if (observed.enabled === true && observed.wheelEnabled === true) {
      return;
    }

    if (observed.enabled === false || observed.wheelEnabled === false) {
      await client.setContext("content");
      await client.executeScript(`
        const enabledToggle = document.querySelector(
          '[data-setting="mouse-gesture-enabled"]',
        );
        const wheelToggle = document.querySelector(
          '[data-setting="mouse-gesture-wheel-enabled"]',
        );
        if (
          enabledToggle instanceof HTMLInputElement &&
          !enabledToggle.checked
        ) {
          enabledToggle.click();
        } else if (
          wheelToggle instanceof HTMLInputElement &&
          !wheelToggle.checked
        ) {
          wheelToggle.click();
        }
      `);
    }
    await new Promise((resolve) =>
      setTimeout(resolve, SETTINGS_WAIT_INTERVAL_MS)
    );
  }
  throw new Error(
    `Mouse gesture settings toggles did not become enabled: ${
      JSON.stringify(observed)
    }`,
  );
}

async function waitForWheelSettings(
  expectedValues?: [string, string],
): Promise<WheelSettingsSnapshot> {
  let observed: WheelSettingsSnapshot | null = null;
  for (let attempt = 0; attempt < 120; attempt++) {
    observed = await readWheelSettingsSnapshot();
    const valuesMatch = !expectedValues ||
      observed.selectedValues[0] === expectedValues[0] &&
        observed.selectedValues[1] === expectedValues[1];
    if (
      observed.wheelSelectCount === 2 &&
      observed.disabled.every((disabled) => !disabled) &&
      valuesMatch
    ) {
      return observed;
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  throw new Error(
    `Mouse gesture settings did not become ready: ${JSON.stringify(observed)}`,
  );
}

function assertWheelSettingsContract(snapshot: WheelSettingsSnapshot): void {
  assert(
    snapshot.wheelSelectCount === 2,
    `Expected exactly two wheel action selects, got ${snapshot.wheelSelectCount} among ${snapshot.allSelectCount} selects`,
  );
  for (const [index, values] of snapshot.optionValues.entries()) {
    assert(
      values.length === REPEAT_SAFE_WHEEL_ACTIONS.length,
      `Wheel select ${index} has ${values.length} options, expected ${REPEAT_SAFE_WHEEL_ACTIONS.length}`,
    );
    assert(
      !values.includes(FORBIDDEN_ACTION),
      `Wheel select ${index} exposes forbidden ${FORBIDDEN_ACTION}`,
    );
  }
}

async function setWheelActionsThroughSettings(): Promise<void> {
  await client.setContext("content");
  await client.executeScript(`
    const allowed = ${JSON.stringify(REPEAT_SAFE_WHEEL_ACTIONS)};
    const sortedAllowed = [...allowed].sort();
    const wheelSelects = [...document.querySelectorAll("select")].filter(
      (select) => {
        const values = [...select.options].map((option) => option.value).sort();
        return values.length === sortedAllowed.length &&
          values.every((value, index) => value === sortedAllowed[index]);
      },
    );
    if (wheelSelects.length !== 2) {
      throw new Error(
        "Expected exactly two wheel action selects, got " +
          wheelSelects.length,
      );
    }
    const nativeValueSetter = Object.getOwnPropertyDescriptor(
      HTMLSelectElement.prototype,
      "value",
    ).set;
    const nextValues = [
      ${JSON.stringify(SAFE_SCROLL_UP_ACTION)},
      ${JSON.stringify(SAFE_SCROLL_DOWN_ACTION)},
    ];
    wheelSelects.forEach((select, index) => {
      nativeValueSetter.call(select, nextValues[index]);
      select.dispatchEvent(new Event("change", { bubbles: true }));
    });
  `);
}

async function assertTestSettingsEnvironment(): Promise<void> {
  await client.setContext("chrome");
  const startedAt = Date.now();
  type TestSettingsEnvironment = {
    startupMode: string;
    startupLoader: string;
    startupTest: string;
    startupError: string;
  };
  let environment: TestSettingsEnvironment | null = null;
  while (Date.now() - startedAt < STARTUP_WAIT_TIMEOUT_MS) {
    const raw = await client.executeScript(`
      return JSON.stringify({
        startupMode: Services.prefs.getStringPref("nora.startup.mode", ""),
        startupLoader: Services.prefs.getStringPref("nora.startup.loader", ""),
        startupTest: Services.prefs.getStringPref("nora.startup.test", ""),
        startupError: Services.prefs.getStringPref("nora.startup.error", ""),
      });
    `);
    const observed = JSON.parse(raw) as TestSettingsEnvironment;
    environment = observed;
    if (
      observed.startupMode === "test" &&
      observed.startupLoader === "loaded" &&
      observed.startupTest === "loaded" &&
      observed.startupError === ""
    ) {
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  throw new Error(
    `Refusing settings E2E before the expected test loader is ready: ${
      JSON.stringify(environment)
    }`,
  );
}

async function assertSettingsRoute(): Promise<void> {
  await client.setContext("content");
  const raw = await client.executeScript(`
    return JSON.stringify({
      protocol: location.protocol,
      host: location.host,
      pathname: location.pathname,
      hash: location.hash,
    });
  `);
  const location = JSON.parse(raw) as {
    protocol: string;
    host: string;
    pathname: string;
    hash: string;
  };
  assert(
    location.protocol === "http:" &&
      location.host === "localhost:5183" &&
      location.pathname === "/" &&
      location.hash === SETTINGS_ROUTE,
    `Unexpected settings route: ${JSON.stringify(location)}`,
  );
}

async function ensureSettingsActor(): Promise<void> {
  await client.setContext("chrome");
  const raw = await client.executeScript(`
    let browser;
    try {
      const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
      browser = browserWindow?.gBrowser?.selectedBrowser;
      if (!browser) {
        return JSON.stringify({
          actor: false,
          url: "",
          remoteType: "",
          error: "The most recent browser window has no selected browser",
        });
      }
      const windowGlobal = browser.browsingContext?.currentWindowGlobal;
      // JSWindowActors are lazy. Explicitly requesting the actor after the
      // test runner has finished guarantees that the child-side bridge is
      // created for this fresh settings document before the page polls it.
      const actor = windowGlobal?.getActor("NRSettings");
      return JSON.stringify({
        actor: Boolean(actor),
        url: browser.currentURI?.spec ?? "",
        remoteType: browser.remoteType ?? "",
      });
    } catch (error) {
      return JSON.stringify({
        actor: false,
        url: browser.currentURI?.spec ?? "",
        remoteType: browser.remoteType ?? "",
        error: String(error),
      });
    }
  `);
  const observed = JSON.parse(raw) as {
    actor: boolean;
    url: string;
    remoteType: string;
    error?: string;
  };
  assert(
    observed.actor,
    `NRSettings actor was not available for settings route: ${
      JSON.stringify(observed)
    }`,
  );
}

async function assertSettingsPersistence(): Promise<void> {
  await assertTestSettingsEnvironment();
  await client.setContext("content");
  await client.navigate(SETTINGS_URL);
  await assertSettingsRoute();
  await ensureSettingsActor();
  await waitForSettingsTogglesEnabled();
  const initial = await waitForWheelSettings();
  assertWheelSettingsContract(initial);

  await setWheelActionsThroughSettings();
  await waitForWheelActions(
    {
      scrollUp: SAFE_SCROLL_UP_ACTION,
      scrollDown: SAFE_SCROLL_DOWN_ACTION,
    },
    "settings wheel action update",
  );

  // Re-navigate to the exact route so the React tree and persistence hook are
  // recreated from stored preferences rather than retaining local state.
  await client.setContext("content");
  await client.navigate(SETTINGS_URL);
  await assertSettingsRoute();
  await ensureSettingsActor();
  await waitForSettingsTogglesEnabled();
  const reloaded = await waitForWheelSettings([
    SAFE_SCROLL_UP_ACTION,
    SAFE_SCROLL_DOWN_ACTION,
  ]);
  assertWheelSettingsContract(reloaded);
  assert(
    reloaded.selectedValues[0] === SAFE_SCROLL_UP_ACTION &&
      reloaded.selectedValues[1] === SAFE_SCROLL_DOWN_ACTION,
    `Wheel action settings did not persist after reload: ${
      JSON.stringify(reloaded.selectedValues)
    }`,
  );
}

function assertNoWheelGestureActivation(
  state: PageState,
  label: string,
): void {
  for (const type of ["click", "auxclick", "dblclick", "contextmenu"]) {
    assert(
      state.counts[type] === 0,
      `${label}: expected zero ${type} events, got ${state.counts[type]}: ${
        JSON.stringify(state.events)
      }`,
    );
  }
}

async function assertSafeWheelSequence(
  x: number,
  y: number,
  setup: BrowserSetup,
): Promise<void> {
  await resetPageState();
  const zoomBefore = await resetZoom();
  assert(
    zoomBefore === setup.baselineZoom,
    `safe wheel: failed to reset zoom to ${setup.baselineZoom}, got ${zoomBefore}`,
  );

  await runNativeSecondaryWheelCycle(x, y, async () => {
    await sendNativeWheel(4);
    assert(
      await readZoom() === setup.zoomInOne,
      `native wheel up tick 1 must zoom exactly once to ${setup.zoomInOne}`,
    );

    await sendNativeWheel(4);
    assert(
      await readZoom() === setup.zoomInTwo,
      `native wheel up tick 2 must execute independently and zoom to ${setup.zoomInTwo}`,
    );

    await sendNativeWheel(5);
    assert(
      await readZoom() === setup.zoomInOne,
      `native wheel down tick 1 must zoom out exactly once to ${setup.zoomInOne}`,
    );

    await sendNativeWheel(5);
    assert(
      await readZoom() === setup.baselineZoom,
      `native wheel down tick 2 must return zoom to ${setup.baselineZoom}`,
    );
  });

  const gestureState = await readPageState();
  assert(
    gestureState.counts.mousedown === 1,
    `safe wheel: native right mousedown did not reach content: ${
      JSON.stringify(gestureState.events)
    }`,
  );
  assert(
    gestureState.counts.wheel === 0,
    `safe wheel: consumed native wheel events leaked into content: ${
      JSON.stringify(gestureState.events)
    }`,
  );
  assertNoWheelGestureActivation(gestureState, "safe wheel");

  // A residual native wheel inside the bounded post-mouseup window must be
  // consumed without re-running either configured action.
  await sendNativeWheel(4);
  assert(
    await readZoom() === setup.baselineZoom,
    "residual native wheel after release must not execute zoom-in",
  );
  const residualState = await readPageState();
  assert(
    residualState.counts.wheel === 0,
    `residual wheel should remain suppressed before timeout: ${
      JSON.stringify(residualState.events)
    }`,
  );
  assertNoWheelGestureActivation(residualState, "residual wheel");

  // After the bounded suppression expires, an ordinary wheel with no right
  // button must pass to content but still must not execute a gesture action.
  await new Promise((resolve) =>
    setTimeout(resolve, PREVENTION_TIMEOUT_MS + 150)
  );
  await resetPageState();
  await sendNativeWheel(4);
  const ordinaryState = await readPageState();
  assert(
    ordinaryState.counts.wheel >= 1,
    `ordinary native wheel after timeout did not reach content: ${
      JSON.stringify(ordinaryState.events)
    }`,
  );
  assert(
    await readZoom() === setup.baselineZoom,
    "ordinary native wheel without RMB must not execute zoom-in",
  );

  console.log(
    `Native wheel action counts: ${
      JSON.stringify({ scrollUp: 2, scrollDown: 2, residualActions: 0 })
    }`,
  );
}

async function assertOrdinaryRightClick(x: number, y: number): Promise<void> {
  await resetPageState();
  await moveNativePointer(x, y);
  await runXdotool(["click", "3"]);
  await new Promise((resolve) => setTimeout(resolve, 150));
  const state = await readPageState();
  assert(
    state.counts.mousedown === 1 && state.counts.mouseup === 1,
    `ordinary native right down/up did not reach content: ${
      JSON.stringify(state.events)
    }`,
  );
  assert(
    state.counts.auxclick === 1 && state.counts.contextmenu === 1,
    `ordinary native right click must retain auxclick/contextmenu behavior: ${
      JSON.stringify(state.events)
    }`,
  );
  assert(
    state.counts.click === 0 && state.counts.dblclick === 0,
    `ordinary native right click produced a primary activation: ${
      JSON.stringify(state.events)
    }`,
  );
}

async function installForbiddenManualMapping(): Promise<void> {
  await client.setContext("chrome");
  await client.executeScript(`
    const pref = ${JSON.stringify(CONFIG_PREF)};
    const config = JSON.parse(Services.prefs.getStringPref(pref, "{}"));
    config.wheelActions = {
      ...config.wheelActions,
      scrollUp: ${JSON.stringify(FORBIDDEN_ACTION)},
      scrollDown: ${JSON.stringify(SAFE_SCROLL_DOWN_ACTION)},
    };
    Services.prefs.setStringPref(pref, JSON.stringify(config));
  `);
}

async function assertForbiddenMappingFailsSafe(
  x: number,
  y: number,
  setup: BrowserSetup,
): Promise<void> {
  await resetZoom();
  await installForbiddenManualMapping();
  await waitForWheelActions(
    {
      scrollUp: NORMALIZED_SCROLL_UP_ACTION,
      scrollDown: SAFE_SCROLL_DOWN_ACTION,
    },
    "forbidden manual mapping normalization",
  );

  const handlesBefore = await getContentWindowHandles();
  assert(
    testHandle !== null && handlesBefore.includes(testHandle),
    `forbidden mapping precondition: disposable test tab is missing: ${
      JSON.stringify({ originalHandle, testHandle, handlesBefore })
    }`,
  );

  // If close-tab escapes normalization or the controller's execution-boundary
  // policy, the trusted native wheel closes testHandle and this fails.
  await runNativeSecondaryWheelCycle(x, y, async () => {
    await sendNativeWheel(4);
  });

  const handlesAfter = await getContentWindowHandles();
  assert(
    handlesAfter.length === handlesBefore.length &&
      testHandle !== null &&
      handlesAfter.includes(testHandle),
    `forbidden ${FORBIDDEN_ACTION} mapping executed destructively: before=${
      JSON.stringify(handlesBefore)
    }, after=${JSON.stringify(handlesAfter)}`,
  );

  // The normalized previous-tab action may select the original tab.
  await switchToContentWindow(testHandle);
  assert(
    await readZoom() === setup.baselineZoom,
    "forbidden mapping must not execute the requested close-tab or zoom",
  );
}

async function restoreOriginalState(
  state: OriginalState,
  restoreTestTabZoom: boolean,
): Promise<void> {
  await client.setContext("chrome");
  await client.executeScript(`
    const enabledPref = ${JSON.stringify(ENABLED_PREF)};
    const configPref = ${JSON.stringify(CONFIG_PREF)};
    const state = ${JSON.stringify(state)};
    const restoreTestTabZoom = ${JSON.stringify(restoreTestTabZoom)};

    if (state.hadConfigUserValue) {
      Services.prefs.setStringPref(configPref, state.originalConfig);
    } else if (Services.prefs.prefHasUserValue(configPref)) {
      Services.prefs.clearUserPref(configPref);
    }
    if (state.hadEnabledUserValue) {
      Services.prefs.setBoolPref(enabledPref, state.originalEnabled);
    } else if (Services.prefs.prefHasUserValue(enabledPref)) {
      Services.prefs.clearUserPref(enabledPref);
    }
    if (restoreTestTabZoom) {
      ZoomManager.zoom = state.originalZoom;
    }

    if (
      Services.prefs.prefHasUserValue(configPref) !==
        state.hadConfigUserValue ||
      Services.prefs.getStringPref(configPref, "") !== state.originalConfig ||
      Services.prefs.prefHasUserValue(enabledPref) !==
        state.hadEnabledUserValue ||
      Services.prefs.getBoolPref(enabledPref, false) !==
        state.originalEnabled ||
      (restoreTestTabZoom && ZoomManager.zoom !== state.originalZoom)
    ) {
      throw new Error("Mouse gesture wheel X11 E2E state restoration mismatch");
    }
  `);
}

let failure: unknown = null;

try {
  originalHandle = await getContentWindowHandle();
  testHandle = await newContentTab();
  await switchToContentWindow(testHandle);

  await client.setContext("chrome");
  originalState = JSON.parse(
    await client.executeScript(`
      const enabledPref = ${JSON.stringify(ENABLED_PREF)};
      const configPref = ${JSON.stringify(CONFIG_PREF)};
      const originalConfig = Services.prefs.getStringPref(configPref, "");
      if (!originalConfig) {
        throw new Error("Mouse gesture config pref is empty");
      }
      return JSON.stringify({
        originalZoom: ZoomManager.zoom,
        hadEnabledUserValue: Services.prefs.prefHasUserValue(enabledPref),
        originalEnabled: Services.prefs.getBoolPref(enabledPref, false),
        hadConfigUserValue: Services.prefs.prefHasUserValue(configPref),
        originalConfig,
      });
    `),
  ) as OriginalState;

  const setup = JSON.parse(
    await client.executeScript(`
      const enabledPref = ${JSON.stringify(ENABLED_PREF)};
      const configPref = ${JSON.stringify(CONFIG_PREF)};
      const config = JSON.parse(
        Services.prefs.getStringPref(configPref, "{}"),
      );
      config.enabled = true;
      config.rockerGesturesEnabled = false;
      config.wheelGesturesEnabled = true;
      config.showTrail = false;
      config.showLabel = false;
      config.contextMenu = {
        ...(config.contextMenu ?? {}),
        preventionTimeout: ${PREVENTION_TIMEOUT_MS},
      };
      Services.prefs.setStringPref(configPref, JSON.stringify(config));
      Services.prefs.setBoolPref(enabledPref, true);

      FullZoom.reset();
      const baselineZoom = ZoomManager.zoom;
      FullZoom.enlarge();
      const zoomInOne = ZoomManager.zoom;
      FullZoom.enlarge();
      const zoomInTwo = ZoomManager.zoom;
      FullZoom.reduce();
      const zoomAfterOneReduction = ZoomManager.zoom;
      FullZoom.reduce();
      const zoomAfterTwoReductions = ZoomManager.zoom;
      FullZoom.reset();
      return JSON.stringify({
        processId: Services.appinfo.processID,
        baselineZoom,
        zoomInOne,
        zoomInTwo,
        zoomAfterOneReduction,
        zoomAfterTwoReductions,
      });
    `),
  ) as BrowserSetup & {
    zoomAfterOneReduction: number;
    zoomAfterTwoReductions: number;
  };
  assert(
    Number.isInteger(setup.processId) && setup.processId > 0,
    `Invalid Floorp process ID: ${setup.processId}`,
  );
  assert(
    setup.zoomInOne !== setup.baselineZoom &&
      setup.zoomInTwo !== setup.zoomInOne &&
      setup.zoomAfterOneReduction === setup.zoomInOne &&
      setup.zoomAfterTwoReductions === setup.baselineZoom,
    `Zoom action precondition failed: ${JSON.stringify(setup)}`,
  );

  await assertSettingsPersistence();

  await client.setContext("content");
  const fixture = `<!doctype html>
    <meta charset="utf-8">
    <title>${WINDOW_TITLE_MARKER}</title>
    <style>
      body { margin: 30px; }
      #target { width: 480px; height: 360px; font-size: 24px; }
    </style>
    <button id="target">Wheel gesture target</button>
    <script>
      window.__floorpWheelX11E2E = { counts: {}, events: [] };
      for (const type of ["mousedown", "mouseup", "click", "auxclick", "dblclick", "contextmenu", "wheel"]) {
        document.addEventListener(type, (event) => {
          window.__floorpWheelX11E2E.counts[type] =
            (window.__floorpWheelX11E2E.counts[type] ?? 0) + 1;
          window.__floorpWheelX11E2E.events.push({
            type: event.type,
            button: event.button ?? 0,
            buttons: event.buttons ?? 0,
            deltaY: typeof event.deltaY === "number" ? event.deltaY : null,
            defaultPrevented: event.defaultPrevented,
          });
          if (type === "contextmenu") {
            event.preventDefault();
          }
        }, true);
      }
    </script>`;
  await client.navigate(
    `data:text/html;charset=utf-8,${encodeURIComponent(fixture)}`,
  );
  const point = await client.executeScript(`
    const rect = document.querySelector("#target").getBoundingClientRect();
    const dpr = window.devicePixelRatio;
    return {
      x: Math.round((window.mozInnerScreenX + rect.x + rect.width / 2) * dpr),
      y: Math.round((window.mozInnerScreenY + rect.y + rect.height / 2) * dpr),
      devicePixelRatio: dpr,
      targetRect: {
        x: rect.x,
        y: rect.y,
        width: rect.width,
        height: rect.height,
      },
      contentScreenOrigin: {
        x: window.mozInnerScreenX,
        y: window.mozInnerScreenY,
      },
    };
  `) as TargetPoint;
  assert(
    Number.isFinite(point.x) &&
      Number.isFinite(point.y) &&
      point.devicePixelRatio > 0,
    `Invalid Marionette content screen point: ${JSON.stringify(point)}`,
  );

  const floorpWindow = await findSoleVisibleFloorpWindow(setup.processId);
  assert(
    point.x >= floorpWindow.x &&
      point.x < floorpWindow.x + floorpWindow.width &&
      point.y >= floorpWindow.y &&
      point.y < floorpWindow.y + floorpWindow.height,
    `Marionette content point (${point.x}, ${point.y}) is outside xdotool Floorp geometry ${
      JSON.stringify(floorpWindow)
    }`,
  );

  console.log(
    `Mouse gesture wheel X11 target: ${
      JSON.stringify({ floorpWindow, point })
    }`,
  );

  await assertSafeWheelSequence(point.x, point.y, setup);
  await assertOrdinaryRightClick(point.x, point.y);
  await assertForbiddenMappingFailsSafe(point.x, point.y, setup);

  console.log("Mouse gesture wheel X11 E2E passed");
} catch (error) {
  failure = error;
}

const cleanupErrors: unknown[] = [];
if (secondaryButtonHeld) {
  try {
    await releaseNativeSecondary();
  } catch (error) {
    cleanupErrors.push(error);
  }
}

let handles: string[] = [];
try {
  handles = await getContentWindowHandles();
} catch (error) {
  cleanupErrors.push(error);
}

const testHandleExists = testHandle !== null && handles.includes(testHandle);
const restorationHandle = testHandleExists
  ? testHandle
  : originalHandle && handles.includes(originalHandle)
  ? originalHandle
  : null;

if (restorationHandle) {
  try {
    await switchToContentWindow(restorationHandle);
    if (originalState) {
      await restoreOriginalState(originalState, testHandleExists);
    }
  } catch (error) {
    cleanupErrors.push(error);
  }
} else if (originalState) {
  cleanupErrors.push(
    new Error("No surviving browser tab is available to restore preferences"),
  );
}

if (testHandleExists && testHandle) {
  try {
    await switchToContentWindow(testHandle);
    await closeContentWindow();
  } catch (error) {
    cleanupErrors.push(error);
  }
}

if (originalHandle && handles.includes(originalHandle)) {
  try {
    await switchToContentWindow(originalHandle);
  } catch (error) {
    cleanupErrors.push(error);
  }
}

await client.close();

if (failure !== null) {
  if (cleanupErrors.length > 0) {
    console.error("Mouse gesture wheel X11 E2E cleanup errors:", cleanupErrors);
  }
  throw failure;
}
if (cleanupErrors.length > 0) {
  throw new AggregateError(
    cleanupErrors,
    "Mouse gesture wheel X11 E2E cleanup failed",
  );
}
