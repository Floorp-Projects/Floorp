// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertStringIncludes } from "@std/assert";
import type {
  BuiltTokenFileAdapter,
  FakeGeckoOptions,
  FakeGeckoState,
} from "./os-server-token-test-types.ts";

// Build bridge/loader-modules first: exercise its actual adapter with fake Gecko APIs.
Deno.test("built adapter: Windows zero IO; Unix links guard publication and all cleanup", async () => {
  const globals = globalThis as unknown as Record<string, unknown>;
  const names = ["Services", "PathUtils", "Cc", "Ci", "Cr", "IOUtils"];
  const saved = names.map((name) =>
    Object.getOwnPropertyDescriptor(globalThis, name)
  );
  const nodes = new Map<string, string>([["/", "directory"], [
    "/profile",
    "directory",
  ]]);
  const calls: string[] = [];
  let os = "WINNT",
    mode = "dev",
    optIn = true,
    lookupThrows = false,
    swapOnWrite = false;
  const parent = (p: string): string | null =>
    p === "/" ? null : p.slice(0, p.lastIndexOf("/")) || "/";
  const file = (initial = ""): object => {
    let p = initial;
    return {
      initWithPath(path: string) {
        p = path;
      },
      get path() {
        return p;
      },
      get parent() {
        const next = parent(p);
        return next ? file(next) : null;
      },
      isSymlink() {
        calls.push("inspect:" + p);
        if (!nodes.has(p)) {
          throw Object.assign(new Error("missing"), { result: "missing" });
        }
        return nodes.get(p) === "link";
      },
      exists() {
        return nodes.has(p);
      },
      isDirectory() {
        return nodes.get(p) === "directory";
      },
      isFile() {
        return nodes.get(p) === "file";
      },
    };
  };
  const env: Record<string, unknown> = {
    Services: {
      appinfo: {
        get OS() {
          return os;
        },
      },
      prefs: { getBoolPref: () => optIn, getStringPref: () => mode },
      dirsvc: {
        get(key: string) {
          calls.push("dirsvc:" + key);
          if (lookupThrows) throw new Error("synthetic-sensitive-message");
          return { path: key === "ProfD" ? "/profile" : "/home" };
        },
      },
    },
    PathUtils: { join: (...parts: string[]) => parts.join("/") },
    Cc: { "@mozilla.org/file/local;1": { createInstance: () => file() } },
    Ci: { nsIFile: {}, nsIException: Error },
    Cr: { NS_ERROR_FILE_NOT_FOUND: "missing" },
    IOUtils: {
      makeDirectory(p: string) {
        calls.push("mkdir:" + p);
        nodes.set(p, "directory");
        return Promise.resolve();
      },
      exists(p: string) {
        calls.push("exists:" + p);
        return Promise.resolve(nodes.has(p));
      },
      setPermissions(p: string) {
        calls.push("chmod:" + p);
        return Promise.resolve();
      },
      write(p: string, data: Uint8Array) {
        calls.push("write:" + p + ":" + data.length);
        nodes.set(p, "file");
        if (swapOnWrite && data.length) {
          nodes.set("/profile/floorp-os-server", "link");
        }
        return Promise.resolve();
      },
      remove(p: string) {
        calls.push("remove:" + p);
        nodes.delete(p);
        return Promise.resolve();
      },
    },
  };
  const mutations = () =>
    calls.filter((c) => /^(mkdir|chmod|write|remove):/.test(c));
  try {
    for (const name of names) {
      Object.defineProperty(globalThis, name, {
        value: env[name],
        writable: true,
        configurable: true,
      });
    }
    const url = new URL(
      "../../bridge/loader-modules/_dist/modules/os-server/token-file.sys.mjs",
      import.meta.url,
    );
    url.search = "test=" + crypto.randomUUID();
    const adapter = await import(url.href) as {
      publishTokenFile(token: string): Promise<void>;
      cleanupTokenFile(): Promise<void>;
    };
    await assertRejects(() => adapter.publishTokenFile("fake"));
    await adapter.cleanupTokenFile();
    assertEquals(calls, []);
    mode = "production";
    await adapter.publishTokenFile("fake");
    assertEquals(calls, []);
    os = "Linux";
    mode = "dev";
    lookupThrows = true;
    const rejection = adapter.publishTokenFile("fake"); // sync lookup errors must become rejection
    await assertRejects(() => rejection);
    lookupThrows = false;
    for (
      const path of [
        "/profile",
        "/profile/floorp-os-server",
        "/profile/floorp-os-server/os-server-token",
      ]
    ) {
      nodes.set("/profile", "directory");
      nodes.set("/profile/floorp-os-server", "directory");
      nodes.delete("/profile/floorp-os-server/os-server-token");
      nodes.set(path, "link");
      calls.length = 0;
      await assertRejects(() => adapter.publishTokenFile("fake"));
      assertEquals(mutations(), []);
    }
    nodes.set("/profile", "directory");
    nodes.set("/profile/floorp-os-server", "directory");
    nodes.delete("/profile/floorp-os-server/os-server-token");
    await adapter.publishTokenFile("fake");
    nodes.set("/profile/floorp-os-server", "link");
    calls.length = 0;
    await assertRejects(() => adapter.cleanupTokenFile());
    assertEquals(mutations(), []);
    os = "WINNT";
    calls.length = 0;
    await assertRejects(() => adapter.cleanupTokenFile());
    assertEquals(calls, []);
    os = "Linux";
    nodes.set("/profile/floorp-os-server", "directory");
    await adapter.cleanupTokenFile();
    swapOnWrite = true;
    calls.length = 0;
    await assertRejects(() => adapter.publishTokenFile("fake"));
    assertEquals(calls.some((c) => c.startsWith("remove:")), false); // failure cleanup rejects replaced parent
    swapOnWrite = false;
    nodes.set("/profile/floorp-os-server", "directory");
    await adapter.cleanupTokenFile();
    os = "WINNT";
    optIn = false;
    calls.length = 0;
    await adapter.publishTokenFile("fake");
    assertEquals(calls.includes("dirsvc:Home"), true);
    assertEquals(calls.includes("write:/home/.floorp/os-server-token:4"), true);
  } finally {
    names.forEach((name, i) => {
      const d = saved[i];
      if (d) Object.defineProperty(globalThis, name, d);
      else delete globals[name];
    });
  }
});

const HOME_DIRECTORY = "/home/.floorp";
const HOME_TOKEN_PATH = HOME_DIRECTORY + "/os-server-token";

/** Mutable fake Gecko file/pref environment for the refusal regressions. */
function installFakeGecko(options: FakeGeckoOptions = {}) {
  const globals = globalThis as unknown as Record<string, unknown>;
  const names = ["Services", "PathUtils", "Cc", "Ci", "Cr", "IOUtils"];
  const saved = names.map((name) =>
    Object.getOwnPropertyDescriptor(globalThis, name)
  );
  const nodes = new Map<string, string>([
    ["/", "directory"],
    ["/profile", "directory"],
    ["/home", "directory"],
    [HOME_DIRECTORY, "directory"],
  ]);
  const calls: string[] = [];
  const state: FakeGeckoState = { os: "WINNT", mode: "dev", optIn: false };
  const parent = (p: string): string | null =>
    p === "/" ? null : p.slice(0, p.lastIndexOf("/")) || "/";
  const file = (initial = ""): object => {
    let p = initial;
    return {
      initWithPath(path: string) {
        p = path;
      },
      get path() {
        return p;
      },
      get parent() {
        const next = parent(p);
        return next ? file(next) : null;
      },
      isSymlink() {
        calls.push("inspect:" + p);
        if (!nodes.has(p)) {
          throw Object.assign(new Error("missing"), { result: "missing" });
        }
        return nodes.get(p) === "link";
      },
      exists() {
        return nodes.has(p);
      },
      isDirectory() {
        return nodes.get(p) === "directory";
      },
      isFile() {
        return nodes.get(p) === "file";
      },
    };
  };
  const env: Record<string, unknown> = {
    Services: {
      appinfo: {
        get OS() {
          return state.os;
        },
      },
      prefs: {
        getBoolPref: () => state.optIn,
        getStringPref: () => state.mode,
      },
      dirsvc: {
        get(key: string) {
          calls.push("dirsvc:" + key);
          return { path: key === "ProfD" ? "/profile" : "/home" };
        },
      },
    },
    PathUtils: { join: (...parts: string[]) => parts.join("/") },
    Cc: { "@mozilla.org/file/local;1": { createInstance: () => file() } },
    Ci: { nsIFile: {}, nsIException: Error },
    Cr: { NS_ERROR_FILE_NOT_FOUND: "missing" },
    IOUtils: {
      makeDirectory(p: string) {
        calls.push("mkdir:" + p);
        nodes.set(p, "directory");
        return Promise.resolve();
      },
      exists(p: string) {
        calls.push("exists:" + p);
        return Promise.resolve(nodes.has(p));
      },
      setPermissions(p: string) {
        calls.push("chmod:" + p);
        return Promise.resolve();
      },
      async write(p: string, data: Uint8Array) {
        calls.push("write:" + p + ":" + data.length);
        if (data.length && options.tokenWriteGate) {
          options.onTokenWriteStarted?.();
          await options.tokenWriteGate;
        }
        nodes.set(p, "file");
      },
      remove(p: string) {
        calls.push("remove:" + p);
        if (options.failOwnedRemoval) {
          return Promise.reject(new Error("synthetic removal failure"));
        }
        nodes.delete(p);
        return Promise.resolve();
      },
    },
  };
  for (const name of names) {
    Object.defineProperty(globalThis, name, {
      value: env[name],
      writable: true,
      configurable: true,
    });
  }
  return {
    state,
    nodes,
    calls,
    mutations: () =>
      calls.filter((c) => /^(mkdir|chmod|write|remove):/.test(c)),
    restore() {
      names.forEach((name, i) => {
        const d = saved[i];
        if (d) Object.defineProperty(globalThis, name, d);
        else delete globals[name];
      });
    },
  };
}

async function loadBuiltAdapter(): Promise<BuiltTokenFileAdapter> {
  const url = new URL(
    "../../bridge/loader-modules/_dist/modules/os-server/token-file.sys.mjs",
    import.meta.url,
  );
  url.search = "test=" + crypto.randomUUID();
  return await import(url.href) as BuiltTokenFileAdapter;
}

Deno.test("built adapter: initial Windows opt-in refusal stays before dirsvc and IO", async () => {
  const fake = installFakeGecko();
  try {
    const adapter = await loadBuiltAdapter();
    fake.state.optIn = true;
    fake.state.mode = "dev";
    await assertRejects(() => adapter.publishTokenFile("fake"));
    assertEquals(fake.calls, []);
    assertEquals(fake.nodes.has(HOME_TOKEN_PATH), false);
  } finally {
    fake.restore();
  }
});

Deno.test("built adapter: Windows opt-in refusal withdraws own legacy advert", async () => {
  const fake = installFakeGecko();
  try {
    const adapter = await loadBuiltAdapter();
    await adapter.publishTokenFile("fake");
    assertEquals(fake.nodes.get(HOME_TOKEN_PATH), "file");
    fake.state.optIn = true;
    fake.state.mode = "dev";
    fake.calls.length = 0;
    await assertRejects(() => adapter.publishTokenFile("fake"));
    assertEquals(fake.nodes.has(HOME_TOKEN_PATH), false);
    assertEquals(
      fake.calls.filter((c) => c.startsWith("remove:")),
      ["remove:" + HOME_TOKEN_PATH],
    );
    assertEquals(fake.calls.some((c) => c.startsWith("dirsvc:")), false);
    assertEquals(fake.nodes.get(HOME_DIRECTORY), "directory");
  } finally {
    fake.restore();
  }
});

Deno.test("built adapter: Windows opt-in refusal invalidates legacy write before queue starts", async () => {
  const fake = installFakeGecko();
  try {
    const adapter = await loadBuiltAdapter();
    const queued = adapter.publishTokenFile("fake");
    fake.state.optIn = true;
    const refusal = adapter.publishTokenFile("fake");
    await assertRejects(() => refusal);
    await queued;
    assertEquals(fake.nodes.has(HOME_TOKEN_PATH), false);
    assertEquals(fake.mutations(), []);
    assertEquals(fake.calls.filter((c) => c.startsWith("dirsvc:")), [
      "dirsvc:Home",
    ]);
  } finally {
    fake.restore();
  }
});

Deno.test("built adapter: Windows opt-in refusal drains in-flight legacy write", async () => {
  const gate = Promise.withResolvers<void>();
  const started = Promise.withResolvers<void>();
  const fake = installFakeGecko({
    tokenWriteGate: gate.promise,
    onTokenWriteStarted: () => started.resolve(),
  });
  try {
    const adapter = await loadBuiltAdapter();
    const inFlight = adapter.publishTokenFile("fake");
    await started.promise;
    fake.state.optIn = true;
    fake.state.mode = "dev";
    const refusal = adapter.publishTokenFile("fake");
    gate.resolve();
    await assertRejects(() => refusal);
    await inFlight;
    assertEquals(fake.nodes.has(HOME_TOKEN_PATH), false);
  } finally {
    fake.restore();
  }
});

Deno.test("built adapter: refusal rejects instead of swallowing withdrawal failure", async () => {
  const fake = installFakeGecko({ failOwnedRemoval: true });
  try {
    const adapter = await loadBuiltAdapter();
    await adapter.publishTokenFile("fake");
    fake.state.optIn = true;
    fake.state.mode = "dev";
    fake.calls.length = 0;
    const error = await assertRejects(() => adapter.publishTokenFile("fake"));
    assertStringIncludes(String(error), "synthetic removal failure");
    assertEquals(
      fake.calls.filter((c) => c.startsWith("remove:")),
      ["remove:" + HOME_TOKEN_PATH],
    );
  } finally {
    fake.restore();
  }
});
