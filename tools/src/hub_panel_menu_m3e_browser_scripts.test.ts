// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";
import { HUB_PANEL_MENU_M3E_INITIAL_WINDOW_SCRIPT } from "./hub_panel_menu_m3e_lifecycle_script.ts";
import {
  buildHubPanelMenuM3eBrowserScript,
  buildHubPanelMenuM3eBrowserScriptSuite,
} from "./hub_panel_menu_m3e_browser_scripts.ts";

Deno.test("generated preparation preserves the Marionette sandbox owner instead of enumeration order", () => {
  const first = { closed: false, private: false, browsingContext: {} };
  const current = { closed: false, private: false, browsingContext: {} };
  const choose = (owner: unknown, existing: unknown[]) =>
    new Function(
      "window",
      "PrivateBrowsingUtils",
      HUB_PANEL_MENU_M3E_INITIAL_WINDOW_SCRIPT + ";return initialWindow;",
    )(owner, { isWindowPrivate: (win: { private: boolean }) => win.private })(
      existing,
    );
  assertEquals(choose(current, [first, current]), current);
  assertEquals(choose(current, []), null);
  assertEquals(choose(current, [first]), null);
  assertEquals(
    choose({ ...current, private: true }, [{ ...current, private: true }]),
    null,
  );
  assertEquals(choose({ ...current, closed: true }, [current]), null);
});

Deno.test("restored window preparation retains the owner and one peer while isolating surplus owned windows", () => {
  const first = { closed: false, private: false, browsingContext: {} };
  const owner = { closed: false, private: false, browsingContext: {} };
  const extra = { closed: false, private: false, browsingContext: {} };
  const plan = new Function(
    "window",
    "PrivateBrowsingUtils",
    HUB_PANEL_MENU_M3E_INITIAL_WINDOW_SCRIPT + ";return preparationWindows;",
  )(owner, { isWindowPrivate: (win: { private: boolean }) => win.private });
  assertEquals(plan([]), null);
  assertEquals(plan([first]), null);
  assertEquals(plan([owner]), { initial: owner, second: null, surplus: [] });
  assertEquals(plan([first, owner]), {
    initial: owner,
    second: first,
    surplus: [],
  });
  assertEquals(plan([first, extra, owner]), {
    initial: owner,
    second: first,
    surplus: [extra],
  });
});

Deno.test("generated restart trigger interaction verifies live and quarantined menus and retains a showing failure", async () => {
  for (
    const [count, stuck, deferred] of [[1, false, false], [0, false, false], [
      1,
      true,
      false,
    ], [1, false, true]] as const
  ) {
    class Popup extends EventTarget {
      state = "closed";
      hidePopup() {
        this.state = "closed";
        this.dispatchEvent(new Event("popuphidden"));
      }
    }
    const services = {
      focus: { activeWindow: null as unknown },
      wm: {
        getEnumerator: () => {
          let index = 0;
          return {
            hasMoreElements: () => index < wins.length,
            getNext: () => wins[index++],
          };
        },
      },
    };
    const wins = [0, 1].map((id) => {
      let initialized = !deferred;
      const popup = new Popup();
      const settings = {};
      const node = {
        disabled: false,
        isConnected: true,
        previousElementSibling: settings,
        getAttribute: () => "Floorp Hub",
      };
      const trigger = {
        click() {
          if (deferred) setTimeout(() => initialized = true, 1);
          popup.state = stuck ? "showing" : "open";
          if (!stuck) popup.dispatchEvent(new Event("popupshown"));
        },
      };
      const win = {
        closed: false,
        browsingContext: { id },
        focus() {
          services.focus.activeWindow = win;
        },
        document: {
          activeElement: { id: "" },
          getElementById: (key: string) =>
            key === "appMenu-popup"
              ? popup
              : key === "PanelUI-menu-button"
              ? trigger
              : settings,
          querySelectorAll: () => count && initialized ? [node] : [],
        },
      };
      return win;
    });
    const taskId = "00000000-0000-4000-8000-000000000000";
    const sandbox = {
      __hubPanelMenuM3eStateByTask: {
        [`${taskId}:C`]: {
          windows: { initial: wins[0], second: wins[1] },
          hosts: {},
          closedHosts: {},
          nodeIds: new WeakMap(),
          nextNodeId: 0,
        },
      },
    };
    const imports = {
      getSharedAppMenuRegistry: () => ({}),
      PrivateBrowsingUtils: { isWindowPrivate: () => false },
      NavigableManager: {
        getIdForBrowsingContext: (context: { id: number }) =>
          String(context.id),
      },
      setTimeout: (fn: () => void) => setTimeout(fn, 0),
      clearTimeout,
    };
    const script = buildHubPanelMenuM3eBrowserScript({
      kind: "lifecycle",
      payload: {
        testId: taskId,
        action: "interactive-menu",
        expectedCount: count,
        expectedLabel: "Floorp Hub",
      },
    });
    const result = await new Promise<Record<string, unknown>>((resolve) => {
      new Function("ChromeUtils", "Services", "globalThis", "window", script)(
        { importESModule: () => imports },
        services,
        sandbox,
        wins[0],
        resolve,
      );
    });
    assertEquals(result.passed, !stuck);
    if (stuck) {
      assertEquals(
        (result.observations as Record<string, unknown>[])[0].state,
        "showing",
      );
    }
  }
});

Deno.test("every generated M3e browser script compiles before native execution", () => {
  const scripts = [
    ...buildHubPanelMenuM3eBrowserScriptSuite(),
    buildHubPanelMenuM3eBrowserScript({
      kind: "startup-observation",
      payload: { featureEnabled: true, manualHostEnabled: false },
    }),
    buildHubPanelMenuM3eBrowserScript({
      kind: "host-operation",
      payload: { expectedRevision: 2, txId: "syntax-only" },
    }),
  ];
  for (const script of scripts) {
    new Function(script);
  }
  assertEquals(scripts.length, 19);
});

Deno.test("generated script errors retain finite IDs without exception text", async () => {
  const script = buildHubPanelMenuM3eBrowserScript({
    kind: "startup-observation",
  });
  const result = await new Promise((resolve) => {
    new Function("ChromeUtils", script)({
      importESModule() {
        throw new Error("private-source-marker");
      },
    }, resolve);
  });
  assertEquals(result, {
    scriptError: { stage: "startup-observation", check: "imports" },
  });
});

Deno.test("task state persists across generated script calls", async () => {
  const sandbox: Record<string, unknown> = {};
  const script = buildHubPanelMenuM3eBrowserScript({
    kind: "startup-observation",
    payload: {
      testId: "00000000-0000-4000-8000-000000000000",
      rowId: "A",
    },
  });
  async function execute(): Promise<unknown> {
    return await new Promise((resolve) => {
      new Function("ChromeUtils", "Services", "globalThis", script)(
        { importESModule: () => ({}) },
        {
          wm: {
            getEnumerator() {
              throw new Error("stop after state initialization");
            },
          },
        },
        sandbox,
        resolve,
      );
    });
  }
  const expectedError = {
    scriptError: { stage: "startup-observation", check: "startup-readiness" },
  };
  assertEquals(await execute(), expectedError);
  const table = sandbox.__hubPanelMenuM3eStateByTask as Record<string, unknown>;
  const key = "00000000-0000-4000-8000-000000000000:A";
  const state = table[key];
  assertEquals(typeof state, "object");
  assertEquals(await execute(), expectedError);
  assertEquals(table[key] === state, true);
  const clear = buildHubPanelMenuM3eBrowserScript({ kind: "clear-task-state" });
  const cleared = await new Promise((resolve) => {
    new Function("globalThis", clear)(sandbox, resolve);
  });
  assertEquals(cleared, { cleared: true });
  assertEquals(Object.hasOwn(sandbox, "__hubPanelMenuM3eStateByTask"), false);
});

Deno.test("task state access failures retain only their finite check", async () => {
  const script = buildHubPanelMenuM3eBrowserScript({
    kind: "startup-observation",
    payload: {
      testId: "00000000-0000-4000-8000-000000000000",
      rowId: "A",
    },
  });
  const result = await new Promise((resolve) => {
    new Function("ChromeUtils", "globalThis", script)(
      { importESModule: () => ({}) },
      new Proxy({}, {
        get() {
          throw new Error("private-source-marker");
        },
      }),
      resolve,
    );
  });
  assertEquals(result, {
    scriptError: { stage: "startup-observation", check: "task-state-table" },
  });
});

Deno.test("generated close retains the closed facade and barriers and restores the real registry observer", async () => {
  const testId = "00000000-0000-4000-8000-000000000000";
  let restored = 0;
  const host = {
    close: () => Promise.resolve(),
    cleanupDiagnosticsForTests: () => ({ proven: true, evidence: [] }),
  };
  const task = {
    hosts: { "hub-panel-menu": host },
    closedHosts: {} as Record<string, unknown>,
    barriers: { retained: true },
    registryObserver: {
      restore: () => {
        restored++;
      },
    },
  };
  const sandbox = { __hubPanelMenuM3eStateByTask: { [testId + ":C"]: task } };
  const script = buildHubPanelMenuM3eBrowserScript({
    kind: "close-host",
    payload: { testId, rowId: "C" },
  });
  const result = await new Promise((resolve) => {
    new Function("ChromeUtils", "globalThis", script)(
      { importESModule: () => ({}) },
      sandbox,
      resolve,
    );
  });
  assertEquals(result, { proven: true, evidence: [] });
  assertEquals(restored, 1);
  assertEquals(task.closedHosts["hub-panel-menu"] === host, true);
  assertEquals(Object.hasOwn(task.hosts, "hub-panel-menu"), false);
  assertEquals(task.barriers.retained, true);
});
