// SPDX-License-Identifier: MPL-2.0

import {
  assert,
  assertEquals,
  assertRejects,
  assertStrictEquals,
  assertThrows,
} from "@std/assert";
import {
  assertHubPanelMenuM3eFocusResult,
  assertHubPanelMenuM3eGatedCancellation,
  HubPanelMenuM3eFocusFailure,
  withHubPanelMenuM3eFocusCleanup,
} from "./hub_panel_menu_m3e_focus.ts";
import { HUB_PANEL_MENU_M3E_FOCUS_SCRIPT } from "./hub_panel_menu_m3e_focus_script.ts";

Deno.test("focus oracles distinguish positive restoration, real timely cancellation, and expiry", () => {
  const positive = {
    oldConnected: false,
    preparedAt: 1000,
    at: 1300,
    foreground: true,
    popupOpen: true,
    activeButton: true,
    buttonCount: 1,
    oldNodeId: 1,
    nodeId: 2,
  };
  assertHubPanelMenuM3eFocusResult("focus-restoration", positive);
  for (
    const mutation of [
      { foreground: false },
      { popupOpen: false },
      { activeButton: false },
      { nodeId: 1 },
      { at: 4001 },
    ]
  ) {
    assertThrows(() =>
      assertHubPanelMenuM3eFocusResult("focus-restoration", {
        ...positive,
        ...mutation,
      })
    );
  }
  for (
    const [caseId, type] of Object.entries({
      "focus-pointer-cancellation": "pointermove",
      "focus-keyboard-cancellation": "keydown",
      "focus-movement-cancellation": "focusin",
      "focus-blur-cancellation": "blur",
      "focus-menu-close-cancellation": "popuphidden",
      "focus-navigation-cancellation": "focusin",
      "focus-window-close-intent-cancellation": "unload",
    })
  ) {
    const cancellation = {
      ...positive,
      closed: caseId === "focus-window-close-intent-cancellation",
      buttonCount: caseId === "focus-window-close-intent-cancellation" ? 0 : 1,
      activeButton: false,
      actionAt: 1100,
      claimLiveAtAction: true,
      claimInvalidatedAt: 1101,
      gatedCancellation: {
        actionAt: 1100,
        gateCheckedAt: 1150,
        claimLiveAtAction: true,
        claimInvalidatedAt: 1101,
        buttonCount: 0,
        activeButton: false,
        events: [{ type, trusted: true, at: 1101 }],
      },
      events: [{ type, trusted: true, at: 1101, newButton: false }],
    };
    assertHubPanelMenuM3eFocusResult(caseId, cancellation);
    for (
      const mutation of [
        { claimLiveAtAction: false },
        { claimInvalidatedAt: null },
        { closed: true, buttonCount: 0, claimInvalidatedAt: null },
        { activeButton: true },
        { at: 4500 },
        { events: [] },
        {
          events: [{ type, trusted: false, at: 1101 }],
        },
        { events: [{ type, trusted: true, at: 1101, newButton: true }] },
      ]
    ) {
      assertThrows(() =>
        assertHubPanelMenuM3eFocusResult(caseId, {
          ...cancellation,
          ...mutation,
        })
      );
    }
  }
  const expired = {
    ...positive,
    at: 4200,
    activeButton: false,
    activeSettings: true,
    fallbackVisible: true,
  };
  assertHubPanelMenuM3eFocusResult("focus-bounded-expiry", expired);
  assertThrows(() =>
    assertHubPanelMenuM3eFocusResult("focus-bounded-expiry", {
      ...expired,
      at: 1300,
    })
  );
  assertThrows(() =>
    assertHubPanelMenuM3eFocusResult("focus-bounded-expiry", {
      ...expired,
      foreground: false,
    })
  );
  for (const malformed of [null, {}, { ...positive, at: null }]) {
    assertThrows(() =>
      assertHubPanelMenuM3eFocusResult("focus-restoration", malformed)
    );
  }
});

Deno.test("cancellation must be proven with its named trusted event before replacement render is released", () => {
  const proof = {
    actionAt: 1000,
    gateCheckedAt: 1100,
    claimLiveAtAction: true,
    claimInvalidatedAt: 1010,
    buttonCount: 0,
    activeButton: false,
    events: [{ type: "pointermove", trusted: true, at: 1010 }],
  };
  assertHubPanelMenuM3eGatedCancellation("focus-pointer-cancellation", proof);
  for (
    const invalid of [
      null,
      {},
      { ...proof, claimInvalidatedAt: null },
      { ...proof, claimInvalidatedAt: 1200 },
      { ...proof, buttonCount: 1 },
      { ...proof, events: [] },
      { ...proof, events: [{ type: "keydown", trusted: true, at: 1010 }] },
      { ...proof, events: [{ type: "pointermove", trusted: false, at: 1010 }] },
      { ...proof, events: [{ type: "pointermove", trusted: true, at: 1200 }] },
    ]
  ) {
    assertThrows(() =>
      assertHubPanelMenuM3eGatedCancellation(
        "focus-pointer-cancellation",
        invalid,
      )
    );
  }
});

Deno.test("generated passive focus probes preserve exact native calls and restore descriptors", async () => {
  const returned = {};
  let active: unknown;
  let calledThis: unknown;
  let calledArgs: unknown;
  let focusArgs: unknown;
  function element(id: string) {
    return {
      id,
      isConnected: true,
      disabled: false,
      getClientRects: () => [{}],
      getAttribute: () => null,
      focus(...args: unknown[]) {
        active = this;
        focusArgs = args;
        return returned;
      },
    };
  }
  const old = element("appMenu-floorp-hub-button");
  active = old;
  const settings = element("appMenu-settings-button");
  const trigger = element("PanelUI-menu-button");
  const nodes: Record<string, ReturnType<typeof element>> = {
    [old.id]: old,
    [settings.id]: settings,
    [trigger.id]: trigger,
  };
  const popup = { state: "open" };
  const listeners = new Set<unknown>();
  const document = {
    get activeElement() {
      return active;
    },
    hasFocus: () => true,
    getElementById: (id: string) => id === "appMenu-popup" ? popup : nodes[id],
    querySelectorAll: () => [old],
    addEventListener: (_type: string, callback: unknown) =>
      listeners.add(callback),
    removeEventListener: (_type: string, callback: unknown) =>
      listeners.delete(callback),
  };
  const win = {
    document,
    closed: false,
    focus() {},
    getComputedStyle: () => ({ display: "block", visibility: "visible" }),
    addEventListener: document.addEventListener,
    removeEventListener: document.removeEventListener,
  };
  const task = {
    windows: { initial: win },
    registryOwners: [{ generation: 1 }],
  };
  class Registry {
    revoke(...args: unknown[]) {
      calledThis = this;
      calledArgs = args;
      return returned;
    }
  }
  const registry = new Registry();
  const originalRevoke = registry.revoke;
  const originalFocus = settings.focus;
  const run = new Function(
    "input",
    "getTaskState",
    "Services",
    "ChromeUtils",
    "nodeId",
    "waitFor",
    `const REGISTRY_URI = "controlled-registry"; return (async () => { ${HUB_PANEL_MENU_M3E_FOCUS_SCRIPT} })();`,
  );
  const execute = (action: string) =>
    run(
      { kind: "focus", action },
      () => task,
      {
        focus: {
          activeWindow: win,
          elementIsFocusable: (node: unknown, flags: unknown) => {
            assert(node === settings || node === trigger);
            assertStrictEquals(flags, 0);
            return true;
          },
        },
      },
      { importESModule: () => ({ getSharedAppMenuRegistry: () => registry }) },
      (_state: unknown, node: unknown) => node ? 1 : null,
      (predicate: () => boolean) => Promise.resolve(assert(predicate())),
    );
  await execute("prepare");
  const lease = {};
  assertStrictEquals(registry.revoke(lease, "replacement"), returned);
  assertStrictEquals(calledThis, registry);
  assertEquals(calledArgs, [lease, "replacement"]);
  assertStrictEquals(settings.focus({ preventScroll: true }), returned);
  assertEquals(focusArgs, [{ preventScroll: true }]);
  popup.state = "closed";
  await execute("dispose");
  assertStrictEquals(registry.revoke, originalRevoke);
  assertStrictEquals(settings.focus, originalFocus);
  assert(!Object.hasOwn(registry, "revoke"));
  assert(!Object.hasOwn(task, "focusProbe"));
  assertEquals(listeners.size, 0);
});

Deno.test("generated pointer preparation validates a live fallback target without manufacturing an input event", async () => {
  let width = 10;
  let present = true;
  const settings = {
    id: "appMenu-settings-button",
    isConnected: true,
    disabled: false,
    getClientRects: () => [{}],
    getAttribute: () => null,
    getBoundingClientRect: () => ({ x: 20, y: 40, width, height: 20 }),
  };
  const trigger = { ...settings, id: "PanelUI-menu-button" };
  const movement = {
    ...settings,
    id: "appMenu-new-tab-button2",
    focus() {
      win.document.activeElement = movement;
    },
  };
  const audit = {
    old: { isConnected: false },
    preparedAt: Date.now(),
    fallbackAt: Date.now(),
    actionAt: null,
    claimListeners: new Map([[
      {},
      new Set(["keydown", "pointerdown", "pointermove"]),
    ]]),
    claimInvalidatedAt: null,
    events: [],
  };
  const win = {
    closed: false,
    innerWidth: 100,
    innerHeight: 100,
    document: {
      activeElement: settings,
      hasFocus: () => true,
      querySelectorAll: () => [],
      getElementById: (id: string) =>
        id === "appMenu-popup"
          ? { state: "open" }
          : id === settings.id
          ? present ? settings : null
          : id === trigger.id
          ? trigger
          : id === movement.id
          ? movement
          : null,
    },
    getComputedStyle: () => ({ display: "block", visibility: "visible" }),
  };
  const run = new Function(
    "input",
    "getTaskState",
    "Services",
    "nodeId",
    `return (async () => { ${HUB_PANEL_MENU_M3E_FOCUS_SCRIPT} })();`,
  );
  const execute = (caseId = "focus-pointer-cancellation") =>
    run(
      { kind: "focus", action: "act", caseId },
      () => ({ windows: { initial: win }, focusProbe: audit }),
      { focus: { activeWindow: win, elementIsFocusable: () => true } },
      () => null,
    );
  assertEquals((await execute()).pointerTarget, { x: 25, y: 50 });
  assertEquals(audit.events, []);
  const moved = await execute("focus-movement-cancellation");
  assertEquals(moved.after.activeId, movement.id);
  assertEquals(audit.events, []);
  win.document.activeElement = settings;
  width = 0;
  let rejected = false;
  try {
    await execute();
  } catch {
    rejected = true;
  }
  assert(rejected);
  present = false;
  assertEquals((await execute()).precondition, false);
});

Deno.test("focus disposal preserves original evidence and surfaces a standalone disposal failure", async () => {
  const primary = new HubPanelMenuM3eFocusFailure({
    prepared: "original evidence",
  });
  const disposal = new Error("dispose failed");
  const failure = await assertRejects(() =>
    withHubPanelMenuM3eFocusCleanup(
      () => Promise.reject(primary),
      () => Promise.reject(disposal),
    )
  );
  assertStrictEquals(failure, primary);
  assertEquals(primary.evidence.disposalFailed, true);
  assertStrictEquals(
    await assertRejects(() =>
      withHubPanelMenuM3eFocusCleanup(
        () => Promise.resolve(),
        () => Promise.reject(disposal),
      )
    ),
    disposal,
  );
  assertEquals(
    await withHubPanelMenuM3eFocusCleanup(
      () => Promise.resolve(42),
      () => Promise.resolve(),
    ),
    42,
  );
});
