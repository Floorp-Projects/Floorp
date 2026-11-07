// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects } from "@std/assert";
import * as path from "@std/path";
import {
  listDevProcesses,
  readDevProcesses,
  recordDevProcess,
  reportDevPortConflict,
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
  return `${process.pid} ${process.ppid} ${process.startTime} ${process.commandLine}`;
}

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
