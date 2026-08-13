// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertMatch } from "@std/assert";
import * as path from "@std/path";

const sourcePath = path.fromFileUrl(
  new URL("../g5_macos_lifecycle_helper.c", import.meta.url),
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
    "macOS G5 lifecycle helper reads a session with a high-resolution generation",
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
      assert(typeof record.pgid === "number" && record.pgid > 0);
      assert(typeof record.sid === "number" && record.sid > 0);
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
    "macOS G5 lifecycle helper rejects launch mode instead of executing a command",
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

      const rejected = commandOutput(helperPath, [
        "launch",
        "--",
        "/usr/bin/true",
      ]);
      assertEquals(rejected.code, 64);
      assertEquals(decode(rejected.stdout), "");
    } finally {
      Deno.removeSync(temporaryDirectory, { recursive: true });
    }
  },
});

Deno.test({
  name:
    "macOS G5 lifecycle helper rejects every mode except exact probe-session",
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

      for (const args of [
        [] as readonly string[],
        ["unknown"],
        ["launch", "--", "/usr/bin/true"],
        ["exec", "--", "/usr/bin/true"],
        ["probe-session", "unexpected"],
      ]) {
        const rejected = commandOutput(helperPath, args);
        assertEquals(rejected.code, 64);
        assertEquals(decode(rejected.stdout), "");
        assertMatch(decode(rejected.stderr), /^usage: .* probe-session\n$/u);
      }
    } finally {
      Deno.removeSync(temporaryDirectory, { recursive: true });
    }
  },
});

Deno.test({
  name:
    "macOS G5 lifecycle helper rejects exec mode instead of executing a command",
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

      const rejected = commandOutput(helperPath, [
        "exec",
        "--",
        "/usr/bin/true",
      ]);
      assertEquals(rejected.code, 64);
      assertEquals(decode(rejected.stdout), "");
    } finally {
      Deno.removeSync(temporaryDirectory, { recursive: true });
    }
  },
});
