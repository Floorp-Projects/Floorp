// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals } from "@std/assert";

const SOURCE_PATCH = new URL(
  "../../.github/patches/floorp-runtime/common/workspace-external-containers.patch",
  import.meta.url,
);
const DEFAULT_ID = "01234567-89ab-cdef-0123-456789abcdef";

type TargetWindow = {
  isPrivate?: boolean;
  workspacesFuncs?: {
    getCurrentWorkspaceUserContextId(): unknown;
    canOpenWorkspaceTab?(): boolean;
  };
};
type ResolveContext = (
  win?: TargetWindow | null,
  forcePrivate?: boolean,
) => number | null;

async function fixture() {
  const patch = await Deno.readTextFile(SOURCE_PATCH);
  // Execute the actual Runtime helper carried by the shipping patch. Keeping
  // a separate TypeScript copy would allow the policy tests to drift from it.
  const match = patch.match(
    /^\+export function getWorkspaceUserContextIdForExternalOpen\([\s\S]*?^\+\}/m,
  );
  assert(match, "the Runtime patch exports the tested pre-creation helper");
  const source = match[0].replace(/^\+/gm, "").replace("export ", "");
  const prefs = new Map<string, boolean | string>([
    ["privacy.userContext.enabled", true],
    [
      "floorp.workspaces.v4.store",
      JSON.stringify({
        defaultID: DEFAULT_ID,
        data: [[DEFAULT_ID, { userContextId: 7 }]],
        order: [DEFAULT_ID],
      }),
    ],
  ]);
  const identities = new Set([7, 9]);
  const privateBrowsing = {
    permanentPrivateBrowsing: false,
    isWindowPrivate: (win: TargetWindow) => Boolean(win.isPrivate),
  };
  const resolve = new Function(
    "Services",
    "lazy",
    `${source}\nreturn getWorkspaceUserContextIdForExternalOpen;`,
  )({
    prefs: {
      getBoolPref: (name: string, fallback: boolean) =>
        prefs.get(name) ?? fallback,
      getStringPref: (name: string, fallback: string) =>
        prefs.get(name) ?? fallback,
    },
  }, {
    PrivateBrowsingUtils: privateBrowsing,
    ContextualIdentityService: {
      getPublicIdentityFromId: (id: number) =>
        identities.has(id) ? { userContextId: id, public: true } : undefined,
    },
  }) as ResolveContext;
  return { resolve, prefs, identities, privateBrowsing };
}

Deno.test("external workspace Runtime patches agree for development and packaging", async () => {
  const source = await Deno.readTextFile(SOURCE_PATCH);
  const local = await Deno.readTextFile(
    new URL("../patches/workspace-external-containers.patch", import.meta.url),
  );
  assertEquals(
    local,
    source.replaceAll(
      "a/browser/components/BrowserContentHandler.sys.mjs",
      "a/moz-src/browser/components/BrowserContentHandler.sys.mjs",
    ).replaceAll(
      "b/browser/components/BrowserContentHandler.sys.mjs",
      "b/moz-src/browser/components/BrowserContentHandler.sys.mjs",
    ),
  );
  assertEquals(
    [...source.matchAll(/^\+\+\+ b\/(.+)$/gm)].map((match) => match[1]),
    [
      "browser/components/BrowserContentHandler.sys.mjs",
      "browser/modules/BrowserDOMWindow.sys.mjs",
    ],
    "only existing JavaScript modules change; no native rebuild is needed",
  );
  assertEquals(
    [...local.matchAll(/^\+\+\+ b\/(.+)$/gm)].map((match) => match[1]),
    [
      "moz-src/browser/components/BrowserContentHandler.sys.mjs",
      "browser/modules/BrowserDOMWindow.sys.mjs",
    ],
    "Firefox 159 packages BrowserContentHandler under moz-src",
  );
  for (const patch of [source, local]) {
    assert(
      patch.includes(
        '+    "moz-src:///browser/components/BrowserContentHandler.sys.mjs",',
      ),
      "the helper uses the native Firefox 159 module URI in both patch paths",
    );
    assert(!patch.includes("moz-src:///moz-src/"));
  }
});

Deno.test("Firefox's valid default-container guess stays distinct from no host guess", async () => {
  const patch = await Deno.readTextFile(SOURCE_PATCH);
  const match = patch.match(
    /^\+ {4}var openingUserContextId = guessUserContextIdEnabled\n(?:\+.*\n)*?\+ {6}: Ci\.nsIScriptSecurityManager\.DEFAULT_USER_CONTEXT_ID;/m,
  );
  assert(match, "the actual Runtime guess expression is tested");
  const resolveGuess = new Function(
    "guessUserContextIdEnabled",
    "lazy",
    "Ci",
    "aURI",
    `${match[0].replace(/^\+/gm, "")}\nreturn openingUserContextId;`,
  ) as (
    enabled: boolean,
    lazy: { URILoadingHelper: { guessUserContextId(): number | null } },
    constants: {
      nsIScriptSecurityManager: { DEFAULT_USER_CONTEXT_ID: number };
    },
    uri: null,
  ) => number | null;
  for (const guess of [0, 9, null]) {
    const lazy = { URILoadingHelper: { guessUserContextId: () => guess } };
    const constants = {
      nsIScriptSecurityManager: { DEFAULT_USER_CONTEXT_ID: 0 },
    };
    assertEquals(resolveGuess(true, lazy, constants, null), guess);
    assertEquals(
      resolveGuess(false, lazy, constants, null),
      0,
      "forced default and non-external opens never select the workspace fallback",
    );
  }
});

Deno.test("external opens before Workspace initialization use the persisted default workspace", async () => {
  const { resolve, prefs } = await fixture();
  const previous = [...prefs];
  assertEquals(resolve(), 7, "cold startup uses the default workspace");
  assertEquals(resolve({}), 7, "an initializing window uses the same fallback");
  assertEquals([...prefs], previous, "resolution must not migrate/write prefs");
});

Deno.test("the target window's selected workspace overrides the profile default", async () => {
  const { resolve } = await fixture();
  assertEquals(
    resolve({
      workspacesFuncs: { getCurrentWorkspaceUserContextId: () => 9 },
    }),
    9,
  );
  assertEquals(
    resolve({
      workspacesFuncs: { getCurrentWorkspaceUserContextId: () => 0 },
    }),
    0,
    "a selected workspace without a container must stay in context 0",
  );
});

Deno.test("external workspace fallback respects disabled workspaces, disabled containers and forced default", async () => {
  for (
    const [name, value] of [
      ["floorp.workspaces.enabled", false],
      ["privacy.userContext.enabled", false],
      ["browser.link.force_default_user_context_id_for_external_opens", true],
    ] as const
  ) {
    const { resolve, prefs } = await fixture();
    prefs.set(name, value);
    assertEquals(resolve(), 0, name);
    assertEquals(
      resolve({
        workspacesFuncs: { getCurrentWorkspaceUserContextId: () => 9 },
      }),
      0,
      name,
    );
  }
});

Deno.test("external workspace fallback never introduces a container into private browsing", async () => {
  const { resolve, privateBrowsing } = await fixture();
  assertEquals(resolve(null, true), 0, "a command-line private window");
  assertEquals(resolve({ isPrivate: true }), 0, "an existing private window");
  privateBrowsing.permanentPrivateBrowsing = true;
  assertEquals(resolve(), 0, "permanent and temporary autostart private mode");
});

Deno.test("external workspace fallback rejects missing, deleted and malformed identities", async () => {
  const { resolve, identities, prefs } = await fixture();
  identities.delete(7);
  assertEquals(resolve(), 0, "a deleted public identity");
  identities.add(7);
  for (
    const userContextId of [undefined, null, -1, 1.5, "7", 0x100000000, 99]
  ) {
    prefs.set(
      "floorp.workspaces.v4.store",
      JSON.stringify({
        defaultID: DEFAULT_ID,
        data: [[DEFAULT_ID, { userContextId }]],
      }),
    );
    assertEquals(resolve(), 0, `invalid default identity ${userContextId}`);
    assertEquals(
      resolve({
        workspacesFuncs: {
          getCurrentWorkspaceUserContextId: () => userContextId,
        },
      }),
      0,
      `invalid live identity ${userContextId}`,
    );
  }
});

Deno.test("external workspace fallback tolerates unavailable or malformed persisted state", async () => {
  const { resolve, prefs } = await fixture();
  for (
    const serialized of [
      "",
      "not JSON",
      "null",
      "[]",
      "{}",
      JSON.stringify({
        defaultID: "invalid",
        data: [["invalid", { userContextId: 7 }]],
      }),
      JSON.stringify({ defaultID: DEFAULT_ID, data: {} }),
      JSON.stringify({ defaultID: DEFAULT_ID, data: [null] }),
      JSON.stringify({ defaultID: DEFAULT_ID, data: [[DEFAULT_ID]] }),
      JSON.stringify({ defaultID: DEFAULT_ID, data: [] }),
    ]
  ) {
    prefs.set("floorp.workspaces.v4.store", serialized);
    assertEquals(resolve(), 0, serialized);
  }
  assertEquals(
    resolve({
      workspacesFuncs: {
        getCurrentWorkspaceUserContextId() {
          throw new Error("window unloading");
        },
      },
    }),
    0,
    "a failing getter cannot prevent an external URL from opening",
  );
});

Deno.test("container-capable cold opens and pending live defaults defer without choosing jar 0", async () => {
  const { resolve, prefs } = await fixture();
  prefs.set("floorp.workspaces.containerSync.enabled", true);
  assertEquals(
    resolve(),
    null,
    "cold startup cannot trust a persisted local ID",
  );
  const target = {
    workspacesFuncs: {
      getCurrentWorkspaceUserContextId: () => 7,
      canOpenWorkspaceTab: () => false,
    },
  };
  assertEquals(resolve(target), null);
  target.workspacesFuncs.canOpenWorkspaceTab = () => true;
  assertEquals(resolve(target), 7);
  target.workspacesFuncs.getCurrentWorkspaceUserContextId = () => 0;
  assertEquals(resolve(target), 0, "resolved none is distinct from pending");
  prefs.set(
    "browser.link.force_default_user_context_id_for_external_opens",
    true,
  );
  target.workspacesFuncs.canOpenWorkspaceTab = () => false;
  assertEquals(
    resolve(target),
    0,
    "explicit forced default bypasses workspace selection",
  );
});

type QueueTab = {
  isConnected: boolean;
  closing: boolean;
  linkedBrowser: {
    currentURI: { spec: string };
    userTypedValue: string | null;
  };
  getAttribute(name: string): string;
  setAttribute(name: string, value: string): void;
  removeAttribute(name: string): void;
};
type QueueListener = (event: { target?: QueueTab }) => void;

class QueueEvents {
  listeners = new Map<string, Set<QueueListener>>();
  addEventListener(name: string, callback: QueueListener) {
    const callbacks = this.listeners.get(name) ?? new Set();
    callbacks.add(callback);
    this.listeners.set(name, callbacks);
  }
  removeEventListener(name: string, callback: QueueListener) {
    this.listeners.get(name)?.delete(callback);
  }
  fire(name: string, event: { target?: QueueTab } = {}) {
    for (const callback of [...this.listeners.get(name) ?? []]) callback(event);
  }
}

async function queueFixture() {
  const patch = await Deno.readTextFile(SOURCE_PATCH);
  const functions = [
    "getWorkspaceUserContextIdForExternalOpen",
    "deferWorkspaceExternalOpen",
  ].map((name) => {
    const match = patch.match(
      new RegExp(`^\\+export function ${name}\\([\\s\\S]*?^\\+\\}`, "m"),
    );
    assert(match, name);
    return match[0].replace(/^\+/gm, "").replace("export ", "");
  }).join("\n");
  let ready = false;
  let workspaceId = "workspace-one";
  const tabs: QueueTab[] = [];
  const tabEvents = new QueueEvents();
  const addTab = (url: string): QueueTab => {
    const attrs = new Map([["floorpWorkspaceId", workspaceId]]);
    const tab = {
      isConnected: true,
      closing: false,
      linkedBrowser: { currentURI: { spec: url }, userTypedValue: null },
      getAttribute: (name: string) => attrs.get(name) ?? "",
      setAttribute: (name: string, value: string) => attrs.set(name, value),
      removeAttribute: (name: string) => attrs.delete(name),
    };
    tabs.push(tab);
    return tab;
  };
  const initial = addTab("about:blank");
  const win = Object.assign(new QueueEvents(), {
    closed: false,
    floorpPendingWorkspaceExternalOpens: undefined as
      | Set<{ url: string; tab: QueueTab | null }>
      | undefined,
    gBrowserInit: { delayedStartupFinished: true },
    workspacesFuncs: {
      canOpenWorkspaceTab: () => ready,
      getCurrentWorkspaceUserContextId: () => 7,
      getSelectedWorkspaceID: () => workspaceId,
    },
    gBrowser: {
      tabs,
      selectedTab: initial,
      tabContainer: tabEvents,
      addTab,
      removeTab(tab: QueueTab) {
        tab.isConnected = false;
        const index = tabs.indexOf(tab);
        if (index >= 0) tabs.splice(index, 1);
        tabEvents.fire("TabClose", { target: tab });
      },
    },
  });
  const startupObservers = new Set<(subject: unknown) => void>();
  const closedObservers = new Set<(subject: unknown) => void>();
  const defer = new Function(
    "Services",
    "lazy",
    `${functions}\nreturn deferWorkspaceExternalOpen;`,
  )({
    prefs: {
      getBoolPref: (name: string, fallback: boolean) =>
        ({
          "floorp.workspaces.enabled": true,
          "floorp.workspaces.containerSync.enabled": true,
          "privacy.userContext.enabled": true,
        })[name] ?? fallback,
    },
    obs: {
      addObserver: (cb: (subject: unknown) => void, topic: string) =>
        (topic === "domwindowclosed" ? closedObservers : startupObservers).add(
          cb,
        ),
      removeObserver: (cb: (subject: unknown) => void, topic: string) =>
        (topic === "domwindowclosed" ? closedObservers : startupObservers)
          .delete(cb),
    },
    scriptSecurityManager: { getSystemPrincipal: () => ({}) },
  }, {
    PrivateBrowsingUtils: {
      permanentPrivateBrowsing: false,
      isWindowPrivate: () => false,
    },
    ContextualIdentityService: { getPublicIdentityFromId: () => ({}) },
  }) as (
    targetWindow: typeof win,
    url: string,
    open: (id: number) => unknown,
    reuse?: boolean,
    inBackground?: boolean,
  ) => unknown;
  return {
    win,
    defer,
    setReady: (value: boolean) => ready = value,
    setWorkspace: (value: string) => workspaceId = value,
    startupObservers,
    closedObservers,
  };
}

Deno.test("external URL is retained on a blank placeholder until the actual workspace is ready", async () => {
  const { win, defer, setReady } = await queueFixture();
  const calls: number[] = [];
  defer(win, "https://example.invalid/pending", (id) => {
    calls.push(id);
    return win.gBrowser.addTab("https://example.invalid/pending");
  }, true);
  assertEquals(calls, []);
  assertEquals(win.gBrowser.tabs.length, 1, "reuse the owned startup blank");
  const placeholder = win.gBrowser.tabs[0];
  assertEquals(placeholder.linkedBrowser.currentURI.spec, "about:blank");
  assertEquals(
    placeholder.getAttribute("label"),
    "https://example.invalid/pending",
  );
  assertEquals(win.floorpPendingWorkspaceExternalOpens?.size, 1);
  setReady(true);
  win.fire("floorp-workspace-container-ready");
  assertEquals(calls, [7]);
  assertEquals(win.floorpPendingWorkspaceExternalOpens?.size, 0);
  assert(!placeholder.isConnected);
  win.fire("floorp-workspace-container-ready");
  assertEquals(calls, [7], "repeated readiness never repeats the request");
});

Deno.test("repurposed external placeholder is kept and its pending request is cancelled", async () => {
  for (const kind of ["navigate", "typed", "closed"]) {
    const { win, defer, setReady } = await queueFixture();
    let opened = false;
    defer(win, "https://example.invalid/pending", () => opened = true, true);
    const placeholder = win.gBrowser.tabs[0];
    if (kind === "navigate") {
      placeholder.linkedBrowser.currentURI.spec =
        "https://example.invalid/user";
    } else if (kind === "typed") {
      placeholder.linkedBrowser.userTypedValue = "user input";
    } else win.gBrowser.removeTab(placeholder);
    setReady(true);
    win.fire("floorp-workspace-container-ready");
    assertEquals(opened, false);
    assertEquals(win.floorpPendingWorkspaceExternalOpens?.size, 0);
    assertEquals(placeholder.isConnected, kind !== "closed");
  }
});

Deno.test("deferred external request follows its placeholder workspace and respects background", async () => {
  const { win, defer, setReady, setWorkspace } = await queueFixture();
  const selected = win.gBrowser.selectedTab;
  const calls: number[] = [];
  defer(
    win,
    "https://example.invalid/background",
    (id) => calls.push(id),
    false,
    true,
  );
  assertEquals(win.gBrowser.selectedTab, selected);
  setReady(true);
  setWorkspace("workspace-two");
  win.fire("floorp-workspace-container-ready");
  assertEquals(calls, []);
  setWorkspace("workspace-one");
  win.fire("floorp-workspace-container-ready");
  assertEquals(calls, [7]);
});

Deno.test("cold external queue waits for delayed startup and disposes observers on window close", async () => {
  const { win, defer, startupObservers, closedObservers } =
    await queueFixture();
  win.gBrowserInit.delayedStartupFinished = false;
  defer(win, "https://example.invalid/cold", () => true);
  assertEquals(
    win.gBrowser.tabs.length,
    1,
    "no gBrowser access before startup",
  );
  assertEquals(startupObservers.size, 1);
  assertEquals(closedObservers.size, 1);
  win.closed = true;
  for (const observer of [...closedObservers]) observer(win);
  assertEquals(startupObservers.size, 0);
  assertEquals(closedObservers.size, 0);
  assertEquals(win.floorpPendingWorkspaceExternalOpens, undefined);
});

Deno.test("cold external request survives initial document replacement before browser startup", async () => {
  const { win, defer, startupObservers, closedObservers, setReady } =
    await queueFixture();
  win.gBrowserInit.delayedStartupFinished = false;
  const calls: number[] = [];
  defer(win, "https://example.invalid/cold", (id) => calls.push(id), true);
  win.fire("unload");
  Reflect.deleteProperty(win, "floorpPendingWorkspaceExternalOpens");
  assertEquals(
    startupObservers.size,
    1,
    "initial-document unload is not window close",
  );
  win.gBrowserInit.delayedStartupFinished = true;
  for (const observer of [...startupObservers]) observer(win);
  assertEquals(startupObservers.size, 0);
  assertEquals(closedObservers.size, 0);
  assertEquals(win.floorpPendingWorkspaceExternalOpens?.size, 1);
  setReady(true);
  win.fire("floorp-workspace-container-ready");
  assertEquals(calls, [7]);
  assertEquals(win.floorpPendingWorkspaceExternalOpens?.size, 0);
});

Deno.test("a partially failed external replay is not automatically submitted twice", async () => {
  const { win, defer, setReady } = await queueFixture();
  let attempts = 0;
  defer(win, "https://example.invalid/submission", () => {
    attempts++;
    throw new Error("browser creation failed after consuming input");
  }, true);
  setReady(true);
  win.fire("floorp-workspace-container-ready");
  win.fire("floorp-workspace-container-ready");
  assertEquals(attempts, 1);
  assertEquals(win.floorpPendingWorkspaceExternalOpens?.size, 1);
  assertEquals(
    win.gBrowser.tabs[0].linkedBrowser.currentURI.spec,
    "about:blank",
  );
  win.fire("unload");
});
