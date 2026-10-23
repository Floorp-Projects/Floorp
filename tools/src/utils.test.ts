// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertThrows } from "@std/assert";
import {
  arch,
  exists,
  platform,
  ProcessUtils,
  PROJECT_ROOT,
  resolveFromRoot,
} from "./utils.ts";
import * as path from "@std/path";

Deno.test("resolveFromRoot returns absolute path under PROJECT_ROOT", () => {
  const result = resolveFromRoot("deno.json");
  assert(path.isAbsolute(result), "should be absolute");
  assert(result.endsWith("deno.json"), "should end with deno.json");
});

Deno.test("resolveFromRoot with nested path", () => {
  const result = resolveFromRoot("tools/src/utils.ts");
  assert(result.includes("tools"), "should contain tools");
  assert(result.includes("utils.ts"), "should contain utils.ts");
});

Deno.test("platform returns a valid value", () => {
  const p = platform();
  assert(
    ["windows", "darwin", "linux"].includes(p),
    `platform should be one of windows/darwin/linux, got: ${p}`,
  );
});

Deno.test("arch returns a valid value", () => {
  const a = arch();
  assert(
    ["aarch64", "x86_64"].includes(a),
    `arch should be aarch64 or x86_64, got: ${a}`,
  );
});

Deno.test("exists returns true for existing file", () => {
  assertEquals(
    exists(path.join(PROJECT_ROOT, "deno.json")),
    true,
    "deno.json should exist",
  );
});

Deno.test("exists returns false for non-existing file", () => {
  assertEquals(
    exists(path.join(PROJECT_ROOT, "non-existent-file-12345.txt")),
    false,
    "non-existent file should return false",
  );
});

Deno.test("PROJECT_ROOT is a valid directory", () => {
  assert(exists(PROJECT_ROOT), "PROJECT_ROOT should exist");
});

Deno.test("file-output spawn completes both streams while parent JS is blocked", async () => {
  const directory = await Deno.makeTempDir({ prefix: "floorp-spawn-output-" });
  const files = {
    stdout: path.join(directory, "stdout.log"),
    stderr: path.join(directory, "stderr.log"),
  };
  const marker = path.join(directory, "complete");
  let child:
    | ReturnType<typeof ProcessUtils.spawnCommandWithFileOutput>
    | undefined;
  try {
    child = ProcessUtils.spawnCommandWithFileOutput(
      [
        Deno.execPath(),
        "eval",
        `const bytes = new Uint8Array(8192).fill(120);
        for (const output of [Deno.stdout, Deno.stderr]) {
          for (let i = 0; i < 1024; i++) output.writeSync(bytes);
        }
        Deno.writeTextFileSync(Deno.args[0], Deno.cwd());`,
        marker,
      ],
      files,
      directory,
    );
    assert(child.pid > 0);
    const deadline = Date.now() + 10_000;
    const sleep = new Int32Array(new SharedArrayBuffer(4));
    while (!exists(marker) && Date.now() < deadline) {
      Atomics.wait(sleep, 0, 0, 10);
    }
    assert(
      exists(marker),
      "native file output needs no parent event-loop reads",
    );
    assertEquals(await Deno.readTextFile(marker), directory);
    assertEquals(await child.status, { code: 0 });
    assertEquals((await Deno.stat(files.stdout)).size, 8 * 1024 * 1024);
    assertEquals((await Deno.stat(files.stderr)).size, 8 * 1024 * 1024);
  } finally {
    if (child) {
      child.kill();
      await child.status.catch(() => {});
    }
    await Deno.remove(directory, { recursive: true });
  }
});

Deno.test("file-output spawn stops its owned child handle", async () => {
  const directory = await Deno.makeTempDir({ prefix: "floorp-spawn-kill-" });
  let child:
    | ReturnType<typeof ProcessUtils.spawnCommandWithFileOutput>
    | undefined;
  try {
    child = ProcessUtils.spawnCommandWithFileOutput(
      [
        Deno.execPath(),
        "eval",
        "await new Promise(() => setInterval(() => {}, 1000));",
      ],
      {
        stdout: path.join(directory, "stdout.log"),
        stderr: path.join(directory, "stderr.log"),
      },
    );
    child.kill("SIGTERM");
    assert((await child.status).code !== 0);
    child.kill("SIGTERM");
  } finally {
    if (child) {
      child.kill();
      await child.status.catch(() => {});
    }
    await Deno.remove(directory, { recursive: true });
  }
});

Deno.test("file-output spawn retains UTF-8, startup output and nonzero exit", async () => {
  const directory = await Deno.makeTempDir({ prefix: "floorp-output-status-" });
  const files = {
    stdout: path.join(directory, "stdout.log"),
    stderr: path.join(directory, "stderr.log"),
  };
  const expected = "x".repeat(65_535) + "日\n" +
    "Marionette\tINFO\tListening on port 28291\n";
  try {
    const child = ProcessUtils.spawnCommandWithFileOutput(
      [
        Deno.execPath(),
        "eval",
        'Deno.stdout.writeSync(new TextEncoder().encode("x".repeat(65535) + "日\\nMarionette\\tINFO\\tListening on port 28291\\n")); Deno.stderr.writeSync(new TextEncoder().encode("stderr without newline")); Deno.exit(7);',
      ],
      files,
    );
    assertEquals(await child.status, { code: 7 });
    assertEquals(await Deno.readTextFile(files.stdout), expected);
    assertEquals(
      await Deno.readTextFile(files.stderr),
      "stderr without newline",
    );
  } finally {
    await Deno.remove(directory, { recursive: true });
  }
});

Deno.test("file-output spawn releases handles when spawning fails", async () => {
  const directory = await Deno.makeTempDir({
    prefix: "floorp-output-failure-",
  });
  try {
    assertThrows(() =>
      ProcessUtils.spawnCommandWithFileOutput(
        [path.join(directory, "missing-executable")],
        {
          stdout: path.join(directory, "stdout.log"),
          stderr: path.join(directory, "stderr.log"),
        },
      )
    );
    // The native spawn error is asynchronous on some platforms. Let its
    // installed handler observe it before this test's resource checks.
    await new Promise((resolve) => setTimeout(resolve, 10));
  } finally {
    await Deno.remove(directory, { recursive: true });
  }
});
