// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects } from "@std/assert";
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

Deno.test("file-backed logging preserves both streams, UTF-8 and nonzero exit", async () => {
  const directory = await Deno.makeTempDir({ prefix: "floorp-file-logging-" });
  const files = {
    stdout: path.join(directory, "stdout.log"),
    stderr: path.join(directory, "stderr.log"),
  };
  const output = { stdout: "", stderr: "" };
  const expected = "x".repeat(65_535) + "日\n" +
    "Marionette\tINFO\tListening on port 28291\n";
  try {
    const code = await ProcessUtils.runCommandWithFileLogging(
      [
        Deno.execPath(),
        "eval",
        'Deno.stdout.writeSync(new TextEncoder().encode("x".repeat(65535) + "日\\nMarionette\\tINFO\\tListening on port 28291\\n")); Deno.stderr.writeSync(new TextEncoder().encode("stderr without newline")); Deno.exit(7);',
      ],
      (stream, line) => output[stream] += line,
      files,
    );
    assertEquals(code, 7);
    assertEquals(output.stdout, expected);
    assertEquals(output.stderr, "stderr without newline\n");
    assertEquals(await Deno.readTextFile(files.stdout), expected);
    assertEquals(
      await Deno.readTextFile(files.stderr),
      "stderr without newline",
    );
    assertEquals(
      output.stdout.match(/Marionette\tINFO\tListening on port (\d+)/)?.[1],
      "28291",
    );
  } finally {
    await Deno.remove(directory, { recursive: true });
  }
});

Deno.test("native file output progresses while a log consumer is blocked", async () => {
  const directory = await Deno.makeTempDir({
    prefix: "floorp-output-progress-",
  });
  const files = {
    stdout: path.join(directory, "stdout.log"),
    stderr: path.join(directory, "stderr.log"),
  };
  const consumerStarted = path.join(directory, "consumer-started");
  const childFinished = path.join(directory, "child-finished");
  let blocked = false;
  const observed = { stdout: 0, stderr: 0 };
  const childScript = `
    const [consumerStarted, childFinished] = Deno.args;
    Deno.stdout.writeSync(new TextEncoder().encode("start\\n"));
    while (true) {
      try { await Deno.stat(consumerStarted); break; } catch {}
      await new Promise(resolve => setTimeout(resolve, 5));
    }
    const bytes = new Uint8Array(8192).fill(120);
    for (const output of [Deno.stdout, Deno.stderr]) {
      for (let i = 0; i < 1024; i++) output.writeSync(bytes);
    }
    await Deno.writeTextFile(childFinished, "done");
  `;
  try {
    const code = await ProcessUtils.runCommandWithFileLogging(
      [Deno.execPath(), "eval", childScript, consumerStarted, childFinished],
      (stream, line) => {
        observed[stream] += line.length;
        if (blocked) return;
        blocked = true;
        Deno.writeTextFileSync(consumerStarted, "started");
        const deadline = Date.now() + 10_000;
        const sleep = new Int32Array(new SharedArrayBuffer(4));
        // Intentionally block this event loop. A pipe would fill before the
        // child writes its completion marker; native file handles keep going.
        while (!exists(childFinished) && Date.now() < deadline) {
          Atomics.wait(sleep, 0, 0, 10);
        }
        assert(exists(childFinished), "child writes exceed pipe capacity");
      },
      files,
    );
    assertEquals(code, 0);
    assert(blocked);
    assert(observed.stdout >= 8 * 1024 * 1024);
    assert(observed.stderr >= 8 * 1024 * 1024);
    assertEquals((await Deno.stat(files.stdout)).size, 8 * 1024 * 1024 + 6);
    assertEquals((await Deno.stat(files.stderr)).size, 8 * 1024 * 1024);
  } finally {
    await Deno.remove(directory, { recursive: true });
  }
});

Deno.test("file-backed logging releases handles when spawning fails", async () => {
  const directory = await Deno.makeTempDir({
    prefix: "floorp-output-failure-",
  });
  try {
    await assertRejects(() =>
      ProcessUtils.runCommandWithFileLogging(
        [path.join(directory, "missing-executable")],
        () => {},
        {
          stdout: path.join(directory, "stdout.log"),
          stderr: path.join(directory, "stderr.log"),
        },
      )
    );
  } finally {
    await Deno.remove(directory, { recursive: true });
  }
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
