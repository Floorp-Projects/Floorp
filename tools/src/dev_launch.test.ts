// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects } from "@std/assert";
import { nativeDevLogTarget, startDevBrowser } from "./dev_launch.ts";
import { withDevStateLock } from "./dev_state_lock.ts";

Deno.test("launch startup retains the checkout lock through ready servers and registered browser, not browser lifetime", async () => {
  const root = await Deno.makeTempDir();
  const path = `${root}/lock`;
  const servers = Promise.withResolvers<void>();
  const browser = Promise.withResolvers<void>();
  const entered = Promise.withResolvers<void>();
  let started: (() => void) | undefined;
  let exited = false;
  const values = new Map<string, string>();
  const environment = {
    get: (key: string) => values.get(key),
    set: (key: string, value: string) => {
      values.set(key, value);
    },
    delete: (key: string) => {
      values.delete(key);
    },
  };
  const locked = async () => {
    const file = await Deno.open(path, { read: true, write: true });
    try {
      assertEquals(await file.tryLock(true), false);
    } finally {
      file.close();
    }
  };
  try {
    const launch = withDevStateLock(
      () =>
        startDevBrowser(
          async () => {
            await locked();
            entered.resolve();
            await servers.promise;
          },
          (onStarted) => {
            started = onStarted;
            return browser.promise;
          },
          () => {},
          () => {
            exited = true;
          },
          () => {},
        ),
      "launcher",
      path,
      environment,
    );
    await entered.promise;
    await locked();
    servers.resolve();
    while (!started) await new Promise((resolve) => setTimeout(resolve, 1));
    await locked();
    started();
    await launch;
    const file = await Deno.open(path, { read: true, write: true });
    try {
      assertEquals(await file.tryLock(true), true);
    } finally {
      file.close();
    }
    assertEquals(exited, false);
    browser.resolve();
    await Promise.resolve();
    assertEquals(exited, true);
  } finally {
    servers.resolve();
    browser.resolve();
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("launch failures before and after spawn shut servers down and reject startup", async () => {
  for (const phase of ["servers", "spawn", "empty"]) {
    let stopped = 0;
    await assertRejects(() =>
      startDevBrowser(
        () =>
          phase === "servers"
            ? Promise.reject(new Error("server failed"))
            : Promise.resolve(),
        () =>
          phase === "spawn"
            ? Promise.reject(new Error("spawn failed"))
            : Promise.resolve(),
        () => {
          stopped++;
        },
        () => {},
        () => {},
      )
    );
    assertEquals(stopped, 1);
  }
});

Deno.test("all checkout launch modes await startup inside the lock", async () => {
  const source = await Deno.readTextFile(
    new URL("../feles-build.ts", import.meta.url),
  );
  for (const mode of ["Dev", "Test", "Stage"]) {
    assertEquals(
      source.replace(/\s/g, "").includes(
        `awaitwithDevStateLock(async()=>run${mode}(`,
      ),
      true,
    );
  }
  assertEquals((source.match(/await startDevBrowser\(/g) ?? []).length, 3);
});

Deno.test("native detached browser output goes to a null sink rather than the shared log", async () => {
  assertEquals(nativeDevLogTarget("windows"), "NUL");
  assertEquals(nativeDevLogTarget("linux"), "/dev/null");
  const sentinel = "NATIVE_LOG_SECRET_SENTINEL";
  await Deno.writeTextFile(nativeDevLogTarget(Deno.build.os), sentinel);
  assertEquals(
    (await Deno.readTextFile(nativeDevLogTarget(Deno.build.os))).includes(
      sentinel,
    ),
    false,
  );
  const source = await Deno.readTextFile(
    new URL("../dev-tool.ts", import.meta.url),
  );
  assertEquals(
    source.includes('Deno.env.get("FLOORP_M3E_LAUNCH_TOKEN")'),
    true,
  );
  assertEquals(source.includes("? nativeDevLogTarget(PLATFORM)"), true);
});
