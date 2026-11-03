// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals, assertRejects } from "@std/assert";
import { runInNewContext } from "node:vm";
const sourcePath =
  ".github/patches/floorp-runtime/common/chrome-extension-capture.patch";
const artifactPath = "tools/patches/chrome-extension-capture.patch";
const prefix = "toolkit/components/extensions/";
async function parts(path: string) {
  return (await Deno.readTextFile(path)).split("diff --git ").slice(1);
}
function added(sections: string[], path: string) {
  const section = sections.find((value) =>
    value.startsWith(`a/${path} b/${path}\n`)
  );
  assert(section);
  return section.split("\n").filter((line) =>
    line.startsWith("+") && !line.startsWith("+++")
  ).map((line) => line.slice(1)).join("\n");
}
type API = {
  getMediaStreamId(
    options?: { targetTabId?: number; consumerTabId?: number },
  ): Promise<string>;
  _consumeStreamId(
    token: string,
  ): Promise<{ sessionId: string; browserId: number }>;
  _streamStarted(id: string): Promise<void>;
  _streamStopped(id: string): Promise<void>;
  getCapturedTabs(): Promise<{ status: string }[]>;
};
async function harness() {
  const source = added(
    await parts(sourcePath),
    prefix + "parent/ext-tabCapture.js",
  );
  const timers = new Map<number, () => void>();
  let nextTimer = 0;
  let nextUUID = 0;
  const listeners = new Map<string, Set<(subject: unknown) => void>>();
  const consumer = {
    innerWindowId: 41,
    documentPrincipal: {
      originNoSuffix: "moz-extension://fixture",
      equals(other: unknown) {
        return this === other;
      },
    },
  };
  const target = {
    innerWindowId: 21,
    documentPrincipal: {
      isSystemPrincipal: false,
      isExpandedPrincipal: false,
      addonPolicy: null,
    },
  };
  type Progress = {
    onStateChange(
      webProgress: { isTopLevel: boolean },
      request: unknown,
      flags: number,
    ): void;
    onLocationChange(webProgress: { isTopLevel: boolean }): void;
  };
  const progressListeners = new Set<Progress>();
  const browser = {
    browsingContext: { id: 31, browserId: 32, currentWindowGlobal: target },
    currentURI: {
      spec: "https://example.com",
      schemeIs: (scheme: string) => scheme === "https",
    },
    webProgress: {},
    addProgressListener(listener: Progress) {
      progressListeners.add(listener);
    },
    removeProgressListener(listener: Progress) {
      progressListeners.delete(listener);
    },
  };
  const owner = { gBrowser: { selectedTab: undefined as unknown } };
  const tab = { linkedBrowser: browser, documentGlobal: owner };
  owner.gBrowser.selectedTab = tab;
  const tracker = { activeTab: tab, getId: () => 1 };
  let permitted = true, accessible = true;
  const originalId = "abcdefghijklmnopabcdefghijklmnop";
  const extensionListeners = new Map<
    string,
    (event: string, removed: { permissions: string[] }) => void
  >();
  const extension = {
    id: `${originalId}@chromium-extension`,
    manifest: {
      browser_specific_settings: {
        gecko: {
          floorp_chrome_extension: {
            schema_version: 1,
            original_id: originalId,
          },
        },
      },
    },
    baseURI: { prePath: "moz-extension://fixture" },
    hasPermission: () => permitted,
    tabManager: {
      get: () => ({ nativeTab: tab }),
      canAccessTab: () => accessible,
      hasActiveTabPermission: () => false,
    },
    floorpTabCaptureGrant: { browsingContextId: 31, innerWindowId: 21 },
    on: (
      event: string,
      listener: (event: string, removed: { permissions: string[] }) => void,
    ) => extensionListeners.set(event, listener),
    off: (event: string) => extensionListeners.delete(event),
  };
  const context = {
    unloaded: false,
    envType: "addon_parent",
    actor: { manager: { isCurrentGlobal: true, innerWindowId: 41 } },
    browsingContext: {
      currentWindowContext: consumer,
      top: { currentWindowContext: consumer },
    },
    callContextData: { isHandlingUserInput: true },
    pendingEventBrowser: browser as typeof browser | null,
    closures: [] as { close(): void }[],
    callOnClose(closure: { close(): void }) {
      this.closures.push(closure);
    },
    forgetOnClose(closure: { close(): void }) {
      this.closures = this.closures.filter((item) => item !== closure);
    },
  };
  class ExtensionAPI {
    constructor(public extension: unknown) {}
    emit(_event: string, _value: unknown) {}
    on(_event: string, _listener: unknown) {}
    off(_event: string, _listener: unknown) {}
  }
  class ExtensionError extends Error {}
  class EventManager {
    constructor(_options: unknown) {}
    api() {
      return {};
    }
  }
  let permission: { expireType: number; expireTime: number } | undefined;
  const scope = {
    Date: { now: () => 1000 },
    Ci: {
      nsIWebProgress: { NOTIFY_STATE_DOCUMENT: 1, NOTIFY_LOCATION: 2 },
      nsIWebProgressListener: { STATE_START: 1, STATE_IS_DOCUMENT: 2 },
    },
    ExtensionAPIPersistent: ExtensionAPI,
    EventManager,
    ExtensionUtils: { ExtensionError },
    tabTracker: tracker,
    ChromeUtils: {
      registerWindowActor() {},
      generateQI: () => () => {},
      importESModule: () => ({
        ChromeExtensionCaptureRegistry: {
          register() {},
          drop() {},
          shutdown() {},
        },
        setTimeout(callback: () => void) {
          timers.set(++nextTimer, callback);
          return nextTimer;
        },
        clearTimeout(id: number) {
          timers.delete(id);
        },
      }),
    },
    Services: {
      uuid: { generateUUID: () => `uuid-${++nextUUID}` },
      perms: {
        UNKNOWN_ACTION: 0,
        ALLOW_ACTION: 1,
        EXPIRE_TIME: 2,
        testExactPermissionFromPrincipal() {
          return permission ? 1 : 0;
        },
        addFromPrincipal(
          _principal: unknown,
          _name: string,
          _allow: number,
          expireType: number,
          expireTime: number,
        ) {
          permission = { expireType, expireTime };
        },
        getPermissionObject() {
          return permission;
        },
        removeFromPrincipal() {
          permission = undefined;
        },
      },
      obs: {
        addObserver(observer: (subject: unknown) => void, topic: string) {
          if (!listeners.has(topic)) listeners.set(topic, new Set());
          listeners.get(topic)!.add(observer);
        },
        removeObserver(observer: (subject: unknown) => void, topic: string) {
          listeners.get(topic)?.delete(observer);
        },
      },
    },
  };
  runInNewContext(source, scope);
  const Constructor = (scope as unknown as {
    tabCapture: new (
      extension: unknown,
    ) => {
      tokens: Map<string, { expires: number }>;
      getAPI(context: unknown): { tabCapture: API };
      onShutdown(): void;
    };
  }).tabCapture;
  const instance = new Constructor(extension);
  const api = instance.getAPI(context).tabCapture;
  return {
    api,
    instance,
    context,
    consumer,
    target,
    browser,
    tab,
    owner,
    tracker,
    extension,
    timers,
    listeners,
    progressListeners,
    setPermission(value: { expireType: number; expireTime: number }) {
      permission = value;
    },
    getPermission() {
      return permission;
    },
    setPermitted(value: boolean) {
      permitted = value;
    },
    setAccessible(value: boolean) {
      accessible = value;
    },
    removed() {
      extensionListeners.get("remove-permissions")!("remove-permissions", {
        permissions: ["tabCapture"],
      });
    },
  };
}
Deno.test("capture token requires trusted invocation, gesture and accessible active target", async () => {
  const h = await harness();
  h.context.callContextData.isHandlingUserInput = false;
  await assertRejects(() => h.api.getMediaStreamId(), Error, "user gesture");
  h.context.callContextData.isHandlingUserInput = true;
  h.extension.floorpTabCaptureGrant.innerWindowId = 0;
  h.extension.tabManager.hasActiveTabPermission = () => true;
  await assertRejects(
    () => h.api.getMediaStreamId(),
    Error,
    "action invocation",
  );
  h.extension.floorpTabCaptureGrant.innerWindowId = 21;
  h.setAccessible(false);
  await assertRejects(() => h.api.getMediaStreamId(), Error, "active tab");
  h.setAccessible(true);
  h.context.pendingEventBrowser = null;
  h.tracker.activeTab = { ...h.tab };
  await assertRejects(() => h.api.getMediaStreamId(), Error, "active tab");
  h.tracker.activeTab = h.tab;
  await assertRejects(
    () => h.api.getMediaStreamId({ consumerTabId: 1 }),
    Error,
    "consumerTabId",
  );
  h.instance.onShutdown();
  assertEquals(h.timers.size, 0);
});
Deno.test("capture token is opaque, single use and bound to real current documents", async () => {
  const h = await harness();
  const token = await h.api.getMediaStreamId();
  assert(token.startsWith("floorp-tab-capture-"));
  assertEquals(h.timers.size, 1);
  const consumed = await h.api._consumeStreamId(token);
  assertEquals(consumed.browserId, 32);
  assertEquals(h.timers.size, 0);
  await assertRejects(
    () => h.api._consumeStreamId(token),
    Error,
    "Invalid, expired",
  );
  await h.api._streamStarted(consumed.sessionId);
  assertEquals((await h.api.getCapturedTabs())[0].status, "active");
  await h.api._streamStopped(consumed.sessionId);
  assertEquals((await h.api.getCapturedTabs()).length, 0);
  const navigated = await h.api.getMediaStreamId();
  h.target.innerWindowId = 22;
  await assertRejects(
    () => h.api._consumeStreamId(navigated),
    Error,
    "navigated",
  );
  h.target.innerWindowId = 21;
  const stale = await h.api.getMediaStreamId();
  h.context.actor.manager.isCurrentGlobal = false;
  await assertRejects(
    () => h.api._consumeStreamId(stale),
    Error,
    "live document",
  );
  h.context.actor.manager.isCurrentGlobal = true;
  h.context.actor.manager.innerWindowId = 42;
  await assertRejects(
    () => h.api._consumeStreamId(stale),
    Error,
    "live document",
  );
  h.context.actor.manager.innerWindowId = 41;
  h.instance.onShutdown();
  assertEquals(h.timers.size, 0);
});
Deno.test("capture revalidates target after native start and revokes active sessions on navigation", async () => {
  const h = await harness();
  const token = await h.api.getMediaStreamId();
  const { sessionId } = await h.api._consumeStreamId(token);
  h.target.innerWindowId = 22;
  await assertRejects(
    () => h.api._streamStarted(sessionId),
    Error,
    "navigated before stream start",
  );
  assertEquals((await h.api.getCapturedTabs()).length, 0);
  h.target.innerWindowId = 21;
  const next = await h.api._consumeStreamId(await h.api.getMediaStreamId());
  await h.api._streamStarted(next.sessionId);
  assertEquals((await h.api.getCapturedTabs()).length, 1);
  for (const listener of h.progressListeners) {
    listener.onStateChange({ isTopLevel: true }, null, 3);
  }
  assertEquals((await h.api.getCapturedTabs()).length, 0);
  assertEquals(h.listeners.get("window-global-created")?.size, 0);
  h.instance.onShutdown();
});
Deno.test("capture permission removal, expiry and context shutdown invalidate grants", async () => {
  const h = await harness();
  const expired = await h.api.getMediaStreamId();
  [...h.timers.values()][0]();
  assertEquals(h.instance.tokens.size, 0);
  await assertRejects(
    () => h.api._consumeStreamId(expired),
    Error,
    "Invalid, expired",
  );
  const token = await h.api.getMediaStreamId();
  h.setPermitted(false);
  await assertRejects(
    () => h.api._consumeStreamId(token),
    Error,
    "live document",
  );
  h.setPermitted(true);
  const active = await h.api._consumeStreamId(token);
  await h.api._streamStarted(active.sessionId);
  h.removed();
  assertEquals((await h.api.getCapturedTabs()).length, 0);
  const unused = await h.api.getMediaStreamId();
  for (const closure of h.context.closures) closure.close();
  await assertRejects(
    () => h.api._consumeStreamId(unused),
    Error,
    "Invalid, expired",
  );
  h.instance.onShutdown();
  assertEquals(h.timers.size, 0);
});
Deno.test("capture cannot authorize system or addon principal behind an allowed URL", async () => {
  const h = await harness();
  h.target.documentPrincipal.isSystemPrincipal = true;
  await assertRejects(() => h.api.getMediaStreamId(), Error, "privileged");
  h.target.documentPrincipal.isSystemPrincipal = false;
  h.target.documentPrincipal.isExpandedPrincipal = true;
  await assertRejects(() => h.api.getMediaStreamId(), Error, "privileged");
  h.instance.onShutdown();
});
Deno.test("capture preserves another pending backend permission and rejects foreign embedding", async () => {
  const h = await harness();
  const existing = { expireType: 2, expireTime: 11000 };
  h.setPermission(existing);
  await assertRejects(
    () => h.api._consumeStreamId("floorp-tab-capture-forged"),
    Error,
  );
  const token = await h.api.getMediaStreamId();
  await assertRejects(
    () => h.api._consumeStreamId(token),
    Error,
    "Another video permission",
  );
  assertEquals(
    h.getPermission(),
    existing,
    "an equal expiry timestamp does not grant ownership",
  );
  h.context.browsingContext.top.currentWindowContext = {
    ...h.consumer,
    documentPrincipal: {
      originNoSuffix: "https://foreign.example",
      equals: () => false,
    },
  };
  await assertRejects(() => h.api.getMediaStreamId(), Error, "live document");
  h.instance.onShutdown();
  assertEquals(h.getPermission(), existing);
});
Deno.test("capture source and extracted artifact patches match; action grant is after real click validation", async () => {
  const source = await parts(sourcePath);
  const artifact = await parts(artifactPath);
  const applicable = source.filter((section) =>
    !section.split("\n")[0].includes("/jar.mn ") &&
    !section.split("\n")[0].includes("/moz.build ")
  );
  assertEquals(artifact.length, applicable.length);
  for (const section of applicable) {
    const path = section.split(" ")[0].slice(2);
    const relative = path.slice(prefix.length);
    const mapped =
      (relative.startsWith("schemas/") || relative.startsWith("parent/") ||
          relative.startsWith("child/") || relative === "ext-toolkit.json"
        ? "chrome/toolkit/content/extensions/"
        : "modules/") + relative;
    assertEquals(
      artifact.find((value) => value.startsWith(`a/${mapped} `)),
      section.replaceAll(path, mapped),
    );
  }
  const action = source.find((section) =>
    section.startsWith(`a/${prefix}ExtensionActions.sys.mjs `)
  );
  assert(action);
  assert(action.includes("     this.setActiveTabForPreload(null);"));
  assert(
    !action.includes("if (tab)"),
    "preloading must never grant capture authority",
  );
  const schema = JSON.parse(added(source, prefix + "schemas/tab_capture.json"));
  assertEquals(
    schema[1].functions.find((f: { name: string }) =>
      f.name === "getMediaStreamId"
    ).requireUserInput,
    true,
  );
});
