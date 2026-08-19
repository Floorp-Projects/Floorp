// SPDX-License-Identifier: MPL-2.0

import { MarionetteClient } from "./browser_connector.ts";

const ENABLED_PREF = "floorp.mousegesture.enabled";
const CONFIG_PREF = "floorp.mousegesture.config";
const STATE_KEY = "__floorpMouseGestureRockerE2EState";
const POINTER_ID = "floorp-rocker-e2e-mouse";

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

async function resetPageState(): Promise<void> {
  await client.setContext("content");
  await client.executeScript(`
    window.__floorpRockerE2E.counts = {
      mousedown: 0,
      mouseup: 0,
      click: 0,
      auxclick: 0,
      dblclick: 0,
      contextmenu: 0,
    };
    window.__floorpRockerE2E.events = [];
  `);
}

async function readPageState(): Promise<PageState> {
  await client.setContext("content");
  const raw = await client.executeScript(`
    return JSON.stringify(window.__floorpRockerE2E);
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

function assertNoRockerActivation(state: PageState, label: string): void {
  for (const type of ["click", "auxclick", "dblclick", "contextmenu"]) {
    assert(
      state.counts[type] === 0,
      `${label}: expected zero ${type} events, got ${state.counts[type]}: ${
        JSON.stringify(state.events)
      }`,
    );
  }
}

async function assertOrdinaryClick(
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
    state.counts.click === 1,
    `${label}: expected one ordinary click, got ${state.counts.click}: ${
      JSON.stringify(state.events)
    }`,
  );
  assert(
    state.counts.auxclick === 0 &&
      state.counts.dblclick === 0 &&
      state.counts.contextmenu === 0,
    `${label}: ordinary click produced an unexpected click-like event: ${
      JSON.stringify(state.events)
    }`,
  );
}

async function assertRocker(
  x: number,
  y: number,
  setup: BrowserSetup,
  label: string,
  steps: PointerStep[],
  expectForwardedLeftRelease: boolean,
): Promise<void> {
  await resetPageState();
  const zoomBefore = await resetZoom();
  assert(
    zoomBefore === setup.baselineZoom,
    `${label}: failed to reset zoom to ${setup.baselineZoom}, got ${zoomBefore}`,
  );

  await performPointerSequence(x, y, steps);

  // Marionette context is session-wide, so content and chrome reads must not
  // race each other on the same connection.
  const state = await readPageState();
  const zoomAfter = await readZoom();
  assert(
    zoomAfter === setup.oneStepZoom,
    `${label}: rocker action must execute exactly once; expected zoom ${setup.oneStepZoom}, got ${zoomAfter}`,
  );
  assertNoRockerActivation(state, label);

  if (expectForwardedLeftRelease) {
    const leftDown = state.events.filter((event) =>
      event.type === "mousedown" && event.button === 0
    ).length;
    const leftUp = state.events.filter((event) =>
      event.type === "mouseup" && event.button === 0
    ).length;
    assert(
      leftDown === 1 && leftUp === 1,
      `${label}: leftRight must forward the matching left down/up without activating the target: ${
        JSON.stringify(state.events)
      }`,
    );
  }

  await assertOrdinaryClick(x, y, `${label} follow-up`);
}

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
      config.rockerGesturesEnabled = true;
      config.rockerActions = {
        leftRight: "gecko-zoom-in",
        rightLeft: "gecko-zoom-in",
      };
      Services.prefs.setStringPref(configPref, JSON.stringify(config));
      Services.prefs.setBoolPref(enabledPref, true);

      FullZoom.reset();
      const baselineZoom = ZoomManager.zoom;
      FullZoom.enlarge();
      const oneStepZoom = ZoomManager.zoom;
      FullZoom.reset();
      const rect = gBrowser.selectedBrowser.getBoundingClientRect();
      return JSON.stringify({
        baselineZoom,
        oneStepZoom,
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
    setup.oneStepZoom !== setup.baselineZoom,
    `Zoom action precondition failed: enlarge kept zoom at ${setup.baselineZoom}`,
  );

  await client.setContext("content");
  const fixture = `<!doctype html>
    <meta charset="utf-8">
    <style>
      body { margin: 40px; }
      #target { width: 260px; height: 120px; font-size: 24px; }
    </style>
    <button id="target">Rocker target</button>
    <script>
      window.__floorpRockerE2E = { counts: {}, events: [] };
      const target = document.getElementById("target");
      for (const type of ["mousedown", "mouseup", "click", "auxclick", "dblclick", "contextmenu"]) {
        target.addEventListener(type, (event) => {
          window.__floorpRockerE2E.counts[type] =
            (window.__floorpRockerE2E.counts[type] ?? 0) + 1;
          window.__floorpRockerE2E.events.push({
            type: event.type,
            button: event.button,
            buttons: event.buttons,
            defaultPrevented: event.defaultPrevented,
          });
          if (type === "contextmenu") {
            event.preventDefault();
          }
        });
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
      x < setup.browserRect.x + setup.browserRect.width &&
      y >= setup.browserRect.y &&
      y < setup.browserRect.y + setup.browserRect.height,
    `Target coordinate (${x}, ${y}) is outside the browser viewport ${
      JSON.stringify(setup.browserRect)
    }`,
  );

  console.log(
    `Mouse gesture rocker W3C target: ${
      JSON.stringify({
        browserRect: setup.browserRect,
        targetRect,
        x,
        y,
      })
    }`,
  );

  await assertOrdinaryClick(x, y, "ordinary click before rockers");
  await assertRocker(
    x,
    y,
    setup,
    "leftRight",
    [
      { type: "pointerDown", button: 0 },
      { type: "pointerDown", button: 2 },
      { type: "pointerUp", button: 2 },
      { type: "pointerUp", button: 0 },
    ],
    true,
  );
  await assertRocker(
    x,
    y,
    setup,
    "rightLeft",
    [
      { type: "pointerDown", button: 2 },
      { type: "pointerDown", button: 0 },
      { type: "pointerUp", button: 2 },
      { type: "pointerUp", button: 0 },
    ],
    false,
  );

  console.log("Mouse gesture rocker W3C E2E passed");
} finally {
  try {
    await client.send("WebDriver:ReleaseActions", {});
  } catch {
    // The session may not own any active actions after a successful test.
  }
  try {
    if (testHandle) {
      await client.send("WebDriver:SwitchToWindow", { handle: testHandle });
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
          delete window[stateKey];
        }
      `);
      await client.send("WebDriver:CloseWindow", {});
    }
    if (originalHandle) {
      await client.send("WebDriver:SwitchToWindow", {
        handle: originalHandle,
      });
    }
  } finally {
    await client.close();
  }
}
