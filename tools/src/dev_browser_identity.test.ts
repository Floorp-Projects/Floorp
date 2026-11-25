// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects } from "@std/assert";
import * as path from "@std/path";
import {
  captureWindowsBrowserListenerIdentity,
  listWindowsBrowserListeners,
  listWindowsBrowserProcesses,
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
        output: () => output.promise,
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
