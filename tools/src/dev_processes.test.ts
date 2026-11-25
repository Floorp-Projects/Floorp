// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects } from "@std/assert";
import * as path from "@std/path";
import {
  devProcessExitConfirmed,
  listDevProcesses,
  readDevProcesses,
  recordDevProcess,
  reportDevPortConflict,
  reportDevStartupPortConflict,
} from "./dev_processes.ts";
import type { DevProcessInfo } from "./dev_process_types.ts";

async function withCommandOutput(
  output: (cmd: string, args: string[]) => string,
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
        stdout: new TextEncoder().encode(
          output(this.cmd, this.options.args ?? []),
        ),
        stderr: new Uint8Array(),
      });
    }
    spawn() {
      const result = this.output();
      return {
        status: result,
        stdout: new ReadableStream({
          async start(controller) {
            controller.enqueue((await result).stdout);
            controller.close();
          },
        }),
        stderr: new ReadableStream({
          async start(controller) {
            controller.enqueue((await result).stderr);
            controller.close();
          },
        }),
        kill() {},
      };
    }
  }
  Object.defineProperty(Deno, "Command", {
    configurable: true,
    writable: true,
    enumerable: original.enumerable,
    value: MockCommand,
  });
  try {
    await run();
  } finally {
    Object.defineProperty(Deno, "Command", original);
  }
}

const process: DevProcessInfo = {
  pid: 987654321,
  ppid: 1,
  name: "deno.exe",
  commandLine: "deno run -A npm:vite",
  startTime: "Sat Nov  7 10:30:00 2026",
};

function snapshotOutput(_cmd: string, args: string[]): string {
  if (Deno.build.os === "windows") return JSON.stringify([process]);
  if (args.includes("pid=,comm=")) return `${process.pid} deno`;
  // ps -o pid=,ppid=,pgid=,lstart=,args= (the group id is the pid here).
  return `${process.pid} ${process.ppid} ${process.pid} ${process.startTime} ${process.commandLine}`;
}

Deno.test("process inventory cancels its child, has its own deadline, and never writes a cancelled registration", async () => {
  const directory = await Deno.makeTempDir();
  const file = path.join(directory, "registry.json");
  const descriptor = Object.getOwnPropertyDescriptor(Deno, "Command")!;
  const empty = {
    success: true,
    code: 0,
    signal: null,
    stdout: new Uint8Array(),
    stderr: new Uint8Array(),
  };
  let spawned = Promise.withResolvers<void>();
  let output = Promise.withResolvers<Deno.CommandOutput>();
  let kills = 0;
  class MockCommand {
    spawn() {
      spawned.resolve();
      return {
        status: output.promise,
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
          kills++;
          output.resolve(empty);
        },
      };
    }
  }
  try {
    await Deno.writeTextFile(file, "unchanged");
    Object.defineProperty(Deno, "Command", {
      configurable: true,
      enumerable: descriptor.enumerable,
      value: MockCommand,
    });
    const controller = new AbortController();
    const registration = recordDevProcess(
      81,
      file,
      undefined,
      controller.signal,
    );
    await spawned.promise;
    controller.abort(new Error("cancelled registration"));
    await assertRejects(() => registration, Error, "cancelled registration");
    assertEquals(await Deno.readTextFile(file), "unchanged");
    spawned = Promise.withResolvers<void>();
    output = Promise.withResolvers<Deno.CommandOutput>();
    await assertRejects(
      () => listDevProcesses(undefined, undefined, 20),
      Error,
      "Process inventory timed out",
    );
    output = Promise.withResolvers<Deno.CommandOutput>();
    assertEquals(
      await devProcessExitConfirmed(
        81,
        (pid) => listDevProcesses(pid, undefined, 20),
      ),
      false,
    );
    assert(kills >= 2);
    const before = kills;
    await assertRejects(
      () => listDevProcesses(undefined, controller.signal),
      Error,
      "cancelled registration",
    );
    assertEquals(kills, before);
  } finally {
    Object.defineProperty(Deno, "Command", descriptor);
    await Deno.remove(directory, { recursive: true });
  }
});

Deno.test("startup treats a failed inventory as unknown and confirms exit only from a successful empty snapshot", async () => {
  let checks = 0;
  const inspect = (pid?: number) => {
    assertEquals(pid, 81);
    checks++;
    if (checks === 1) {
      return Promise.reject(new Error("Process inventory timed out"));
    }
    return Promise.resolve(checks === 2 ? [{ ...process, pid: 81 }] : []);
  };
  assertEquals(await devProcessExitConfirmed(81, inspect), false);
  assertEquals(await devProcessExitConfirmed(81, inspect), false);
  assertEquals(await devProcessExitConfirmed(81, inspect), true);
  const source = await Deno.readTextFile(
    new URL("../dev-tool.ts", import.meta.url),
  );
  assert(
    source.replace(/\s/g, "").includes(
      "i%5===4&&awaitdevProcessExitConfirmed(Number(pid),listDevProcesses,signal,",
    ),
  );
  assert(source.replace(/\s/g, "").includes("Math.min(5000,remaining())"));
});

Deno.test("process inventory accepts an empty snapshot and propagates command failures", async () => {
  for (const timeout of [0, -1, NaN, Infinity]) {
    await assertRejects(
      () => listDevProcesses(undefined, undefined, timeout),
      TypeError,
      "Invalid process inventory timeout",
    );
  }
  await withCommandOutput(
    () => "",
    async () => assertEquals(await listDevProcesses(), []),
  );
  await withCommandOutput(() => {
    throw new Error("inventory failed");
  }, async () => {
    await assertRejects(() => listDevProcesses(), Error, "inventory failed");
  });
});

Deno.test("ordinary structured startup conflicts retain owner lookup and recovery advice; native and invalid ports stay private", async () => {
  const error = console.error;
  const printed: unknown[][] = [];
  let lookups = 0;
  console.error = (...args: unknown[]) => {
    printed.push(args);
  };
  try {
    await withCommandOutput(() => {
      lookups++;
      return "port 15611 PID 81";
    }, async () => {
      for (const port of [null, 0, -1, 65536, NaN]) {
        await reportDevStartupPortConflict(port, false);
      }
      await reportDevStartupPortConflict(15611, true);
      assertEquals(lookups, 0);
      await reportDevStartupPortConflict(15611, false);
      assertEquals(lookups, 1);
      assert(JSON.stringify(printed).includes("PID 81"));
      assert(JSON.stringify(printed).includes("owning checkout"));
    });
    const source = await Deno.readTextFile(
      new URL("../dev-tool.ts", import.meta.url),
    );
    assert(
      source.includes("await reportDevStartupPortConflict(port, nativeLaunch)"),
    );
  } finally {
    console.error = error;
  }
});

Deno.test("process recorder writes identity atomically and preserves orphan records", async () => {
  const directory = await Deno.makeTempDir();
  const file = path.join(directory, "_dist", "dev-processes.json");
  const orphan = {
    pid: 987654320,
    startTime: "old-start",
    commandLine: "deno vite",
  };
  await Deno.mkdir(path.dirname(file));
  await Deno.writeTextFile(
    file,
    JSON.stringify({ version: 1, processes: [orphan] }),
  );
  try {
    await withCommandOutput(snapshotOutput, async () => {
      await recordDevProcess(process.pid, file);
      await recordDevProcess(process.pid, file);
      assertEquals(readDevProcesses(file), [orphan, {
        pid: process.pid,
        startTime: process.startTime,
        commandLine: process.commandLine,
      }]);
      assertEquals(
        [...Deno.readDirSync(path.dirname(file))].map((entry) => entry.name),
        ["dev-processes.json"],
      );
    });
  } finally {
    await Deno.remove(directory, { recursive: true });
  }
});

Deno.test("process reader tolerates a missing checkout registry", async () => {
  const directory = await Deno.makeTempDir();
  try {
    assertEquals(readDevProcesses(path.join(directory, "missing.json")), []);
  } finally {
    await Deno.remove(directory, { recursive: true });
  }
});

Deno.test(
  "Windows process snapshot handles zero and one entries and rejects malformed data",
  {
    ignore: Deno.build.os !== "windows",
  },
  async () => {
    await withCommandOutput(
      () => "[]",
      async () => assertEquals(await listDevProcesses(), []),
    );
    await withCommandOutput(
      snapshotOutput,
      async () => assertEquals(await listDevProcesses(process.pid), [process]),
    );
    await withCommandOutput(() => '[{"pid":1}]', async () => {
      await assertRejects(
        () => listDevProcesses(),
        Error,
        "Invalid Windows process entry",
      );
    });
  },
);

Deno.test("port conflict diagnostics query the reported port and print its PID and recovery command", async () => {
  const original = console.error;
  const messages: string[] = [];
  console.error = (...values: unknown[]) => {
    messages.push(values.join(" "));
  };
  try {
    await withCommandOutput(
      (_cmd, args) => {
        assert(args.some((arg) => arg.includes("5181")));
        return "[dev-tool] Port 5181: PID 1234";
      },
      async () =>
        assert(
          await reportDevPortConflict("Error: Port 5181 is already in use"),
        ),
    );
    assert(messages.some((line) => line.includes("Port 5181: PID 1234")));
    assert(messages.some((line) => line.includes("stop --all")));
  } finally {
    console.error = original;
  }
});

Deno.test("normal startup logs do not query port owners", async () => {
  await withCommandOutput(() => {
    throw new Error("Unexpected subprocess");
  }, async () => {
    assertEquals(await reportDevPortConflict("Vite ready on port 5181"), false);
  });
});
