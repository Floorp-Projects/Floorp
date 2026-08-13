// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertMatch } from "@std/assert";
import * as path from "@std/path";

const sourcePath = path.fromFileUrl(
  new URL("../native/g5_macos_lifecycle_helper.c", import.meta.url),
);

function commandOutput(command: string, args: readonly string[]) {
  return new Deno.Command(command, {
    args: [...args],
    stderr: "piped",
    stdout: "piped",
  }).outputSync();
}

function decode(bytes: Uint8Array): string {
  return new TextDecoder().decode(bytes);
}

Deno.test({
  name:
    "macOS G5 lifecycle helper creates an exclusive session with a high-resolution generation",
  sanitizeOps: false,
  sanitizeResources: false,
  fn() {
    if (Deno.build.os !== "darwin") return;
    const temporaryDirectory = Deno.makeTempDirSync({
      prefix: "floorp-g5-lifecycle-helper-test-",
    });
    const helperPath = path.join(
      temporaryDirectory,
      "g5-macos-lifecycle-helper",
    );
    try {
      const compile = commandOutput("/usr/bin/cc", [
        "-std=c17",
        "-Wall",
        "-Wextra",
        "-Werror",
        sourcePath,
        "-o",
        helperPath,
      ]);
      assertEquals(decode(compile.stderr), "");
      assert(compile.success);

      const probe = commandOutput(helperPath, ["probe-session"]);
      assertEquals(decode(probe.stderr), "");
      assert(probe.success);
      const record = JSON.parse(decode(probe.stdout)) as Record<
        string,
        unknown
      >;
      assert(typeof record.pid === "number" && record.pid > 0);
      assertEquals(record.pid, record.pgid);
      assertEquals(record.pid, record.sid);
      assert(typeof record.start_abstime === "string");
      assertMatch(record.start_abstime, /^[1-9][0-9]{8,}$/u);
      assertEquals(
        record.process_generation,
        `pid-${record.pid}-generation-${record.start_abstime}`,
      );
      assertEquals(record.descendant_ownership, "not-established");
      assertEquals(record.event_stream, "incomplete");
    } finally {
      Deno.removeSync(temporaryDirectory, { recursive: true });
    }
  },
});

Deno.test({
  name:
    "macOS G5 lifecycle helper exec mode preserves the tracked root PID in an exclusive process group",
  sanitizeOps: false,
  sanitizeResources: false,
  async fn() {
    if (Deno.build.os !== "darwin") return;
    const temporaryDirectory = Deno.makeTempDirSync({
      prefix: "floorp-g5-lifecycle-helper-test-",
    });
    const helperPath = path.join(
      temporaryDirectory,
      "g5-macos-lifecycle-helper",
    );
    try {
      const compile = commandOutput("/usr/bin/cc", [
        "-std=c17",
        "-Wall",
        "-Wextra",
        "-Werror",
        sourcePath,
        "-o",
        helperPath,
      ]);
      assertEquals(decode(compile.stderr), "");
      assert(compile.success);

      const child = new Deno.Command(helperPath, {
        args: [
          "exec",
          "--",
          "/bin/sh",
          "-c",
          "ps -o pid=,pgid= -p $$; /bin/kill -0 -$$",
        ],
        stderr: "piped",
        stdout: "piped",
      }).spawn();
      const expectedRootPid = child.pid;
      const output = await child.output();
      assertEquals(decode(output.stderr), "");
      assert(output.success);
      const values = decode(output.stdout).trim().split(/\s+/u).map(Number);
      assertEquals(values, [expectedRootPid, expectedRootPid]);
    } finally {
      Deno.removeSync(temporaryDirectory, { recursive: true });
    }
  },
});

Deno.test({
  name:
    "macOS G5 lifecycle helper probes a process that is already a session leader",
  sanitizeOps: false,
  sanitizeResources: false,
  fn() {
    if (Deno.build.os !== "darwin") return;
    const temporaryDirectory = Deno.makeTempDirSync({
      prefix: "floorp-g5-lifecycle-helper-test-",
    });
    const helperPath = path.join(
      temporaryDirectory,
      "g5-macos-lifecycle-helper",
    );
    try {
      const compile = commandOutput("/usr/bin/cc", [
        "-std=c17",
        "-Wall",
        "-Wextra",
        "-Werror",
        sourcePath,
        "-o",
        helperPath,
      ]);
      assertEquals(decode(compile.stderr), "");
      assert(compile.success);

      const probe = commandOutput(helperPath, [
        "launch",
        "--",
        helperPath,
        "probe-session",
      ]);
      assertEquals(decode(probe.stderr), "");
      assert(probe.success);
      const record = JSON.parse(decode(probe.stdout)) as Record<
        string,
        unknown
      >;
      assertEquals(record.pid, record.pgid);
      assertEquals(record.pid, record.sid);
      assertMatch(record.start_abstime as string, /^[1-9][0-9]{8,}$/u);
    } finally {
      Deno.removeSync(temporaryDirectory, { recursive: true });
    }
  },
});

Deno.test({
  name:
    "macOS G5 lifecycle helper launches a command in the helper-owned session",
  sanitizeOps: false,
  sanitizeResources: false,
  fn() {
    if (Deno.build.os !== "darwin") return;
    const temporaryDirectory = Deno.makeTempDirSync({
      prefix: "floorp-g5-lifecycle-helper-test-",
    });
    const helperPath = path.join(
      temporaryDirectory,
      "g5-macos-lifecycle-helper",
    );
    try {
      const compile = commandOutput("/usr/bin/cc", [
        "-std=c17",
        "-Wall",
        "-Wextra",
        "-Werror",
        sourcePath,
        "-o",
        helperPath,
      ]);
      assertEquals(decode(compile.stderr), "");
      assert(compile.success);

      const launch = commandOutput(helperPath, [
        "launch",
        "--",
        "/bin/sh",
        "-c",
        "ps -o pid=,pgid= -p $$; /bin/kill -0 -$$",
      ]);
      assertEquals(decode(launch.stderr), "");
      assert(launch.success);
      const values = decode(launch.stdout).trim().split(/\s+/u).map(Number);
      assertEquals(values.length, 2);
      assert(values.every((value) => Number.isSafeInteger(value) && value > 0));
      assertEquals(values[0], values[1]);
    } finally {
      Deno.removeSync(temporaryDirectory, { recursive: true });
    }
  },
});
