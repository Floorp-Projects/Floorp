// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects } from "@std/assert";
import {
  createDevServerLog,
  DEV_STARTUP_FINALIZE_MS,
  DEV_STARTUP_TIMEOUT_MS,
  DevStartupFailure,
  nativeDevLogTarget,
  pollDevStartup,
  probeDevMarionettePort,
  safeDevStartupFailure,
  startDevBrowser,
  waitForDevMarionette,
} from "./dev_launch.ts";
import { withDevStateLock } from "./dev_state_lock.ts";
import { devProcessExitConfirmed } from "./dev_processes.ts";

Deno.test("a listener cannot report ready before NewSession and DeleteSession; errors and cancelled late connections are closed", async () => {
  const controller = new AbortController();
  const connected = Promise.withResolvers<
    { sendWithTimeout: () => Promise<void>; abort: () => void }
  >();
  const deleted = Promise.withResolvers<void>();
  let closed = 0;
  let ready = false;
  const probe = waitForDevMarionette(
    2828,
    controller.signal,
    () => 90000,
    (port, budget, signal) => {
      assertEquals([port, budget, signal], [2828, 90000, controller.signal]);
      return connected.promise;
    },
  ).then(() => {
    ready = true;
  });
  await Promise.resolve();
  assertEquals(ready, false);
  connected.resolve({
    sendWithTimeout: () => deleted.promise,
    abort: () => {
      closed++;
    },
  });
  await Promise.resolve();
  assertEquals(ready, false);
  deleted.resolve();
  await probe;
  assertEquals([ready, closed], [true, 1]);
  for (const fails of [false, true]) {
    const abort = new AbortController();
    await assertRejects(
      () =>
        waitForDevMarionette(2828, abort.signal, () => 100, () => {
          if (fails) return Promise.reject(new Error("connection rejected"));
          abort.abort(new Error("cancelled"));
          return Promise.resolve({
            sendWithTimeout: () => Promise.resolve(),
            abort: () => {
              closed++;
            },
          });
        }),
      Error,
      fails ? "connection rejected" : "cancelled",
    );
  }
  assertEquals(closed, 2);
  await assertRejects(
    () =>
      waitForDevMarionette(
        2828,
        controller.signal,
        () => 100,
        () =>
          Promise.resolve({
            sendWithTimeout: () => Promise.reject(new Error("delete rejected")),
            abort: () => {
              closed++;
            },
          }),
      ),
    Error,
    "delete rejected",
  );
  assertEquals(closed, 3);
  const cli = await Deno.readTextFile(
    new URL("../dev-tool.ts", import.meta.url),
  );
  assertEquals(
    cli.includes(
      "probeDevMarionettePort(",
    ),
    true,
  );
});

Deno.test("fresh and existing-instance starts share a readiness gate: empty, invalid, missing and timed-out ports never succeed on rerun", async () => {
  const controller = new AbortController();
  let usable = false;
  let probes = 0;
  let registrations = 0;
  const probe = () => {
    probes++;
    return usable
      ? Promise.resolve()
      : Promise.reject(new DevStartupFailure("startup-timeout"));
  };
  const register = (port: number, signal: AbortSignal, budget: number) => {
    assertEquals([port, signal, budget], [2828, controller.signal, 1000]);
    registrations++;
    return Promise.resolve();
  };
  const check = () =>
    probeDevMarionettePort(
      () => "2828",
      controller.signal,
      () => 1000,
      register,
      probe,
    );
  await assertRejects(check, DevStartupFailure, "startup-timeout");
  await assertRejects(check, DevStartupFailure, "startup-timeout");
  assertEquals([registrations, probes], [2, 2]);
  usable = true;
  assertEquals(await check(), "2828");
  assertEquals([registrations, probes], [3, 3]);
  assertEquals(
    await probeDevMarionettePort(
      () => " ",
      controller.signal,
      () => 1000,
      register,
      probe,
    ),
    "",
  );
  for (const text of ["abc", "0", "65536", "-1", "1.5", "0x1234"]) {
    await assertRejects(
      () =>
        probeDevMarionettePort(
          () => text,
          controller.signal,
          () => 1000,
          register,
          probe,
        ),
      Error,
      "Invalid Marionette readiness port",
    );
  }
  await assertRejects(() =>
    probeDevMarionettePort(
      () => {
        throw new Deno.errors.NotFound("missing port file");
      },
      controller.signal,
      () => 1000,
      register,
      probe,
    ), Deno.errors.NotFound);
  assertEquals([registrations, probes], [3, 3]);
  await assertRejects(
    () =>
      probeDevMarionettePort(
        () => "2828",
        controller.signal,
        () => 1000,
        () => Promise.reject(new Error("registration failed")),
        probe,
      ),
    Error,
    "registration failed",
  );
  assertEquals(probes, 3);
  const cancelled = new AbortController();
  await assertRejects(
    () =>
      probeDevMarionettePort(() => "2828", cancelled.signal, () => 1000, () => {
        cancelled.abort(new Error("cancelled registration"));
        return Promise.resolve();
      }, probe),
    Error,
    "cancelled registration",
  );
  assertEquals(probes, 3);
  const source =
    (await Deno.readTextFile(new URL("../dev-tool.ts", import.meta.url)))
      .replace(/\s/g, "");
  const existing = source.slice(
    source.indexOf("if(existingPid!==null)"),
    source.indexOf('console.log("Startingfeles-builddev..."'),
  );
  assertEquals(
    existing.indexOf("awaitpollDevStartup(") <
      existing.indexOf("Devserveralreadyrunning"),
    true,
  );
  assertEquals(
    existing.includes("!!awaitcheckReadiness(signal,remaining)"),
    true,
  );
  assertEquals(
    existing.includes("awaitDeno.remove(DEV_STARTUP_STATUS_FILE)"),
    true,
  );
  assertEquals(existing.includes("reportDevStartupFailure(error)"), true);
  assertEquals(existing.includes("throwerror;"), true);
  assertEquals(
    source.includes("constport=awaitcheckReadiness(signal,remaining)"),
    true,
  );
  assertEquals(
    source.includes("recordedProcesses:readDevProcesses(),signal,timeoutMs,"),
    true,
  );
});

Deno.test("startup polling charges repeated inventory timeouts to its monotonic deadline and preserves reporting time", async () => {
  let clock = 137;
  const deadline = DEV_STARTUP_TIMEOUT_MS - DEV_STARTUP_FINALIZE_MS;
  const budgets: number[] = [];
  await assertRejects(
    () =>
      pollDevStartup(
        async (signal, remaining, attempt) => {
          if (attempt % 5 === 4) {
            assertEquals(
              await devProcessExitConfirmed(
                81,
                (pid, inventorySignal, timeoutMs) => {
                  assertEquals(pid, 81);
                  assertEquals(inventorySignal, signal);
                  const budget = timeoutMs ?? 0;
                  assertEquals(budget > 0, true);
                  budgets.push(budget);
                  clock += budget;
                  return Promise.reject(
                    new Error("Process inventory timed out"),
                  );
                },
                signal,
                Math.min(5000, remaining()),
              ),
              false,
            );
          }
          return false;
        },
        deadline,
        () => clock,
        (ms) => {
          clock += ms;
          return Promise.resolve();
        },
      ),
    DevStartupFailure,
    "startup-timeout",
  );
  assertEquals(clock, deadline);
  assertEquals(budgets.length, 11);
  assertEquals(budgets.at(-1), 4863);
  assertEquals(clock + 1000 + 5000 < DEV_STARTUP_TIMEOUT_MS, true);
});

Deno.test("startup polling cancels a stalled check; success, errors and exhausted budgets leave no timers", async () => {
  let signal: AbortSignal | undefined;
  await assertRejects(
    () =>
      pollDevStartup(
        (current) => {
          signal = current;
          return new Promise((resolve) =>
            current.addEventListener("abort", () => resolve(false), {
              once: true,
            })
          );
        },
        performance.now() + 20,
        undefined,
        () => Promise.resolve(),
      ),
    DevStartupFailure,
    "startup-timeout",
  );
  assertEquals(signal?.aborted, true);
  let calls = 0;
  await pollDevStartup(
    () => {
      calls++;
      return Promise.resolve(true);
    },
    performance.now() + 1000,
    undefined,
    () => Promise.resolve(),
  );
  assertEquals(calls, 1);
  await assertRejects(
    () =>
      pollDevStartup(() => {
        calls++;
        return Promise.resolve(true);
      }, performance.now() + 20),
    DevStartupFailure,
    "startup-timeout",
  );
  assertEquals(calls, 1);
  await assertRejects(
    () =>
      pollDevStartup(
        () => Promise.reject(new Error("probe failed")),
        performance.now() + 1000,
        undefined,
        () => Promise.resolve(),
      ),
    Error,
    "probe failed",
  );
  await assertRejects(
    () =>
      pollDevStartup(() => {
        calls++;
        return Promise.resolve(true);
      }, performance.now() - 1),
    DevStartupFailure,
    "startup-timeout",
  );
  assertEquals(calls, 1);
  await assertRejects(
    () => pollDevStartup(() => Promise.resolve(false), NaN),
    TypeError,
    "Invalid startup deadline",
  );
  const source =
    (await Deno.readTextFile(new URL("../dev-tool.ts", import.meta.url)))
      .replace(/\s/g, "");
  assertEquals(
    source.includes(
      "performance.now()+DEV_STARTUP_TIMEOUT_MS-DEV_STARTUP_FINALIZE_MS",
    ),
    true,
  );
  assertEquals(
    source.includes("settleWithin(reportDevStartupFailure(error),5000)"),
    true,
  );
});

Deno.test("a polling callback ignoring cancellation cannot hold startup past its bounded drain", async () => {
  let timer: ReturnType<typeof setTimeout> | undefined;
  try {
    await assertRejects(
      () =>
        Promise.race([
          pollDevStartup(
            () => new Promise<boolean>(() => {}),
            performance.now() + 20,
            undefined,
            () => Promise.resolve(),
          ),
          new Promise<never>((_, reject) => {
            timer = setTimeout(
              () => reject(new Error("polling held the deadline")),
              3000,
            );
          }),
        ]),
      DevStartupFailure,
      "startup-timeout",
    );
  } finally {
    clearTimeout(timer);
  }
});

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

Deno.test("startup deadline aborts server setup or browser startup and awaits cancellation before returning", async () => {
  for (const phase of ["servers", "browser"]) {
    let cancelled = false;
    let cleaned = false;
    let stopped = false;
    const stalled = (signal: AbortSignal) =>
      new Promise<void>((resolve) => {
        signal.addEventListener("abort", () => {
          cancelled = true;
          queueMicrotask(() => {
            cleaned = true;
            resolve();
          });
        }, { once: true });
      });
    await assertRejects(
      () =>
        startDevBrowser(
          (signal) => phase === "servers" ? stalled(signal) : Promise.resolve(),
          (_ready, signal) => stalled(signal),
          () => {
            stopped = true;
          },
          () => {},
          () => {},
          20,
        ),
      DevStartupFailure,
      "startup-timeout",
    );
    assertEquals({ cancelled, cleaned, stopped }, {
      cancelled: true,
      cleaned: true,
      stopped: true,
    });
  }
});

Deno.test("native Vite artifacts omit arbitrary output but retain split-chunk port conflicts and sanitized startup codes", async () => {
  const root = await Deno.makeTempDir();
  try {
    const log = await createDevServerLog(
      `${root}/vite-main.log`,
      true,
      "main",
      5173,
      81,
    );
    await log.write(new TextEncoder().encode("SECRET_SENTINEL EADDR"));
    await log.write(new TextEncoder().encode("INUSE SECRET_SENTINEL"));
    assertEquals(log.portConflict, true);
    await log.finish("failed", 1);
    const text = await Deno.readTextFile(`${root}/vite-main.log`);
    assertEquals(text.includes("SECRET_SENTINEL"), false);
    assertEquals(JSON.parse(text).portConflict, true);
    assertEquals(
      safeDevStartupFailure(new DevStartupFailure("port-conflict", 5173)),
      { state: "failed", code: "port-conflict", port: 5173 },
    );
    assertEquals(safeDevStartupFailure(new Error("SECRET_SENTINEL")), {
      state: "failed",
      code: "startup-failed",
      port: null,
    });
    const normal = await createDevServerLog(
      `${root}/ordinary.log`,
      false,
      "main",
      5173,
      81,
    );
    await normal.write(new TextEncoder().encode("ordinary developer output"));
    assertEquals(
      await Deno.readTextFile(`${root}/ordinary.log`),
      "ordinary developer output",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("startup failure releases the lock within its cleanup budget even if registration, shutdown or reporting never settle", async () => {
  const root = await Deno.makeTempDir();
  const environment = { get: () => undefined, set() {}, delete() {} };
  let timer: ReturnType<typeof setTimeout> | undefined;
  try {
    for (const reporting of ["reject", "stall"]) {
      const registration = Promise.withResolvers<void>();
      let lateReady = () => {};
      let exited = false;
      let reported = false;
      await assertRejects(
        () =>
          Promise.race([
            withDevStateLock(
              () =>
                startDevBrowser(
                  () => Promise.resolve(),
                  (ready) => {
                    lateReady = ready;
                    return registration.promise;
                  },
                  () => new Promise<void>(() => {}),
                  () => {
                    exited = true;
                  },
                  () => {
                    exited = true;
                  },
                  20,
                  () => {
                    reported = true;
                    return reporting === "reject"
                      ? Promise.reject(new Error("diagnostic write failed"))
                      : new Promise<void>(() => {});
                  },
                  20,
                ),
              "startup",
              `${root}/lock`,
              environment,
            ),
            new Promise<never>((_, reject) => {
              timer = setTimeout(
                () => reject(new Error("unbounded cleanup")),
                1000,
              );
            }),
          ]),
        DevStartupFailure,
        "startup-timeout",
      );
      clearTimeout(timer);
      await withDevStateLock(
        () => Promise.resolve(),
        "recovery",
        `${root}/lock`,
        environment,
      );
      lateReady();
      registration.resolve();
      await Promise.resolve();
      assertEquals({ reported, exited }, { reported: true, exited: false });
    }
  } finally {
    clearTimeout(timer);
    await Deno.remove(root, { recursive: true });
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
