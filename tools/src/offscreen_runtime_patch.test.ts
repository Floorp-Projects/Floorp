// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects } from "@std/assert";
import { runInNewContext } from "node:vm";

const sourcePath =
  ".github/patches/floorp-runtime/common/chrome-extension-offscreen.patch";
const artifactPath = "tools/patches/chrome-extension-offscreen.patch";
const runtimePrefix = "toolkit/components/extensions/";

async function patchParts(path: string): Promise<string[]> {
  return (await Deno.readTextFile(path)).split("diff --git ").slice(1);
}

function addedFile(parts: string[], path: string): string {
  const part = parts.find((part) => part.startsWith(`a/${path} b/${path}\n`));
  assert(part, `Missing patch for ${path}`);
  return part.split("\n").filter((line) =>
    line.startsWith("+") && !line.startsWith("+++")
  ).map((line) => line.slice(1)).join("\n");
}

type API = {
  createDocument(options: {
    url: string;
    reasons: string[];
    justification: string;
  }): Promise<void>;
  closeDocument(): Promise<void>;
  hasDocument(): Promise<boolean>;
};

function deferred<T>() {
  let resolve!: (value: T) => void;
  let reject!: (reason: Error) => void;
  const promise = new Promise<T>((res, rej) => {
    resolve = res;
    reject = rej;
  });
  return { promise, resolve, reject };
}

async function harness(compat = true) {
  const source = addedFile(
    await patchParts(sourcePath),
    runtimePrefix + "parent/ext-offscreen.js",
  );
  const pages: HiddenPage[] = [];
  const timers = new Map<number, { callback: () => void; delay: number }>();
  let timerId = 0;
  let load = deferred<{
    callOnClose(close: { close(): void }): void;
  }>();
  let documentClose: { close(): void } | undefined;
  const context = {
    callOnClose(close: { close(): void }) {
      documentClose = close;
    },
  };
  const media = {
    isAudible: false,
    listener: undefined as (() => void) | undefined,
    addEventListener(_type: string, listener: () => void) {
      this.listener = listener;
    },
    removeEventListener() {
      this.listener = undefined;
    },
  };
  class HiddenPage {
    unloaded = false;
    browser = {
      browsingContext: { mediaController: media },
      fixupAndLoadURIString(_url: string) {},
    };
    constructor(public extension: unknown, public viewType: string) {
      pages.push(this);
    }
    createBrowserElement() {
      return Promise.resolve(this.browser);
    }
    shutdown() {
      this.unloaded = true;
    }
  }
  class ExtensionError extends Error {}
  const originalId = "abcdefghijklmnopabcdefghijklmnop";
  const base = new URL("moz-extension://offscreen-test/");
  const extension = {
    id: compat ? `${originalId}@chromium-extension` : "firefox@example.com",
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
    principal: {},
    baseURI: {
      prePath: `${base.protocol}//${base.host}`,
      resolve: (path: string) => new URL(path, base).href,
    },
  };
  class ExtensionAPI {
    constructor(public extension: unknown) {}
  }
  const parent = {
    HiddenExtensionPage: HiddenPage,
    apiManager: { emit(_name: string, _browser: unknown) {} },
    promiseExtensionViewLoaded(
      _browser: unknown,
      _message: string,
      signal: AbortSignal,
    ) {
      signal.throwIfAborted();
      const pendingLoad = load;
      signal.addEventListener("abort", () => {
        pendingLoad.reject(
          new ExtensionError("Offscreen document creation cancelled"),
        );
      }, { once: true });
      return pendingLoad.promise;
    },
  };
  const scope = {
    Cu: { importGlobalProperties(_names: string[]) {} },
    ChromeUtils: {
      importESModule(path: string) {
        if (path.includes("ExtensionParent")) {
          return { ExtensionParent: parent };
        }
        return {
          setTimeout(callback: () => void, delay: number) {
            timers.set(++timerId, { callback, delay });
            return timerId;
          },
          clearTimeout(id: number) {
            timers.delete(id);
          },
        };
      },
    },
    Services: {
      io: {
        newURI(input: string) {
          const uri = new URL(input);
          return {
            spec: uri.href,
            prePath: `${uri.protocol}//${uri.host}`,
            schemeIs: (scheme: string) => uri.protocol === `${scheme}:`,
          };
        },
      },
    },
    AbortController,
    ExtensionAPI,
    ExtensionUtils: { ExtensionError },
  };
  runInNewContext(source, scope);
  const Constructor = (scope as unknown as {
    offscreen: new (extension: unknown) => {
      getAPI(context: unknown): { offscreen?: API };
      onShutdown(): void;
    };
  }).offscreen;
  const instance = new Constructor(extension);
  return {
    api: instance.getAPI({}).offscreen,
    secondAPI: () => instance.getAPI({}).offscreen!,
    instance,
    pages,
    media,
    timers,
    ready() {
      load.resolve(context);
    },
    fail() {
      load.reject(
        new ExtensionError("Browser load failed: NS_ERROR_FILE_NOT_FOUND"),
      );
    },
    nextLoad() {
      load = deferred<typeof context>();
    },
    closeDocumentContext() {
      documentClose?.close();
    },
  };
}

const options = {
  url: "offscreen.html",
  reasons: ["DOM_PARSER"],
  justification: "Parse an extension document",
};

Deno.test("offscreen is extension-owned and reserves its singleton before load", async () => {
  const h = await harness();
  assert(h.api);
  const creation = h.api.createDocument(options);
  assertEquals(await h.api.hasDocument(), true);
  await assertRejects(
    () => h.secondAPI().createDocument(options),
    Error,
    "single offscreen document",
  );
  h.ready();
  await creation;
  assertEquals(h.pages.length, 1);
  assertEquals(h.pages[0].viewType, "offscreen");
  assertEquals(await h.secondAPI().hasDocument(), true);
  await h.secondAPI().closeDocument();
  assert(h.pages[0].unloaded);
  assertEquals(await h.api.hasDocument(), false);
});

Deno.test("offscreen cancellation and load failures release their hosts", async () => {
  const h = await harness();
  assert(h.api);
  const creation = h.api.createDocument(options);
  // Let build subscribe to the native load completion before cancellation.
  await Promise.resolve();
  await h.api.closeDocument();
  await assertRejects(() => creation, Error);
  assert(h.pages[0].unloaded);
  h.nextLoad();
  const failed = h.api.createDocument(options);
  await Promise.resolve();
  h.fail();
  await assertRejects(() => failed, Error, "NS_ERROR_FILE_NOT_FOUND");
  assert(h.pages[1].unloaded);
  assertEquals(await h.api.hasDocument(), false);
  h.nextLoad();
  const retried = h.api.createDocument(options);
  h.ready();
  await retried;
  h.instance.onShutdown();
  assert(h.pages[2].unloaded);
});

Deno.test("offscreen rejects foreign URLs, blank justification and Firefox opt-in spoofing", async () => {
  const h = await harness();
  assert(h.api);
  for (
    const url of [
      "https://example.com/offscreen.html",
      "moz-extension://other-extension/offscreen.html",
      "javascript:alert(1)",
    ]
  ) {
    await assertRejects(
      () => h.api!.createDocument({ ...options, url }),
      Error,
      "belong to the extension",
    );
  }
  await assertRejects(
    () => h.api!.createDocument({ ...options, justification: " " }),
    Error,
    "Justification",
  );
  assertEquals(h.pages.length, 0);
  assertEquals((await harness(false)).api, undefined);
});

Deno.test("offscreen audio lifetime tracks native audibility for 30 seconds", async () => {
  const h = await harness();
  assert(h.api);
  const created = h.api.createDocument({
    ...options,
    reasons: ["AUDIO_PLAYBACK"],
  });
  h.ready();
  await created;
  assertEquals(h.timers.size, 1);
  assertEquals([...h.timers.values()][0].delay, 30000);
  h.media.isAudible = true;
  h.media.listener!();
  assertEquals(h.timers.size, 0);
  h.media.isAudible = false;
  h.media.listener!();
  [...h.timers.values()][0].callback();
  assertEquals(await h.api.hasDocument(), false);
  assert(h.pages[0].unloaded);
  assertEquals(h.media.listener, undefined);
  assertEquals(h.timers.size, 0);
});

Deno.test("offscreen document loss and extension shutdown clean independent hosts", async () => {
  const h = await harness();
  assert(h.api);
  const created = h.api.createDocument(options);
  h.ready();
  await created;
  assertEquals(h.timers.size, 0, "DOM parsing has no audio timeout");
  h.closeDocumentContext();
  assertEquals(await h.api.hasDocument(), false);
  assert(h.pages[0].unloaded);
  await assertRejects(
    () => h.api!.closeDocument(),
    Error,
    "No current offscreen",
  );
});

Deno.test("extension shutdown cancels a pending offscreen create", async () => {
  const h = await harness();
  assert(h.api);
  const pending = h.api.createDocument(options);
  h.instance.onShutdown();
  await assertRejects(() => pending, Error);
  assertEquals(await h.api.hasDocument(), false);
  assert(h.pages[0].unloaded);
});

Deno.test("offscreen API restriction keeps root runtime namespaces and members", async () => {
  const parts = await patchParts(sourcePath);
  const child = parts.find((part) =>
    part.startsWith(`a/${runtimePrefix}ExtensionChild.sys.mjs `)
  );
  assert(child);
  const guard = child.split("\n").filter((line) =>
    line.startsWith("+") && !line.startsWith("+++")
  ).map((line) => line.slice(1)).join("\n");
  const shouldInject = new Function(
    "namespace",
    "name",
    guard + "\nreturn true;",
  ) as (
    this: unknown,
    namespace: string,
    name: string,
  ) => boolean;
  const context = { context: { viewType: "offscreen" } };
  assertEquals(shouldInject.call(context, "", "runtime"), true);
  assertEquals(shouldInject.call(context, "runtime", "sendMessage"), true);
  assertEquals(
    shouldInject.call(context, "runtime", "getBackgroundPage"),
    false,
  );
  assertEquals(shouldInject.call(context, "", "storage"), false);
  assertEquals(shouldInject.call(context, "storage", "local"), false);
  for (
    const namespace of [
      "offscreen",
      "sidePanel",
      "tabCapture",
      "desktopCapture",
      "fontSettings",
      "tts",
    ]
  ) {
    const originalId = "abcdefghijklmnopabcdefghijklmnop";
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
    };
    const background = { context: { viewType: "background", extension } };
    assertEquals(shouldInject.call(background, "", namespace), true);
    assertEquals(shouldInject.call(background, namespace, "open"), true);
    extension.id = "ordinary-firefox@example.com";
    assertEquals(shouldInject.call(background, "", namespace), false);
    assertEquals(shouldInject.call(background, namespace, "open"), false);
    const missingMarker = {
      context: {
        viewType: "background",
        extension: { manifest: {}, id: "firefox@example.com" },
      },
    };
    assertEquals(shouldInject.call(missingMarker, "", namespace), false);
  }
});

Deno.test("offscreen production and artifact patches carry identical runtime code", async () => {
  const source = await patchParts(sourcePath);
  const artifact = await patchParts(artifactPath);
  const applicable = source.filter((part) =>
    !part.split("\n")[0].includes("/jar.mn ") &&
    !part.split("\n")[0].includes("/test/")
  );
  assertEquals(artifact.length, applicable.length);
  for (const part of applicable) {
    const path = part.split(" ")[0].slice(2);
    const relative = path.slice(runtimePrefix.length);
    const mapped = relative.startsWith("schemas/") ||
        relative.startsWith("parent/") || relative === "ext-toolkit.json"
      ? `chrome/toolkit/content/extensions/${relative}`
      : `modules/${relative}`;
    assertEquals(
      artifact.find((candidate) => candidate.startsWith(`a/${mapped} `)),
      part.replaceAll(path, mapped),
      path,
    );
  }
  const schema = JSON.parse(
    addedFile(source, runtimePrefix + "schemas/offscreen.json"),
  );
  assertEquals(schema[0].types[0].$extend, "PermissionNoPrompt");
  assertEquals(schema[1].permissions, ["offscreen"]);
  assertEquals(schema[1].types[1].properties.reasons.minItems, 1);
});
