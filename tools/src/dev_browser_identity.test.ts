// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert";
import * as path from "@std/path";
import {
  captureWindowsBrowserListenerIdentity,
  listWindowsBrowserListeners,
  listWindowsBrowserProcesses,
  ownsDevBrowserListener,
  parseDevBrowserLoopbackLsofOwners,
  parseWindowsBrowserProcessSnapshot,
  revalidateWindowsBrowserListenerIdentity,
  selectWindowsBrowserListenerIdentity,
} from "./dev_browser_identity.ts";
import type {
  DevBrowserListenerRecord,
  DevBrowserProcessRecord,
  WindowsBrowserIdentityDeps,
  WindowsBrowserListenerSelection,
} from "./dev_browser_identity_types.ts";
import {
  readDevProcesses,
  recordDevProcess,
  registerWindowsBrowserListenerIdentity,
} from "./dev_processes.ts";
import { sleep } from "./async_utils.ts";

const BIN = "C:\\Floorp\\_dist\\bin\\floorp\\floorp.exe";

Deno.test({
  name:
    "Linux ownership ignores unrelated IPv6-only and different-address listeners, rejects replacement and tolerates an absent IPv6 table",
  ignore: Deno.build.os !== "linux",
  fn: async () => {
    const fixture = async () => {
      const listener = Deno.listen({ hostname: "127.0.0.1", port: 0 });
      const port = listener.addr.port;
      let listening = true;
      let child: Deno.ChildProcess | undefined;
      let reader: ReadableStreamDefaultReader<Uint8Array> | undefined;
      const signal = new AbortController().signal;
      try {
        assertEquals(
          await ownsDevBrowserListener(port, [Deno.pid], signal, 5000),
          true,
        );
        assertEquals(
          await ownsDevBrowserListener(port, [], signal, 5000),
          false,
        );
        const read = Deno.readTextFile;
        try {
          const table = await read("/proc/net/tcp");
          const local = `0100007F:${
            port.toString(16).toUpperCase().padStart(4, "0")
          }`;
          const row = table.split("\n").find((line) =>
            line.trim().split(/\s+/)[1] === local
          )!;
          for (
            const [address, expected] of [
              ["00000000000000000000000000000000", true],
              ["0000000000000000FFFF00000100007F", true],
              ["00000000000000000000000001000000", false],
              ["0000000000000000FFFF00000200007F", false],
            ] as const
          ) {
            Deno.readTextFile = (path, options) =>
              path === "/proc/net/tcp"
                ? Promise.resolve("header\n")
                : path === "/proc/net/tcp6"
                ? Promise.resolve(
                  `header\n${
                    row.replace(local, `${address}:${local.split(":")[1]}`)
                  }\n`,
                )
                : read(path, options);
            assertEquals(
              await ownsDevBrowserListener(port, [Deno.pid], signal, 5000),
              expected,
            );
          }
          for (
            const error of [
              new Deno.errors.NotFound("IPv6 disabled"),
              new Deno.errors.PermissionDenied("unreadable table"),
              new Error("table I/O failure"),
            ]
          ) {
            Deno.readTextFile = (path, options) =>
              path === "/proc/net/tcp6"
                ? Promise.reject(error)
                : read(path, options);
            if (error instanceof Deno.errors.NotFound) {
              assertEquals(
                await ownsDevBrowserListener(port, [Deno.pid], signal, 5000),
                true,
              );
            } else {
              await assertRejects(
                () => ownsDevBrowserListener(port, [Deno.pid], signal, 5000),
                Error,
                error.message,
              );
            }
          }
          Deno.readTextFile = (path, options) =>
            path === "/proc/net/tcp"
              ? Promise.reject(new Deno.errors.NotFound("missing IPv4 table"))
              : read(path, options);
          await assertRejects(
            () => ownsDevBrowserListener(port, [Deno.pid], signal, 5000),
            Deno.errors.NotFound,
            "missing IPv4 table",
          );
        } finally {
          Deno.readTextFile = read;
        }
        const wildcard = Deno.listen({ hostname: "0.0.0.0", port: 0 });
        try {
          assertEquals(
            await ownsDevBrowserListener(
              wildcard.addr.port,
              [Deno.pid],
              signal,
              5000,
            ),
            true,
          );
        } finally {
          wildcard.close();
        }
        for (const hostname of ["::", "127.0.0.2", "127.0.0.1"]) {
          if (hostname === "::") {
            try {
              await Deno.stat("/proc/net/tcp6");
            } catch (error) {
              if (error instanceof Deno.errors.NotFound) continue;
              throw error;
            }
          }
          if (hostname === "127.0.0.1") {
            listener.close();
            listening = false;
          }
          child = new Deno.Command(
            hostname === "::" ? "python3" : Deno.execPath(),
            {
              args: hostname === "::"
                ? [
                  "-u",
                  "-c",
                  `import socket, sys; s = socket.socket(socket.AF_INET6, socket.SOCK_STREAM); s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1); s.bind(("::", ${port})); s.listen(1); assert s.getsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY) == 1; print("ready"); sys.stdin.buffer.read(1); s.close()`,
                ]
                : [
                  "eval",
                  `const listener = Deno.listen({hostname: ${
                    JSON.stringify(hostname)
                  }, port: ${port}}); console.log("ready"); await Deno.stdin.read(new Uint8Array(1)); listener.close();`,
                ],
              stdin: "piped",
              stdout: "piped",
              stderr: "null",
            },
          ).spawn();
          reader = child.stdout.getReader();
          const ready = await reader.read();
          assertEquals(new TextDecoder().decode(ready.value).trim(), "ready");
          assertEquals(
            await ownsDevBrowserListener(port, [Deno.pid], signal, 5000),
            hostname !== "127.0.0.1",
          );
          if (hostname !== "::") {
            assertEquals(
              await ownsDevBrowserListener(port, [child.pid], signal, 5000),
              hostname === "127.0.0.1",
            );
          }
          const cancelled = new AbortController();
          cancelled.abort(new Error("cancelled listener inspection"));
          await assertRejects(
            () =>
              ownsDevBrowserListener(
                port,
                [child!.pid],
                cancelled.signal,
                5000,
              ),
            Error,
            "cancelled listener inspection",
          );
          await child.stdin.close();
          await child.status;
          await reader.cancel();
          reader.releaseLock();
          child = undefined;
          reader = undefined;
        }
      } finally {
        if (listening) listener.close();
        if (child) {
          await child.stdin.close().catch(() => {});
          await child.status;
        }
        if (reader) {
          await reader.cancel();
          reader.releaseLock();
        }
      }
    };
    const source =
      `import { assertEquals, assertRejects } from "@std/assert"; import { ownsDevBrowserListener } from ${
        JSON.stringify(
          new URL("./dev_browser_identity.ts", import.meta.url).href,
        )
      }; await (${fixture.toString()})();`;
    const result = await new Deno.Command(Deno.execPath(), {
      args: [
        "eval",
        "--config",
        new URL("../../deno.json", import.meta.url).pathname,
        source,
      ],
      stdout: "piped",
      stderr: "piped",
    }).output();
    assertEquals(result.success, true, new TextDecoder().decode(result.stderr));
  },
});

Deno.test("lsof ownership selects only endpoints serving IPv4 loopback, including wildcards and mapped addresses", () => {
  for (
    const address of [
      "127.0.0.1",
      "0.0.0.0",
      "*",
      "[::]",
      "[::ffff:127.0.0.1]",
      "[::ffff:7f00:1]",
    ]
  ) {
    assertEquals(
      parseDevBrowserLoopbackLsofOwners(
        `p11\nf3\nn${address}:2828\np12\nf4\nn127.0.0.2:2828\nn[::1]:2828\nn127.0.0.1:2829\n`,
        2828,
      ),
      [11],
    );
  }
  assertEquals(parseDevBrowserLoopbackLsofOwners("", 2828), []);
  assertEquals(
    parseDevBrowserLoopbackLsofOwners("p12\nn127.0.0.2:2828\n", 2828),
    [],
  );
  assertEquals(
    parseDevBrowserLoopbackLsofOwners(
      "p11\nn*:2828\np12\nn127.0.0.1:2828\n",
      2828,
    ),
    [11, 12],
  );
  assertThrows(
    () => parseDevBrowserLoopbackLsofOwners("n127.0.0.1:2828\n", 2828),
    Error,
    "no process owner",
  );
});
const PROFILE = "C:\\Floorp\\_dist\\profile\\test";
const PARENT_START = "2026-11-15T03:00:00.000Z";
const CHILD_START = "2026-11-15T03:00:01.000Z";
const PARENT_COMMAND = "cmd.exe /d /s /c floorp.exe";

const parent: DevBrowserProcessRecord = {
  pid: 44524,
  ppid: 100,
  name: "cmd.exe",
  executablePath: "C:\\Windows\\System32\\cmd.exe",
  commandLine: PARENT_COMMAND,
  startTime: PARENT_START,
};
const child: DevBrowserProcessRecord = {
  pid: 10896,
  ppid: 44524,
  name: "floorp.exe",
  executablePath: BIN,
  commandLine: `${BIN} --profile ${PROFILE} --marionette`,
  startTime: CHILD_START,
};
const listener: DevBrowserListenerRecord = {
  localPort: 5200,
  state: "Listen",
  owningProcess: 10896,
};
const recordedParent = {
  pid: 44524,
  startTime: PARENT_START,
  commandLine: PARENT_COMMAND,
};

function selection(
  overrides: Partial<WindowsBrowserListenerSelection> = {},
): WindowsBrowserListenerSelection {
  return {
    port: 5200,
    expectedBinaryPath: BIN,
    expectedProfilePath: PROFILE,
    recordedProcesses: [recordedParent],
    listeners: [listener],
    processes: [parent, child],
    ...overrides,
  };
}

function deps(
  overrides: Partial<WindowsBrowserIdentityDeps> = {},
): WindowsBrowserIdentityDeps {
  return {
    listListeners() {
      return Promise.resolve().then(() => {
        return [listener];
      });
    },
    listProcesses() {
      return Promise.resolve().then(() => {
        return [parent, child];
      });
    },
    recordIdentity() {
      return Promise.resolve().then(() => {
        return true;
      });
    },
    sleep() {
      return Promise.resolve().then(() => {});
    },
    ...overrides,
  };
}

Deno.test("Windows listener registration cancels, kills and reaps both PowerShell snapshots before returning, without retries or persistence", async () => {
  const descriptor = Object.getOwnPropertyDescriptor(Deno, "Command")!;
  let spawned = 0;
  let killed = 0;
  let reaped = 0;
  let stall = true;
  let fail = false;
  const started = Promise.withResolvers<void>();
  class MockCommand {
    constructor(command: string) {
      assertEquals(command, "powershell");
    }
    spawn() {
      spawned++;
      if (spawned === 2) started.resolve();
      const status = Promise.withResolvers<Deno.CommandStatus>();
      const output = Promise.withResolvers<Deno.CommandOutput>();
      const result = {
        success: !fail,
        code: fail ? 1 : 0,
        signal: null,
        stdout: new TextEncoder().encode("[]"),
        stderr: new TextEncoder().encode("probe failed"),
      };
      if (!stall) {
        status.resolve(result);
        output.resolve(result);
      }
      return {
        status: status.promise.then((value) => {
          reaped++;
          return value;
        }),
        stdout: new ReadableStream({
          async start(controller) {
            controller.enqueue((await output.promise).stdout);
            controller.close();
          },
        }),
        stderr: new ReadableStream({
          async start(controller) {
            controller.enqueue((await output.promise).stderr);
            controller.close();
          },
        }),
        kill() {
          killed++;
          output.resolve(result);
          setTimeout(() => status.resolve(result), spawned % 2 === 0 ? 10 : 1);
        },
      };
    }
  }
  try {
    Object.defineProperty(Deno, "Command", {
      configurable: true,
      value: MockCommand,
    });
    const target = {
      port: 5200,
      expectedBinaryPath: BIN,
      expectedProfilePath: PROFILE,
      recordedProcesses: [recordedParent],
      timeoutMs: 1000,
    };
    const controller = new AbortController();
    const pending = registerWindowsBrowserListenerIdentity({
      ...target,
      signal: controller.signal,
    });
    await started.promise;
    controller.abort(new Error("cancelled startup"));
    await assertRejects(() => pending, Error, "cancelled startup");
    assertEquals([spawned, killed, reaped], [2, 2, 2]);
    await assertRejects(
      () =>
        registerWindowsBrowserListenerIdentity({ ...target, timeoutMs: 20 }),
      Error,
      "Listener registration timed out",
    );
    assertEquals([spawned, killed, reaped], [4, 4, 4]);
    await assertRejects(
      () =>
        registerWindowsBrowserListenerIdentity({
          ...target,
          signal: controller.signal,
        }),
      Error,
      "cancelled startup",
    );
    for (const timeoutMs of [0, -1, NaN, Infinity]) {
      await assertRejects(
        () => registerWindowsBrowserListenerIdentity({ ...target, timeoutMs }),
        TypeError,
        "Invalid listener registration timeout",
      );
    }
    assertEquals(spawned, 4);
    await assertRejects(
      () => listWindowsBrowserListeners(5200, undefined, 20),
      Error,
      "Listener inspection timed out",
    );
    await assertRejects(
      () => listWindowsBrowserProcesses(undefined, 20),
      Error,
      "Listener inspection timed out",
    );
    assertEquals([spawned, killed, reaped], [6, 6, 6]);
    stall = false;
    assertEquals(await listWindowsBrowserListeners(5200), []);
    assertEquals(await listWindowsBrowserProcesses(), []);
    fail = true;
    await assertRejects(
      () => listWindowsBrowserListeners(5200),
      Error,
      "powershell: probe failed",
    );
  } finally {
    Object.defineProperty(Deno, "Command", descriptor);
  }
});

Deno.test("listener registration forwards remaining time and cancellation through revalidation, backoff and persistence", async () => {
  const target = {
    port: 5200,
    expectedBinaryPath: BIN,
    expectedProfilePath: PROFILE,
    recordedProcesses: [recordedParent],
    timeoutMs: 1000,
  };
  const controller = new AbortController();
  let inspections = 0;
  let writes = 0;
  await assertRejects(
    () =>
      captureWindowsBrowserListenerIdentity(
        { ...target, signal: controller.signal },
        deps({
          listProcesses(signal, budget) {
            assert(signal);
            assert(budget! > 0 && budget! <= 1000);
            inspections++;
            return Promise.resolve([parent, child]);
          },
          recordIdentity(_identity, signal) {
            assert(signal);
            controller.abort(new Error("cancelled persistence"));
            signal.throwIfAborted();
            writes++;
            return Promise.resolve(true);
          },
        }),
      ),
    Error,
    "cancelled persistence",
  );
  assertEquals([inspections, writes], [2, 0]);
  const retry = new AbortController();
  let attempts = 0;
  await assertRejects(
    () =>
      captureWindowsBrowserListenerIdentity(
        { ...target, signal: retry.signal },
        deps({
          listListeners() {
            attempts++;
            return Promise.reject(new Error("not ready"));
          },
          sleep(ms, signal) {
            retry.abort(new Error("cancelled backoff"));
            return sleep(ms, signal);
          },
        }),
      ),
    Error,
    "cancelled backoff",
  );
  assertEquals(attempts, 1);
  const source =
    (await Deno.readTextFile(new URL("./dev_processes.ts", import.meta.url)))
      .replace(/\s/g, "");
  assert(
    /recordDevProcess\(identity\.pid,DEV_PROCESSES_FILE,identity,signal,?\)/
      .test(source),
  );
});

Deno.test("unverifiable Windows system rows do not poison the owned process snapshot", () => {
  const raw = JSON.stringify([
    {
      pid: 0,
      ppid: 0,
      name: "System Idle Process",
      executablePath: null,
      commandLine: null,
      startTime: "",
    },
    {
      pid: 4,
      ppid: 0,
      name: "System",
      executablePath: null,
      commandLine: null,
      startTime: "",
    },
    parent,
    child,
  ]);
  assertEquals(parseWindowsBrowserProcessSnapshot(raw).map((row) => row.pid), [
    44524,
    10896,
  ]);
});

Deno.test("conflicting profile arguments cannot establish owned browser identity", async () => {
  const conflicting = {
    ...child,
    commandLine: BIN + " --profile C:\\Foreign --profile " + PROFILE,
  };
  await assertRejects(() =>
    Promise.resolve().then(() => (selectWindowsBrowserListenerIdentity(
      selection({ processes: [parent, conflicting] }),
    )))
  );
});

Deno.test("unbounded listener retry budget is refused before inspection", async () => {
  let calls = 0;
  await assertRejects(() =>
    captureWindowsBrowserListenerIdentity(
      {
        port: 5200,
        expectedBinaryPath: BIN,
        expectedProfilePath: PROFILE,
        recordedProcesses: [recordedParent],
        retryCount: Infinity,
      },
      deps({
        listListeners() {
          return Promise.resolve().then(() => {
            calls++;
            return [listener];
          });
        },
      }),
    )
  );
  assertEquals(calls, 0);
});

Deno.test("direct listener child is selected through its recorded live parent", () => {
  const identity = selectWindowsBrowserListenerIdentity(selection());
  assertEquals(identity.pid, 10896);
  assertEquals(identity.startTime, CHILD_START);
});

Deno.test("already registered exact listener generation is accepted on relaunch", () => {
  const identity = selectWindowsBrowserListenerIdentity(selection({
    recordedProcesses: [
      recordedParent,
      {
        pid: 10896,
        startTime: CHILD_START,
        commandLine: child.commandLine ?? "",
      },
    ],
    processes: [child],
  }));
  assertEquals(identity.pid, 10896);
});

Deno.test("foreign profile argument is refused", async () => {
  const foreign = {
    ...child,
    commandLine: `${BIN} --profile C:\\Floorp\\_dist\\profile\\other`,
  };
  await assertRejects(
    () =>
      Promise.resolve().then(() => (selectWindowsBrowserListenerIdentity(
        selection({ processes: [parent, foreign] }),
      ))),
    Error,
    "exact owned profile",
  );
});

Deno.test("foreign browser executable is refused", async () => {
  const foreign = {
    ...child,
    executablePath: "C:\\Floorp\\_dist\\bin\\other\\other.exe",
  };
  await assertRejects(
    () =>
      Promise.resolve().then(() => (selectWindowsBrowserListenerIdentity(
        selection({ processes: [parent, foreign] }),
      ))),
    Error,
    "expected browser executable",
  );
});

Deno.test("reused parent PID with a different generation is refused", async () => {
  const reused = { ...parent, startTime: "2026-11-15T04:00:00.000Z" };
  await assertRejects(
    () =>
      Promise.resolve().then(() => (selectWindowsBrowserListenerIdentity(
        selection({ processes: [reused, child] }),
      ))),
    Error,
    "recorded generation",
  );
});

Deno.test("missing live parent is refused", async () => {
  await assertRejects(
    () =>
      Promise.resolve().then(
        () => (selectWindowsBrowserListenerIdentity(
          selection({ processes: [child] }),
        )),
      ),
    Error,
    "parent 44524 is not live",
  );
});

Deno.test("content process listener owner is refused", async () => {
  const content = {
    ...child,
    commandLine: child.commandLine + " -contentproc",
  };
  await assertRejects(
    () =>
      Promise.resolve().then(() => (selectWindowsBrowserListenerIdentity(
        selection({ processes: [parent, content] }),
      ))),
    Error,
    "content process",
  );
});

Deno.test("ambiguous listener owners are refused", async () => {
  const second = { ...listener, owningProcess: 10897 };
  await assertRejects(
    () =>
      Promise.resolve().then(() => (selectWindowsBrowserListenerIdentity(
        selection({ listeners: [listener, second] }),
      ))),
    Error,
    "exactly one owner",
  );
});

Deno.test("exited listener child is refused", async () => {
  await assertRejects(
    () =>
      Promise.resolve().then(
        () => (selectWindowsBrowserListenerIdentity(
          selection({ processes: [parent] }),
        )),
      ),
    Error,
    "was not found",
  );
});

Deno.test("capture revalidates a fresh snapshot before recording", async () => {
  let processCalls = 0;
  let recorded = 0;
  const changed = { ...child, startTime: "2026-11-15T03:00:02.000Z" };
  const result = await captureWindowsBrowserListenerIdentity(
    {
      port: 5200,
      expectedBinaryPath: BIN,
      expectedProfilePath: PROFILE,
      recordedProcesses: [recordedParent],
      retryCount: 3,
      retryDelayMs: 1,
    },
    deps({
      listProcesses() {
        return Promise.resolve().then(() => {
          processCalls++;
          return processCalls === 2 ? [parent, changed] : [parent, child];
        });
      },
      recordIdentity() {
        return Promise.resolve().then(() => {
          recorded++;
          return true;
        });
      },
    }),
  );
  assertEquals(result.pid, 10896);
  assertEquals(processCalls, 4);
  assertEquals(recorded, 1);
});

Deno.test("capture accepts an already registered exact child without writing", async () => {
  let recorded = 0;
  const result = await captureWindowsBrowserListenerIdentity(
    {
      port: 5200,
      expectedBinaryPath: BIN,
      expectedProfilePath: PROFILE,
      recordedProcesses: [
        recordedParent,
        {
          pid: 10896,
          startTime: CHILD_START,
          commandLine: child.commandLine ?? "",
        },
      ],
      retryCount: 1,
    },
    deps({
      recordIdentity() {
        return Promise.resolve().then(() => {
          recorded++;
          return true;
        });
      },
    }),
  );
  assertEquals(result.pid, 10896);
  assertEquals(recorded, 0);
});

Deno.test("invalid capture has no registry effect", async () => {
  let recorded = 0;
  await assertRejects(
    () =>
      captureWindowsBrowserListenerIdentity(
        {
          port: 5200,
          expectedBinaryPath: BIN,
          expectedProfilePath: PROFILE,
          recordedProcesses: [recordedParent],
          retryCount: 2,
          retryDelayMs: 1,
        },
        deps({
          listListeners() {
            return Promise.resolve().then(() => {
              return [];
            });
          },
          recordIdentity() {
            return Promise.resolve().then(() => {
              recorded++;
              return true;
            });
          },
        }),
      ),
    Error,
    "exactly one owner",
  );
  assertEquals(recorded, 0);
});

Deno.test("revalidation rejects a changed candidate", () => {
  const identity = selectWindowsBrowserListenerIdentity(selection());
  assert(revalidateWindowsBrowserListenerIdentity(identity, child));
  assert(
    !revalidateWindowsBrowserListenerIdentity(identity, {
      ...child,
      startTime: "changed",
    }),
  );
});

function withProcessSnapshot(
  output: () => string,
  run: () => Promise<void>,
): Promise<void> {
  const original = Object.getOwnPropertyDescriptor(Deno, "Command");
  assert(original);
  class MockCommand {
    constructor(private cmd: string, private options: Deno.CommandOptions) {}
    output(): Promise<Deno.CommandOutput> {
      return Promise.resolve({
        success: true,
        code: 0,
        signal: null,
        stdout: new TextEncoder().encode(output()),
        stderr: new Uint8Array(),
      });
    }
    spawn() {
      return { output: () => this.output(), kill() {} };
    }
  }
  Object.defineProperty(Deno, "Command", {
    configurable: true,
    writable: true,
    enumerable: original.enumerable,
    value: MockCommand,
  });
  return (async () => {
    try {
      await run();
    } finally {
      Object.defineProperty(Deno, "Command", original);
    }
  })();
}

Deno.test("record refuses a reused PID before writing", async () => {
  const directory = await Deno.makeTempDir();
  const file = path.join(directory, "_dist", "dev-processes.json");
  try {
    const snapshot = {
      pid: 10896,
      ppid: 44524,
      name: "floorp.exe",
      commandLine: "changed command",
      startTime: CHILD_START,
    };
    await withProcessSnapshot(
      () => JSON.stringify([snapshot]),
      async () => {
        const expected = {
          pid: 10896,
          startTime: CHILD_START,
          commandLine: child.commandLine ?? "",
        };
        assertEquals(await recordDevProcess(10896, file, expected), false);
        assertEquals(readDevProcesses(file), []);
      },
    );
  } finally {
    await Deno.remove(directory, { recursive: true });
  }
});
