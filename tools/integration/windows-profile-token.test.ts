// SPDX-License-Identifier: MPL-2.0
// Built-module routing/lifecycle contracts. Actual filesystem security is tested
// by native/windows-profile-token/Tests.cpp against the real Windows kernel.
import { assertEquals, assertRejects } from "@std/assert";

import type { Adapter, Options } from "./windows-profile-token-types.ts";
async function fixture(options: Options = {}) {
  const calls: string[] = [];
  const state = {
    optIn: true,
    root: "/profile",
    token: "",
    removeFailures: options.removeFailures ?? 0,
    permanentRemoveFailure: options.permanentRemoveFailure ?? false,
  };
  const names = ["Services", "PathUtils", "Cc", "Ci", "Cr", "IOUtils"];
  const saved = names.map((name) =>
    Object.getOwnPropertyDescriptor(globalThis, name)
  );
  const native = {
    get protocolVersion() {
      calls.push("native:version");
      return options.version ?? 1;
    },
    prepare(root: string) {
      calls.push("native:prepare:" + root);
    },
    write(token: string) {
      calls.push("native:write");
      state.token = token;
      if (options.failWrite) throw new Error("synthetic write failure");
    },
    secure: options.missingSecure ? undefined : () => {
      calls.push("native:secure");
    },
    remove() {
      calls.push("native:remove");
      if (state.permanentRemoveFailure) {
        throw new Error("synthetic permanent refusal");
      }
      if (state.removeFailures-- > 0) {
        throw Object.assign(new Error("synthetic reader lock"), {
          result: "locked",
        });
      }
      state.token = "";
    },
  };
  const generic = (name: string) => {
    calls.push("generic:" + name);
    return Promise.resolve();
  };
  const env: Record<string, unknown> = {
    Services: {
      appinfo: { OS: "WINNT" },
      prefs: { getBoolPref: () => state.optIn, getStringPref: () => "dev" },
      dirsvc: {
        get(key: string) {
          calls.push("dirsvc:" + key);
          return { path: key === "ProfD" ? state.root : "/home" };
        },
      },
    },
    PathUtils: {
      join: (...parts: string[]) => parts.join("/"),
      parent: (path: string) => path.slice(0, path.lastIndexOf("/")),
    },
    Cc: {
      "@floorp.app/private-profile-token;1": {
        createInstance() {
          calls.push("native:create");
          return native;
        },
      },
    },
    Ci: {
      nsISupports: {},
      nsIFloorpPrivateProfileToken: options.missingInterface ? undefined : {},
      nsIFile: {},
      nsIException: Error,
    },
    Cr: {
      NS_ERROR_FILE_NOT_FOUND: "missing",
      NS_ERROR_FILE_IS_LOCKED: "locked",
    },
    IOUtils: {
      makeDirectory: () => generic("mkdir"),
      setPermissions: () => generic("chmod"),
      write: () => generic("write"),
      remove: () => generic("remove"),
      exists: () => {
        calls.push("generic:exists");
        return Promise.resolve(true);
      },
    },
  };
  for (const name of names) {
    Object.defineProperty(globalThis, name, {
      value: env[name],
      configurable: true,
      writable: true,
    });
  }
  const url = new URL(
    "../../bridge/loader-modules/_dist/modules/os-server/token-file.sys.mjs",
    import.meta.url,
  );
  url.search = "test=" + crypto.randomUUID();
  const adapter = await import(url.href) as Adapter;
  return {
    adapter,
    calls,
    state,
    restore() {
      for (const [index, name] of names.entries()) {
        const descriptor = saved[index];
        if (descriptor) Object.defineProperty(globalThis, name, descriptor);
        else Reflect.deleteProperty(globalThis, name);
      }
    },
  };
}

Deno.test("built native route: capability precedes ProfD; all profile operations use native owner", async () => {
  const f = await fixture();
  try {
    await f.adapter.publishTokenFile("SYNTHETIC-NATIVE-TOKEN");
    assertEquals(f.state.token, "SYNTHETIC-NATIVE-TOKEN");
    assertEquals(f.calls, [
      "native:create",
      "native:version",
      "dirsvc:ProfD",
      "native:prepare:/profile",
      "native:write",
      "native:secure",
    ]);
    f.state.root = "/foreign-profile";
    f.state.optIn = false;
    await f.adapter.cleanupTokenFile();
    assertEquals(f.calls.at(-1), "native:remove");
    assertEquals(f.calls.filter((call) => call.startsWith("dirsvc:")), [
      "dirsvc:ProfD",
    ]);
    assertEquals(f.calls.filter((call) => call.startsWith("generic:")), []);
    assertEquals(f.state.token, "");
  } finally {
    f.restore();
  }
});
for (
  const [name, options] of [
    ["missing interface", { missingInterface: true }],
    ["unknown ABI", { version: 2 }],
    ["incomplete ABI", { missingSecure: true }],
  ] as const
) {
  Deno.test(
    "built native route: " + name + " refuses before directory or file IO",
    async () => {
      const f = await fixture(options);
      try {
        await assertRejects(() => f.adapter.publishTokenFile("SYNTHETIC"));
        await f.adapter.cleanupTokenFile();
        assertEquals(
          f.calls.some((call) =>
            call.startsWith("dirsvc:") || call.startsWith("generic:")
          ),
          false,
        );
        assertEquals(
          f.calls.some((call) =>
            /^native:(prepare|write|secure|remove)/.test(call)
          ),
          false,
        );
      } finally {
        f.restore();
      }
    },
  );
}
Deno.test("built native route: failed native write cleans only captured native owner", async () => {
  const f = await fixture({ failWrite: true });
  try {
    await assertRejects(() => f.adapter.publishTokenFile("SYNTHETIC"));
    assertEquals(f.calls.slice(-3), [
      "native:prepare:/profile",
      "native:write",
      "native:remove",
    ]);
    assertEquals(f.calls.some((call) => call.startsWith("generic:")), false);
    assertEquals(f.state.token, "");
    const count = f.calls.length;
    await f.adapter.cleanupTokenFile();
    assertEquals(f.calls.length, count);
  } finally {
    f.restore();
  }
});

Deno.test("built native cleanup: transient reader lock retries captured owner without fresh lookup", async () => {
  const f = await fixture({ removeFailures: 2 });
  try {
    await f.adapter.publishTokenFile("SYNTHETIC");
    f.state.optIn = false;
    f.state.root = "/foreign-profile";
    await f.adapter.cleanupTokenFile();
    assertEquals(f.calls.filter((c) => c === "native:remove").length, 3);
    assertEquals(f.calls.filter((c) => c.startsWith("dirsvc:")).length, 1);
    assertEquals(f.calls.some((c) => c.startsWith("generic:")), false);
    assertEquals(f.state.token, "");
  } finally {
    f.restore();
  }
});

Deno.test("built native cleanup: permanent refusal retains ownership for explicit later retry", async () => {
  const f = await fixture({ permanentRemoveFailure: true });
  try {
    await f.adapter.publishTokenFile("SYNTHETIC");
    await assertRejects(() => f.adapter.cleanupTokenFile());
    assertEquals(f.calls.filter((c) => c === "native:remove").length, 1);
    assertEquals(f.state.token, "SYNTHETIC");
    f.state.permanentRemoveFailure = false;
    await f.adapter.cleanupTokenFile();
    assertEquals(f.state.token, "");
  } finally {
    f.restore();
  }
});

Deno.test("built native cleanup: prolonged reader lock rejects boundedly and remains retryable", async () => {
  const f = await fixture({ removeFailures: 1000 });
  try {
    await f.adapter.publishTokenFile("SYNTHETIC");
    await assertRejects(() => f.adapter.cleanupTokenFile());
    assertEquals(f.calls.filter((c) => c === "native:remove").length, 81);
    assertEquals(f.state.token, "SYNTHETIC");
    f.state.removeFailures = 0;
    await f.adapter.cleanupTokenFile();
    assertEquals(f.state.token, "");
  } finally {
    f.restore();
  }
});
