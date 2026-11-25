// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects } from "@std/assert";
import { commandOutput } from "./dev_process_command.ts";

Deno.test("cancellable command output collects normal and empty output and preserves command failures", async () => {
  assertEquals(
    await commandOutput(
      Deno.execPath(),
      ["eval", 'Deno.stdout.writeSync(new TextEncoder().encode("ok"))'],
      undefined,
      new AbortController().signal,
    ),
    "ok",
  );
  assertEquals(
    await commandOutput(
      Deno.execPath(),
      ["eval", ""],
      undefined,
      new AbortController().signal,
    ),
    "",
  );
  await assertRejects(
    () =>
      commandOutput(
        Deno.execPath(),
        ["eval", 'console.error("fixture failure");Deno.exit(1)'],
        undefined,
        new AbortController().signal,
      ),
    Error,
    "fixture failure",
  );
});

Deno.test("cancellation cancels inherited pipe reads after its bounded drain, even when the owned child is already reaped", async () => {
  const descriptor = Object.getOwnPropertyDescriptor(Deno, "Command")!;
  const status = Promise.withResolvers<Deno.CommandStatus>();
  const reading = Promise.withResolvers<void>();
  let pulls = 0;
  let cancels = 0;
  let kills = 0;
  const pipe = () =>
    new ReadableStream<Uint8Array>({
      pull() {
        if (++pulls === 2) reading.resolve();
        return new Promise<void>(() => {});
      },
      cancel() {
        cancels++;
      },
    });
  const stdout = pipe();
  const stderr = pipe();
  class MockCommand {
    spawn() {
      return {
        stdout,
        stderr,
        status: status.promise,
        kill(signal: string) {
          assertEquals(signal, "SIGKILL");
          kills++;
          status.resolve({ success: false, code: 1, signal: "SIGKILL" });
        },
      };
    }
  }
  try {
    Object.defineProperty(Deno, "Command", {
      configurable: true,
      value: MockCommand,
    });
    const controller = new AbortController();
    const pending = commandOutput("fixture", [], undefined, controller.signal);
    await reading.promise;
    controller.abort(new Error("cancelled fixture"));
    await assertRejects(() => pending, Error, "cancelled fixture");
    assertEquals([kills, cancels, stdout.locked, stderr.locked], [
      1,
      2,
      false,
      false,
    ]);
    await assertRejects(
      () => commandOutput("fixture", [], undefined, controller.signal),
      Error,
      "cancelled fixture",
    );
    assertEquals(kills, 1);
  } finally {
    Object.defineProperty(Deno, "Command", descriptor);
  }
});

Deno.test("a failed output read still cancels the other inherited pipe after reaping its owned child", async () => {
  const descriptor = Object.getOwnPropertyDescriptor(Deno, "Command")!;
  const status = Promise.withResolvers<Deno.CommandStatus>();
  let kills = 0;
  let cancels = 0;
  const stdout = new ReadableStream<Uint8Array>({
    start(controller) {
      controller.error(new Error("fixture read failed"));
    },
  });
  const stderr = new ReadableStream<Uint8Array>({
    pull() {
      return new Promise<void>(() => {});
    },
    cancel() {
      cancels++;
    },
  });
  class MockCommand {
    spawn() {
      return {
        stdout,
        stderr,
        status: status.promise,
        kill(signal: string) {
          assertEquals(signal, "SIGKILL");
          kills++;
          status.resolve({ success: false, code: 1, signal: "SIGKILL" });
        },
      };
    }
  }
  try {
    Object.defineProperty(Deno, "Command", {
      configurable: true,
      value: MockCommand,
    });
    await assertRejects(
      () =>
        commandOutput("fixture", [], undefined, new AbortController().signal),
      Error,
      "fixture read failed",
    );
    assertEquals([kills, cancels, stdout.locked, stderr.locked], [
      1,
      1,
      false,
      false,
    ]);
  } finally {
    Object.defineProperty(Deno, "Command", descriptor);
  }
});
