// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertStringIncludes } from "@std/assert";
import { tokenFileScope } from "../../libs/shared/os-server-token/defines.ts";
import { TokenFileLifecycle } from "../../libs/shared/os-server-token/lifecycle.ts";
import type {
  TokenDestination,
  TokenStorage,
} from "../../libs/shared/os-server-token/types.ts";

const a: TokenDestination = {
  directory: "profile-a/floorp-os-server",
  path: "profile-a/floorp-os-server/os-server-token",
  profileScoped: true,
};
const b: TokenDestination = {
  ...a,
  directory: "profile-b/floorp-os-server",
  path: "profile-b/floorp-os-server/os-server-token",
};
function fixture() {
  const files = new Map<string, string>();
  const calls: string[] = [];
  const storage: TokenStorage = {
    prepare(d) {
      calls.push(`prepare:${d.path}`);
      return Promise.resolve();
    },
    write(d, token) {
      calls.push(`write:${d.path}`);
      files.set(d.path, token);
      return Promise.resolve();
    },
    secure(d) {
      calls.push(`secure:${d.path}`);
      return Promise.resolve();
    },
    remove(d) {
      calls.push(`remove:${d.path}`);
      files.delete(d.path);
      return Promise.resolve();
    },
  };
  return { files, calls, storage, lifecycle: new TokenFileLifecycle(storage) };
}

Deno.test("token scope: default compatible; explicit isolation fails closed outside dev/test", () => {
  for (const mode of ["", "production", "stage", "dev", "test"]) {
    assertEquals(tokenFileScope(false, mode), "legacy");
    assertEquals(
      tokenFileScope(true, mode),
      mode === "dev" || mode === "test" ? "profile" : null,
    );
  }
});

Deno.test("cleanup never touches an unowned destination", async () => {
  const f = fixture();
  f.files.set(a.path, "foreign");
  await f.lifecycle.cleanup();
  assertEquals(f.calls, []);
  assertEquals(f.files.get(a.path), "foreign");
});

Deno.test("queued publish followed by cleanup never resurrects token", async () => {
  const f = fixture();
  await Promise.all([f.lifecycle.publish(a, "fake"), f.lifecycle.cleanup()]);
  assertEquals(f.files.size, 0);
  assertEquals(f.calls, []);
});

Deno.test("in-flight publish/delete/restart serializes and keeps latest instance", async () => {
  const f = fixture();
  const entered = Promise.withResolvers<void>();
  const release = Promise.withResolvers<void>();
  f.storage.write = async (d, token) => {
    entered.resolve();
    await release.promise;
    f.files.set(d.path, token);
  };
  const first = f.lifecycle.publish(a, "old-fake");
  await entered.promise;
  const deletion = f.lifecycle.cleanup();
  const restart = f.lifecycle.publish(b, "new-fake");
  release.resolve();
  await Promise.all([first, deletion, restart]);
  assertEquals([...f.files], [[b.path, "new-fake"]]);
  await f.lifecycle.cleanup();
  assertEquals(f.files.size, 0);
});

Deno.test("two profile owners never delete each other or legacy", async () => {
  const f = fixture();
  const second = new TokenFileLifecycle(f.storage);
  f.files.set("Home/.floorp/os-server-token", "foreign-legacy");
  await f.lifecycle.publish(a, "a-fake");
  await second.publish(b, "b-fake");
  await f.lifecycle.cleanup();
  assertEquals([...f.files], [[
    "Home/.floorp/os-server-token",
    "foreign-legacy",
  ], [b.path, "b-fake"]]);
});

Deno.test("destination is snapshotted before caller mutation", async () => {
  const f = fixture();
  const target = { ...a };
  const publication = f.lifecycle.publish(target, "fake");
  target.path = b.path;
  await publication;
  assertEquals([...f.files], [[a.path, "fake"]]);
  await f.lifecycle.cleanup();
  assertEquals(f.files.size, 0);
});

Deno.test("permission/write failure removes partial token and queue recovers", async () => {
  for (const failing of ["write", "secure"] as const) {
    const f = fixture();
    const originalWrite = f.storage.write;
    const originalSecure = f.storage.secure;
    f.storage[failing] = () => {
      f.files.set(a.path, "partial-fake");
      return Promise.reject(new Error("synthetic-secret-not-for-log"));
    };
    await assertRejects(() => f.lifecycle.publish(a, "fake"));
    assertEquals(f.files.size, 0);
    f.storage.write = originalWrite;
    f.storage.secure = originalSecure;
    await f.lifecycle.publish(b, "recovered-fake");
    assertEquals(f.files.get(b.path), "recovered-fake");
  }
});

Deno.test("prepare failure never writes; destination is captured; rejected mode withdraws owned token", async () => {
  const f = fixture();
  f.storage.prepare = () => Promise.reject(new Error("permission denied"));
  await assertRejects(() => f.lifecycle.publish(a, "fake"));
  assertEquals(f.calls, []);
  f.storage.prepare = () => Promise.resolve();
  await f.lifecycle.publish(a, "fake");
  await f.lifecycle.publish(null, "fake");
  assertEquals(f.files.size, 0);
  assertEquals(f.calls.includes(`remove:${a.path}`), true);
});

Deno.test("Firefox wiring shares canonical adapter, publishes after bind, reset preserves live MCP", async () => {
  const root = "../../browser-features/modules/modules/";
  const read = (p: string) =>
    Deno.readTextFile(new URL(root + p, import.meta.url));
  const server = await read("os-server/server.sys.mts");
  const manager = await read("os-automotor/OSAutomotor-manager.sys.mts");
  const adapter = await read("os-server/token-file.sys.mts");
  const canonical = "resource://noraneko/modules/os-server/token-file.sys.mjs";
  assertStringIncludes(server, canonical);
  assertStringIncludes(manager, canonical);
  assertEquals(
    manager.includes("const tokenFile = PathUtils.join(homeDir"),
    false,
  );
  assertStringIncludes(
    manager,
    "!preserveMcpToken && Services.prefs.prefHasUserValue",
  );
  assertStringIncludes(manager, "if (!preserveMcpToken)");
  assertStringIncludes(
    server,
    'Services.prefs.addObserver("nora.startup.mode", this)',
  );
  assertEquals(
    server.indexOf("server.asyncListen(this)") <
      server.indexOf("this.writeTokenFile(this._token)"),
    true,
  );
  const ensure = server.slice(
    server.indexOf("public ensureToken()"),
    server.indexOf("private writeTokenFile"),
  );
  assertEquals(ensure.includes("writeTokenFile("), false);
  assertEquals(adapter.includes("server.sys.mts"), false);
  assertStringIncludes(adapter, "permissions: 0o700");
  assertStringIncludes(adapter, "IOUtils.setPermissions(target.path, 0o600)");
});
