// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects } from "@std/assert";
import * as path from "@std/path";
import {
  captureWindowsBrowserListenerIdentity,
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
import { readDevProcesses, recordDevProcess } from "./dev_processes.ts";

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
