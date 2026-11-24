// SPDX-License-Identifier: MPL-2.0

import {
  assert,
  assertEquals,
  assertRejects,
  assertStringIncludes,
  assertThrows,
} from "@std/assert";
import { decodeBase64 } from "@std/encoding/base64";
import {
  buildCleanupScript,
  buildGuestSshArgs,
  buildInstallScript,
  buildLaunchScript,
  buildLockScript,
  buildMarionetteScript,
  buildPreflightScript,
  buildUploadScript,
  MacosQaError,
  MacosQaTarget,
  parseGuestResult,
  parseMacosQaTargetConfig,
  shellQuote,
} from "./fdh_qa_macos.ts";
import type {
  GuestRunOutput,
  MacosQaTargetConfig,
} from "./fdh_qa_macos_types.ts";

const RAW = {
  hostSsh: "user@100.92.228.4",
  vmDir: "/Users/user/Documents/Codex/FloorpStabilityVM",
  guestKeyFile: "guest_test_key",
  guestKnownHostsFile: "guest_known_hosts",
  guestUser: "admin",
  guestAddress: "192.168.128.2",
  testUser: "floorptest",
  qaRoot: "/Users/floorptest/fdh-qa",
  marionettePort: 2839,
  minFreeBytes: 2_000_000_000,
};
const CONFIG: MacosQaTargetConfig = parseMacosQaTargetConfig(RAW);
const TASK = "kan-144-live";
const SHA = "a".repeat(64);

function decodedGuestScript(args: string[]): string {
  const host = args.at(-1)!;
  const match = /printf %s '"'"'([A-Za-z0-9+/=]+)'"'"' \| base64 -D/.exec(
    host,
  );
  assert(match, "guest script is not base64-wrapped");
  return new TextDecoder().decode(decodeBase64(match[1]));
}

Deno.test("macOS QA config: accepts the stability VM target", () => {
  assertEquals(CONFIG.guestAddress, "192.168.128.2");
  assert(Object.isFrozen(CONFIG));
});

Deno.test("macOS QA config: rejects unsafe or incomplete values", () => {
  const cases: Array<[Record<string, unknown>, string]> = [
    [{ ...RAW, extra: 1 }, "unknown key"],
    [{ ...RAW, hostSsh: "user@host;rm -rf /" }, "hostSsh"],
    [{ ...RAW, hostSsh: "host" }, "hostSsh"],
    [{ ...RAW, vmDir: "relative/dir" }, "vmDir"],
    [{ ...RAW, vmDir: "/Users/user/../root" }, "vmDir"],
    [{ ...RAW, vmDir: "/Users/user/$(id)" }, "vmDir"],
    [{ ...RAW, guestKeyFile: "../id_rsa" }, "guestKeyFile"],
    [{ ...RAW, guestKeyFile: ".." }, "guestKeyFile"],
    [{ ...RAW, guestUser: "Admin User" }, "guestUser"],
    [{ ...RAW, guestAddress: "192.168.128.256" }, "guestAddress"],
    [{ ...RAW, qaRoot: "/tmp/fdh-qa" }, "qaRoot"],
    [{ ...RAW, qaRoot: "/Users/floorptest" }, "qaRoot"],
    [{ ...RAW, marionettePort: 80 }, "marionettePort"],
    [{ ...RAW, minFreeBytes: -1 }, "minFreeBytes"],
  ];
  for (const [value, message] of cases) {
    assertThrows(() => parseMacosQaTargetConfig(value), Error, message);
  }
  const missing: Record<string, unknown> = { ...RAW };
  delete missing.qaRoot;
  assertThrows(
    () => parseMacosQaTargetConfig(missing),
    Error,
    "missing qaRoot",
  );
  assertThrows(() => parseMacosQaTargetConfig([]), Error, "JSON object");
});

Deno.test("macOS QA shellQuote: single quotes survive", () => {
  assertEquals(shellQuote("a'b"), `'a'"'"'b'`);
  assertEquals(shellQuote(""), "''");
  assertThrows(() => shellQuote("a\0b"), Error, "NUL");
});

Deno.test("macOS QA ssh args: two hops, credentials named on the host, script only as base64", () => {
  const script = "echo 'hello $USER'; rm -rf \"$X\"";
  const args = buildGuestSshArgs(CONFIG, script);
  assertEquals(args.slice(0, 6), [
    "-o",
    "BatchMode=yes",
    "-o",
    "ConnectTimeout=15",
    "-o",
    "ServerAliveInterval=15",
  ]);
  assertEquals(args[6], "user@100.92.228.4");
  assertEquals(args.length, 8);
  const host = args[7];
  assert(
    host.startsWith(
      "cd '/Users/user/Documents/Codex/FloorpStabilityVM' && exec ssh -i 'guest_test_key' ",
    ),
  );
  assertStringIncludes(host, "-o 'UserKnownHostsFile=guest_known_hosts'");
  assertStringIncludes(host, "-o StrictHostKeyChecking=yes");
  assertStringIncludes(host, "'admin@192.168.128.2'");
  assert(!host.includes("hello"), "script text leaked into the command line");
  assertEquals(decodedGuestScript(args), script);
});

Deno.test("macOS QA scripts: the task directory never appears literally (cleanup must not match itself)", () => {
  const literal = `${CONFIG.qaRoot}/${TASK}`;
  const scripts = [
    buildLockScript(CONFIG, TASK),
    buildUploadScript(CONFIG, TASK, "dmg"),
    buildInstallScript(CONFIG, TASK, "dmg", SHA),
    buildInstallScript(CONFIG, TASK, "tar.gz", SHA),
    buildLaunchScript(CONFIG, TASK),
    buildMarionetteScript(CONFIG, TASK, { op: "eval", script: "return 1" }),
    buildCleanupScript(CONFIG, TASK),
  ];
  for (const script of scripts) {
    assert(!script.includes(literal), "literal task dir in script");
    assertStringIncludes(script, "set -euo pipefail");
    assertStringIncludes(script, 'TASKDIR="$ROOT""/""$TASK"');
  }
  for (const script of scripts.slice(1, 6)) {
    assertStringIncludes(script, "|| fail lock-not-held");
  }
});

Deno.test("macOS QA scripts: argument validation", () => {
  assertThrows(() => buildLockScript(CONFIG, "Bad/Task"), Error, "--task-id");
  assertThrows(
    () => buildInstallScript(CONFIG, TASK, "dmg", "ABC"),
    Error,
    "sha256",
  );
  assertThrows(
    () => buildLaunchScript(CONFIG, TASK, "javascript:alert(1)"),
    Error,
    "startUrl",
  );
  assertThrows(
    () => buildLaunchScript(CONFIG, TASK, "https://x/'$(id)"),
    Error,
    "startUrl",
  );
  assertStringIncludes(
    buildLaunchScript(CONFIG, TASK, "https://floorp.app/"),
    "-marionette 'https://floorp.app/'",
  );
});

Deno.test("macOS QA scripts: launch uses the GUI session, a fresh profile and the configured port", () => {
  const script = buildLaunchScript(CONFIG, TASK);
  assertStringIncludes(script, "gui_session || fail gui-session-absent");
  assertStringIncludes(script, 'sudo -n rm -rf "$PROFILE"');
  assertStringIncludes(script, "MPORT=2839");
  assertStringIncludes(
    script,
    'launchctl asuser "$TUID" sudo -n -u "$TUSER" open -n -a "$APP"',
  );
  assertStringIncludes(script, "fail marionette-port-busy");
});

Deno.test("macOS QA scripts: install verifies the hash before extracting", () => {
  const script = buildInstallScript(CONFIG, TASK, "dmg", SHA);
  const verify = script.indexOf("fail sha256-mismatch");
  const attach = script.indexOf("hdiutil attach");
  assert(verify > 0 && attach > verify);
  assertStringIncludes(script, "-readonly -noautoopen");
});

Deno.test("macOS QA scripts: cleanup kills by task dir from env and releases only its own lock", () => {
  const script = buildCleanupScript(CONFIG, TASK);
  assertStringIncludes(script, `D="$TASKDIR" awk`);
  assertStringIncludes(script, 'index($0, ENVIRON["D"])');
  assertStringIncludes(
    script,
    'if [ -n "$OWNER" ] && [ "$OWNER" != "$TASK" ]; then fail lock-held; fi',
  );
  assertStringIncludes(
    script,
    'if [ "$OWNER" = "$TASK" ] && [ "$LEFT" = 0 ] && [ "$GONE" = true ]; then',
  );
  const preflight = buildPreflightScript(CONFIG);
  assertStringIncludes(preflight, 'minFreeBytes\\":2000000000');
  assertStringIncludes(preflight, '!index($0, ENVIRON["R"])');
});

Deno.test("macOS QA result parsing", () => {
  const ok = (stdout: string, code = 0): GuestRunOutput => ({
    code,
    stdout,
    stderr: "",
  });
  assertEquals(
    parseGuestResult(
      ok('noise\nFDH_RESULT {"ok":false}\nFDH_RESULT {"ok":true,"a":1}\n'),
    ),
    { ok: true, a: 1 },
  );
  assertEquals(parseGuestResult(ok("nothing")).error, "no-result");
  assertEquals(
    parseGuestResult(ok("FDH_RESULT {bad")).error,
    "malformed-result",
  );
  assertEquals(
    parseGuestResult(ok("FDH_RESULT [1]")).error,
    "malformed-result",
  );
  assertEquals(
    parseGuestResult(ok('FDH_RESULT {"ok":true}', 255)).error,
    "nonzero-exit",
  );
});

function fakeTransport(replies: string[]) {
  const calls: Array<{ script: string; stdin: boolean }> = [];
  const transport = (script: string, stdin?: ReadableStream<Uint8Array>) => {
    calls.push({ script, stdin: stdin !== undefined });
    const stdout = replies.shift() ?? "";
    return Promise.resolve({
      code: stdout.includes('"ok":true') ? 0 : 3,
      stdout,
      stderr: "",
    });
  };
  return { calls, transport };
}

Deno.test("macOS QA target: preflight reports blockers", async () => {
  const { transport } = fakeTransport([
    'FDH_RESULT {"ok":true,"freeBytes":100,"minFreeBytes":2000000000,"guiSession":false,"lockHolder":"other-task","marionettePortBusy":true,"foreignFloorp":2,"osVersion":"26.6.2"}',
  ]);
  const result = await new MacosQaTarget(CONFIG, transport).preflight();
  assertEquals(result.ready, false);
  assertEquals(result.blockers, [
    "low-disk",
    "gui-session-absent",
    "lock-held",
    "marionette-port-busy",
    "foreign-floorp-running",
  ]);
  assertEquals(result.foreignFloorp, 2);
  assertEquals(result.lockHolder, "other-task");
});

Deno.test("macOS QA target: stage streams only the upload and fails with the step name", async () => {
  const { calls, transport } = fakeTransport([
    'FDH_RESULT {"ok":true,"bytes":42}',
    'FDH_RESULT {"ok":true,"version":"12.21.1@159.0.1"}',
  ]);
  const target = new MacosQaTarget(CONFIG, transport);
  const staged = await target.stage(TASK, "dmg", new Blob(["x"]).stream(), SHA);
  assertEquals(staged, { bytes: 42, version: "12.21.1@159.0.1" });
  assertEquals(calls.map((c) => c.stdin), [true, false]);

  const failing = fakeTransport([
    'FDH_RESULT {"ok":false,"error":"sha256-mismatch"}',
  ]);
  const error = await assertRejects(
    () => new MacosQaTarget(CONFIG, failing.transport).launch(TASK),
    MacosQaError,
    "macOS QA launch failed: sha256-mismatch",
  );
  assertEquals(error.step, "launch");
});

Deno.test("macOS QA target: cleanup is clean only when nothing is left", async () => {
  const { transport } = fakeTransport([
    'FDH_RESULT {"ok":true,"residualProcesses":0,"taskDirRemoved":true,"lockReleased":true}',
    'FDH_RESULT {"ok":true,"residualProcesses":1,"taskDirRemoved":true,"lockReleased":false}',
  ]);
  const target = new MacosQaTarget(CONFIG, transport);
  assertEquals((await target.cleanup(TASK)).clean, true);
  assertEquals((await target.cleanup(TASK)).clean, false);
});
