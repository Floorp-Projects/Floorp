import { assertEquals, assertRejects } from "@std/assert";
import { runInNewContext } from "node:vm";

interface PanelData {
  panel?: string;
  sidePanelPath?: string;
  enabled: boolean;
}
interface NativeTab {
  selected: boolean;
  documentGlobal: NativeWindow;
}
interface NativeWindow {
  id: number;
  CustomEvent: typeof CustomEvent;
  dispatchEvent: (event: Event) => void;
  gBrowser: {
    selectedTab: NativeTab;
    tabContainer: {
      addEventListener: () => void;
      removeEventListener: () => void;
    };
  };
  SidebarController: { show: () => Promise<boolean>; hide: () => void };
}
interface PanelOptions {
  tabId?: number;
  windowId?: number;
  path?: string;
  enabled?: boolean;
}
interface PanelAPI {
  setOptions: (options: PanelOptions) => Promise<void>;
  getOptions: (options: PanelOptions) => Promise<PanelOptions>;
  open: (options: PanelOptions) => Promise<void>;
  close: (options: PanelOptions) => Promise<void>;
  setPanelBehavior: (
    options: { openPanelOnActionClick: boolean },
  ) => Promise<void>;
  getPanelBehavior: () => Promise<{ openPanelOnActionClick: boolean }>;
}

const patch = await Deno.readTextFile(
  new URL("./patches/chrome-side-panel.patch", import.meta.url),
);
const source = patch.split(
  "+++ b/browser/chrome/browser/content/browser/parent/ext-sidePanel.js\n",
)[1]?.split("\ndiff --git ")[0].split("\n").filter((line) =>
  line.startsWith("+") && !line.startsWith("+++")
).map((line) => line.slice(1)).join("\n");
if (!source) {
  throw new Error("Side panel implementation missing from artifact patch");
}

function createHarness(validMetadata = true) {
  const originalId = "a".repeat(32);
  const globals: PanelData = { enabled: true };
  const configs = new WeakMap<NativeTab, PanelData>();
  const displayed: string[] = [];
  const opened = new Set<NativeWindow>();
  const windows = new Map<number, NativeWindow>();
  const tabs = new Map<number, NativeTab>();
  const action = {
    id: "side-panel",
    defaults: globals,
    globals,
    isChromeSidePanel: false,
    chromePanelTargets: new WeakMap<NativeWindow, NativeTab | null>(),
    initialize: (options: { default_panel: string }) => {
      globals.panel = options.default_panel;
    },
    getContextData: (target: NativeTab | null) => {
      if (!target) return globals;
      if (!configs.has(target)) {
        configs.set(target, Object.create(globals) as PanelData);
      }
      return configs.get(target)!;
    },
    getTargetFromDetails: ({ tabId }: PanelOptions) => {
      if (tabId == null) return null;
      if (!tabs.has(tabId)) throw new Error("Invalid tab ID");
      return tabs.get(tabId)!;
    },
    updateOnChange: (_target: NativeTab | null) => {},
    updateButton: (window: NativeWindow) => action.updateWindow(window),
    updateWindow: (window: NativeWindow) => {
      if (opened.has(window)) {
        displayed.push(
          action.getContextData(action.chromePanelTargets.get(window) ?? null)
            .panel!,
        );
      }
    },
    isOpen: (window: NativeWindow) => opened.has(window),
    close: (window: NativeWindow) => opened.delete(window),
  };
  for (const id of [1, 2]) {
    const nativeWindow = {
      id,
      CustomEvent,
      dispatchEvent: () => {},
      gBrowser: {
        tabContainer: {
          addEventListener: () => {},
          removeEventListener: () => {},
        },
      },
      SidebarController: {
        hide: () => {
          opened.delete(windows.get(id)!);
        },
        show: async () => {
          opened.add(windows.get(id)!);
          const target = action.chromePanelTargets.get(
            windows.get(id)!,
          ) ?? null;
          displayed.push(action.getContextData(target).panel!);
          return true;
        },
      },
    } as unknown as NativeWindow;
    const nativeTab = { selected: true, documentGlobal: nativeWindow };
    nativeWindow.gBrowser.selectedTab = nativeTab;
    windows.set(id, nativeWindow);
    tabs.set(id, nativeTab);
  }
  tabs.set(3, { selected: false, documentGlobal: windows.get(1)! });
  const base = new URL("moz-extension://test-extension/");
  const extension = {
    id: `${validMetadata ? originalId : "b".repeat(32)}@chromium-extension`,
    baseURI: {
      scheme: "moz-extension",
      host: base.host,
      resolve: (path: string) => new URL(path, base).href,
    },
    manifest: {
      side_panel: { default_path: "global.html" },
      browser_specific_settings: {
        gecko: {
          floorp_chrome_extension: {
            schema_version: 1,
            original_id: originalId,
          },
        },
      },
    },
    apiManager: { getAPI: () => action },
    canAccessWindow: () => true,
  };
  const scope = {
    ExtensionAPIPersistent: class {
      extension = extension;
      on() {}
      off() {}
      emit() {}
    },
    EventManager: class {
      api() {
        return {};
      }
    },
    tabTracker: {
      getId: (tab: NativeTab) =>
        [...tabs].find(([, value]) => value === tab)?.[0],
    },
    ExtensionUtils: { ExtensionError: Error },
    Services: {
      locale: { isAppLocaleRTL: false },
      prefs: {
        getBoolPref: (_key: string, fallback: boolean) => fallback,
        setBoolPref: () => {},
      },
      io: {
        newURI: (url: string) => {
          const parsed = new URL(url);
          return {
            scheme: parsed.protocol.slice(0, -1),
            host: parsed.host,
            filePath: parsed.pathname,
          };
        },
      },
    },
    windowTracker: {
      addCloseListener: () => {},
      removeCloseListener: () => {},
      getId: (window: NativeWindow) => window.id,
      browserWindows: () => windows.values(),
      getWindow: (id: number) => {
        if (!windows.has(id)) throw new Error("Invalid window ID");
        return windows.get(id)!;
      },
    },
    Cu: {
      reportError: (error: unknown) => {
        throw error;
      },
    },
    global: {},
  };
  runInNewContext(source!, scope);
  const constructor = (scope as unknown as {
    sidePanel: new () => {
      getAPI: (context: unknown) => { sidePanel: PanelAPI };
    };
  }).sidePanel;
  const api =
    new constructor().getAPI({ canAccessWindow: () => true }).sidePanel;
  return { api, action, displayed };
}

Deno.test("sidePanel rejects an extension with mismatched Chrome metadata", async () => {
  const { api } = createHarness(false);
  await assertRejects(
    () => api.getOptions({}),
    Error,
    "converted Chrome extension",
  );
});

Deno.test("sidePanel keeps global and tab options distinct", async () => {
  const { api } = createHarness();
  await api.setOptions({ tabId: 1, path: "tab.html", enabled: false });
  assertEquals((await api.getOptions({})).path, "global.html");
  assertEquals((await api.getOptions({})).enabled, true);
  assertEquals((await api.getOptions({ tabId: 1 })).path, "tab.html");
  assertEquals((await api.getOptions({ tabId: 1 })).enabled, false);
  await assertRejects(
    () => api.open({ tabId: 1 }),
    Error,
    "No enabled side panel",
  );
});

Deno.test("sidePanel rejects external URLs before mutating enabled state", async () => {
  const { api } = createHarness();
  for (
    const path of [
      "https://example.com/",
      "moz-extension://other/panel.html",
      "javascript:alert(1)",
      "",
    ]
  ) {
    await assertRejects(
      () => api.setOptions({ path, enabled: false }),
      Error,
      "within the extension",
    );
  }
  assertEquals((await api.getOptions({})).enabled, true);
  assertEquals((await api.getOptions({})).path, "global.html");
});

Deno.test("sidePanel directs global and tab panels to the requested window", async () => {
  const { api, displayed } = createHarness();
  await api.setOptions({ tabId: 1, path: "first.html" });
  await api.setOptions({ tabId: 2, path: "second.html" });
  await api.open({ windowId: 1 });
  await api.open({ tabId: 1 });
  await api.open({ tabId: 2, windowId: 2 });
  assertEquals(displayed, [
    "moz-extension://test-extension/global.html",
    "moz-extension://test-extension/first.html",
    "moz-extension://test-extension/second.html",
  ]);
  await assertRejects(
    () => api.open({ tabId: 1, windowId: 2 }),
    Error,
    "does not belong",
  );
  await api.open({ tabId: 3 });
  assertEquals(displayed.at(-1), "moz-extension://test-extension/first.html");
  await assertRejects(() => api.open({}), Error, "At least one");
  await assertRejects(
    () => api.open({ windowId: 100 }),
    Error,
    "Invalid window ID",
  );
});

Deno.test("sidePanel behavior settings persist across API calls", async () => {
  const { api } = createHarness();
  assertEquals((await api.getPanelBehavior()).openPanelOnActionClick, false);
  await api.setPanelBehavior({ openPanelOnActionClick: true });
  assertEquals((await api.getPanelBehavior()).openPanelOnActionClick, true);
});

Deno.test("sidePanel close is safe to repeat", async () => {
  const { api } = createHarness();
  await api.close({ windowId: 1 });
  await api.open({ windowId: 1 });
  await api.close({ windowId: 1 });
  await api.close({ windowId: 1 });
});
