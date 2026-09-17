// SPDX-License-Identifier: MPL-2.0

import { MarionetteClient } from "./browser_connector.ts";

const ENABLED_PREF = "floorp.mousegesture.enabled";
const CONFIG_PREF = "floorp.mousegesture.config";
const STATE_KEY = "__floorpMouseGestureDrawnE2EState";
const POINTER_ID = "floorp-drawn-e2e-mouse";

type PointerStep =
  | { type: "pointerDown" | "pointerUp"; button: number }
  | {
    type: "pointerMove";
    duration: number;
    origin: "viewport";
    x: number;
    y: number;
  };

interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

interface PageState {
  counts: Record<string, number>;
  events: Array<{
    type: string;
    button: number;
    buttons: number;
    defaultPrevented: boolean;
  }>;
}

interface BrowserSetup {
  baselineZoom: number;
  oneStepZoom: number;
  twoStepZoom: number;
  browserRect: Rect;
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

const client = await MarionetteClient.connect();
let originalHandle: string | null = null;
let testHandle: string | null = null;

async function performPointerSequence(
  x: number,
  y: number,
  steps: PointerStep[],
): Promise<void> {
  await client.setContext("chrome");
  try {
    await client.send("WebDriver:PerformActions", {
      actions: [
        {
          type: "pointer",
          id: POINTER_ID,
          parameters: { pointerType: "mouse" },
          actions: [
            {
              type: "pointerMove",
              duration: 0,
              origin: "viewport",
              x,
              y,
            },
            ...steps,
          ],
        },
      ],
    });
  } finally {
    await client.send("WebDriver:ReleaseActions", {});
  }
  await new Promise((resolve) => setTimeout(resolve, 100));
}

function dragSteps(
  startX: number,
  startY: number,
  deltaX: number,
  deltaY: number,
): PointerStep[] {
  const steps: PointerStep[] = [{ type: "pointerDown", button: 2 }];
  for (let index = 1; index <= 12; index++) {
    steps.push({
      type: "pointerMove",
      duration: 8,
      origin: "viewport",
      x: startX + Math.round((deltaX * index) / 12),
      y: startY + Math.round((deltaY * index) / 12),
    });
  }
  steps.push({ type: "pointerUp", button: 2 });
  return steps;
}

async function resetPageState(): Promise<void> {
  await client.setContext("content");
  await client.executeScript(`
    window.__floorpDrawnE2E.counts = {
      mousedown: 0,
      mousemove: 0,
      mouseup: 0,
      click: 0,
      auxclick: 0,
      dblclick: 0,
      contextmenu: 0,
    };
    window.__floorpDrawnE2E.events = [];
  `);
}

async function readPageState(): Promise<PageState> {
  await client.setContext("content");
  const raw = await client.executeScript(`
    return JSON.stringify(window.__floorpDrawnE2E);
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

function assertNoClickLikeEvents(state: PageState, label: string): void {
  for (const type of ["click", "auxclick", "dblclick", "contextmenu"]) {
    assert(
      state.counts[type] === 0,
      `${label}: expected zero ${type} events, got ${state.counts[type]}: ${
        JSON.stringify(state.events)
      }`,
    );
  }
}

async function assertOrdinaryLeftClick(
  x: number,
  y: number,
  label: string,
): Promise<void> {
  await resetPageState();
  await performPointerSequence(x, y, [
    { type: "pointerDown", button: 0 },
    { type: "pointerUp", button: 0 },
  ]);
  const state = await readPageState();
  assert(
    state.counts.mousedown === 1 && state.counts.mouseup === 1,
    `${label}: ordinary left down/up did not reach content: ${
      JSON.stringify(state.events)
    }`,
  );
  assert(
    state.counts.click === 1 &&
      state.counts.auxclick === 0 &&
      state.counts.dblclick === 0 &&
      state.counts.contextmenu === 0,
    `${label}: ordinary left click produced unexpected events: ${
      JSON.stringify(state.events)
    }`,
  );
}

async function assertOrdinaryRightClick(
  x: number,
  y: number,
  label: string,
): Promise<void> {
  await resetPageState();
  await performPointerSequence(x, y, [
    { type: "pointerDown", button: 2 },
    { type: "pointerUp", button: 2 },
  ]);
  const state = await readPageState();
  assert(
    state.counts.mousedown === 1 && state.counts.mouseup === 1,
    `${label}: ordinary right down/up did not reach content: ${
      JSON.stringify(state.events)
    }`,
  );
  assert(
    state.counts.auxclick === 1,
    `${label}: expected one ordinary auxclick, got ${state.counts.auxclick}: ${
      JSON.stringify(state.events)
    }`,
  );
  assert(
    state.counts.click === 0 &&
      state.counts.dblclick === 0 &&
      state.counts.contextmenu <= 1,
    `${label}: ordinary right click produced an unexpected primary click: ${
      JSON.stringify(state.events)
    }`,
  );
}

async function assertRecognizedRightGesture(
  x: number,
  y: number,
  setup: BrowserSetup,
): Promise<void> {
  await resetPageState();
  const zoomBefore = await resetZoom();
  assert(
    zoomBefore === setup.baselineZoom,
    `recognized right: failed to reset zoom to ${setup.baselineZoom}, got ${zoomBefore}`,
  );

  await performPointerSequence(x, y, dragSteps(x, y, 120, 0));

  const state = await readPageState();
  const zoomAfter = await readZoom();
  assert(
    zoomAfter === setup.oneStepZoom,
    `recognized right: action must execute exactly once; expected zoom ${setup.oneStepZoom}, got ${zoomAfter} (two steps would be ${setup.twoStepZoom})`,
  );
  assert(
    state.counts.mousedown === 1 && state.counts.mousemove >= 3,
    `recognized right: pointer drag did not reach content: ${
      JSON.stringify(state.events)
    }`,
  );
  assertNoClickLikeEvents(state, "recognized right");
}

async function assertUnrecognizedDownGesture(
  x: number,
  y: number,
  setup: BrowserSetup,
): Promise<void> {
  await resetPageState();
  const zoomBefore = await resetZoom();
  assert(
    zoomBefore === setup.baselineZoom,
    `unrecognized down: failed to reset zoom to ${setup.baselineZoom}, got ${zoomBefore}`,
  );

  // Only the simple "right" pattern is configured. A straight downward trail
  // is therefore deterministically unrecognized and cannot fall back to a
  // complex $1 template.
  await performPointerSequence(x, y, dragSteps(x, y, 0, 120));

  const state = await readPageState();
  const zoomAfter = await readZoom();
  assert(
    zoomAfter === setup.baselineZoom,
    `unrecognized down: no action should run; expected zoom ${setup.baselineZoom}, got ${zoomAfter}`,
  );
  assert(
    state.counts.mousedown === 1 && state.counts.mousemove >= 3,
    `unrecognized down: pointer drag did not reach content: ${
      JSON.stringify(state.events)
    }`,
  );
  assertNoClickLikeEvents(state, "unrecognized down");
}

let failure: unknown = null;

try {
  originalHandle = extractHandle(
    await client.send("WebDriver:GetWindowHandle", {}),
  );
  testHandle = extractHandle(
    await client.send("WebDriver:NewWindow", { type: "tab" }),
  );
  await client.send("WebDriver:SwitchToWindow", { handle: testHandle });

  await client.setContext("chrome");
  const setup = JSON.parse(
    await client.executeScript(`
      const enabledPref = ${JSON.stringify(ENABLED_PREF)};
      const configPref = ${JSON.stringify(CONFIG_PREF)};
      const stateKey = ${JSON.stringify(STATE_KEY)};

      const originalConfig = Services.prefs.getStringPref(configPref, "");
      if (!originalConfig) {
        throw new Error("Mouse gesture config pref is empty");
      }
      window[stateKey] = {
        originalZoom: ZoomManager.zoom,
        hadEnabledUserValue: Services.prefs.prefHasUserValue(enabledPref),
        originalEnabled: Services.prefs.getBoolPref(enabledPref, false),
        hadConfigUserValue: Services.prefs.prefHasUserValue(configPref),
        originalConfig,
      };

      const config = JSON.parse(originalConfig);
      config.enabled = true;
      config.rockerGesturesEnabled = false;
      config.wheelGesturesEnabled = false;
      config.sensitivity = 100;
      config.showTrail = false;
      config.showLabel = false;
      config.contextMenu = {
        ...(config.contextMenu ?? {}),
        minDistance: 5,
        preventionTimeout: 200,
      };
      config.actions = [
        { pattern: ["right"], action: "gecko-zoom-in" },
      ];
      Services.prefs.setStringPref(configPref, JSON.stringify(config));
      Services.prefs.setBoolPref(enabledPref, true);

      FullZoom.reset();
      const baselineZoom = ZoomManager.zoom;
      FullZoom.enlarge();
      const oneStepZoom = ZoomManager.zoom;
      FullZoom.enlarge();
      const twoStepZoom = ZoomManager.zoom;
      FullZoom.reset();
      const rect = gBrowser.selectedBrowser.getBoundingClientRect();
      return JSON.stringify({
        baselineZoom,
        oneStepZoom,
        twoStepZoom,
        browserRect: {
          x: rect.x,
          y: rect.y,
          width: rect.width,
          height: rect.height,
        },
      });
    `),
  ) as BrowserSetup;
  assert(
    setup.oneStepZoom !== setup.baselineZoom &&
      setup.twoStepZoom !== setup.oneStepZoom,
    `Zoom action precondition failed: ${
      JSON.stringify({
        baseline: setup.baselineZoom,
        oneStep: setup.oneStepZoom,
        twoSteps: setup.twoStepZoom,
      })
    }`,
  );

  await client.setContext("content");
  const fixture = `<!doctype html>
    <meta charset="utf-8">
    <style>
      body { margin: 30px; }
      #target { width: 480px; height: 360px; font-size: 24px; }
    </style>
    <button id="target">Drawn gesture target</button>
    <script>
      window.__floorpDrawnE2E = { counts: {}, events: [] };
      for (const type of ["mousedown", "mousemove", "mouseup", "click", "auxclick", "dblclick", "contextmenu"]) {
        document.addEventListener(type, (event) => {
          window.__floorpDrawnE2E.counts[type] =
            (window.__floorpDrawnE2E.counts[type] ?? 0) + 1;
          window.__floorpDrawnE2E.events.push({
            type: event.type,
            button: event.button,
            buttons: event.buttons,
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
  const targetRect = await client.executeScript(`
    const rect = document.querySelector("#target").getBoundingClientRect();
    return { x: rect.x, y: rect.y, width: rect.width, height: rect.height };
  `) as Rect;
  const x = Math.round(
    setup.browserRect.x + targetRect.x + targetRect.width / 2,
  );
  const y = Math.round(
    setup.browserRect.y + targetRect.y + targetRect.height / 2,
  );
  assert(
    x >= setup.browserRect.x &&
      x + 120 < setup.browserRect.x + setup.browserRect.width &&
      y >= setup.browserRect.y &&
      y + 120 < setup.browserRect.y + setup.browserRect.height,
    `Gesture coordinates from (${x}, ${y}) are outside the browser viewport ${
      JSON.stringify(setup.browserRect)
    }`,
  );

  console.log(
    `Mouse gesture drawn W3C target: ${
      JSON.stringify({
        browserRect: setup.browserRect,
        targetRect,
        x,
        y,
      })
    }`,
  );

  await assertOrdinaryRightClick(x, y, "ordinary right click before gestures");
  await assertRecognizedRightGesture(x, y, setup);
  await assertUnrecognizedDownGesture(x, y, setup);
  await assertOrdinaryLeftClick(x, y, "ordinary left click after gestures");
  await assertOrdinaryRightClick(x, y, "ordinary right click after gestures");

  console.log("Mouse gesture drawn W3C E2E passed");
} catch (error) {
  failure = error;
}

const cleanupErrors: unknown[] = [];
try {
  await client.send("WebDriver:ReleaseActions", {});
} catch (error) {
  cleanupErrors.push(error);
}

let testHandleSelected = false;
if (testHandle) {
  try {
    await client.send("WebDriver:SwitchToWindow", { handle: testHandle });
    testHandleSelected = true;
  } catch (error) {
    cleanupErrors.push(error);
  }

  if (testHandleSelected) {
    try {
      await client.setContext("chrome");
      await client.executeScript(`
        const enabledPref = ${JSON.stringify(ENABLED_PREF)};
        const configPref = ${JSON.stringify(CONFIG_PREF)};
        const stateKey = ${JSON.stringify(STATE_KEY)};
        const state = window[stateKey];
        if (state) {
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
          ZoomManager.zoom = state.originalZoom;

          if (
            Services.prefs.prefHasUserValue(configPref) !==
              state.hadConfigUserValue ||
            Services.prefs.getStringPref(configPref, "") !==
              state.originalConfig ||
            Services.prefs.prefHasUserValue(enabledPref) !==
              state.hadEnabledUserValue ||
            Services.prefs.getBoolPref(enabledPref, false) !==
              state.originalEnabled ||
            ZoomManager.zoom !== state.originalZoom
          ) {
            throw new Error("Mouse gesture E2E state restoration mismatch");
          }
          delete window[stateKey];
        }
      `);
    } catch (error) {
      cleanupErrors.push(error);
    }

    // Closing the disposable tab is attempted even if state restoration fails.
    try {
      await client.send("WebDriver:CloseWindow", {});
    } catch (error) {
      cleanupErrors.push(error);
    }
  }
}

if (originalHandle) {
  try {
    await client.send("WebDriver:SwitchToWindow", {
      handle: originalHandle,
    });
  } catch (error) {
    cleanupErrors.push(error);
  }
}

await client.close();

if (failure !== null) {
  if (cleanupErrors.length > 0) {
    console.error("Mouse gesture drawn E2E cleanup errors:", cleanupErrors);
  }
  throw failure;
}
if (cleanupErrors.length > 0) {
  throw new AggregateError(
    cleanupErrors,
    "Mouse gesture drawn E2E cleanup failed",
  );
}
