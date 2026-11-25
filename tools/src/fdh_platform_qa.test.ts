// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertStringIncludes } from "@std/assert";
import { encodeBase64 } from "@std/encoding/base64";
import {
  evalText,
  payloadKindFor,
  type PlatformQaIo,
  type PlatformQaTarget,
  PROBE_SCRIPT,
  probeProblems,
  redact,
  runPlatformQa,
  runPlatformQaCli,
} from "./fdh_platform_qa.ts";
import type { PlatformQaPreflight } from "./fdh_platform_qa_types.ts";
import { QaTargetError } from "./fdh_qa_remote.ts";

const SHA = "a".repeat(64);
const TASK = "kan-145-t1";

interface FakeOptions {
  platform?: "macos" | "linux";
  preflights?: Partial<PlatformQaPreflight>[];
  probe?: unknown;
  evalValue?: unknown;
  failAt?: string;
  cleanClean?: boolean;
}

/** A target that records calls; preflights are consumed in order. */
function fakeTarget(options: FakeOptions = {}) {
  const calls: string[] = [];
  const preflights = [...(options.preflights ?? [{}])];
  const failIf = (name: string) => {
    if (options.failAt === name) {
      throw new QaTargetError("Fake", name, {
        ok: false,
        error: `${name}-broke`,
        detail: "host 100.86.32.44 said no",
      });
    }
  };
  const target: PlatformQaTarget = {
    platform: options.platform ?? "linux",
    preflight() {
      calls.push("preflight");
      failIf("preflight");
      const p = preflights.length > 1 ? preflights.shift()! : preflights[0];
      return Promise.resolve({
        ready: true,
        blockers: [],
        holder: null,
        needsPrepare: false,
        details: {},
        ...p,
      });
    },
    prepare() {
      calls.push("prepare");
      return Promise.resolve({ built: true });
    },
    acquire(taskId) {
      calls.push(`acquire ${taskId}`);
      failIf("acquire");
      return Promise.resolve({ ok: true });
    },
    stage() {
      calls.push("stage");
      failIf("stage");
      return Promise.resolve({ bytes: 3, version: "12.21.1@159.0.1" });
    },
    launch() {
      calls.push("launch");
      failIf("launch");
      return Promise.resolve({ version: "159.0.1" });
    },
    evaluate(_taskId, script) {
      const isProbe = script === PROBE_SCRIPT;
      calls.push(isProbe ? "probe" : "eval");
      failIf(isProbe ? "probe" : "eval");
      return Promise.resolve(
        isProbe
          ? options.probe ?? { os: "Linux", windows: 1, app: "Floorp" }
          : options.evalValue ?? "Floorp",
      );
    },
    screenshot() {
      calls.push("screenshot");
      failIf("screenshot");
      return Promise.resolve(encodeBase64(new Uint8Array([137, 80, 78, 71])));
    },
    cleanup() {
      calls.push("cleanup");
      return Promise.resolve({ clean: options.cleanClean ?? true });
    },
  };
  return { target, calls };
}

function memoryIo(payloadSha = SHA) {
  const files = new Map<string, Uint8Array>();
  const io: PlatformQaIo = {
    sha256File: () => Promise.resolve(payloadSha),
    openFile: () => Promise.resolve(new Blob([new Uint8Array(3)]).stream()),
    writeFile(file, data) {
      files.set(file.replaceAll("\\", "/"), data);
      return Promise.resolve();
    },
    mkdir: () => Promise.resolve(),
    now: () => "2026-11-25T00:00:00.000Z",
  };
  return { io, files };
}

const RUN = {
  taskId: TASK,
  payloadPath: "/p/floorp-linux-x86_64.tar.xz",
  payloadSha256: SHA,
  outDir: "/out",
};

Deno.test("platform QA: a full run passes and cleans up", async () => {
  const { target, calls } = fakeTarget();
  const { io, files } = memoryIo();
  const report = await runPlatformQa(target, {
    ...RUN,
    evalScript: "return 'Floorp';",
    expect: "Floorp",
  }, io);
  assertEquals(report.outcome, "passed");
  assertEquals(report.reason, null);
  assertEquals(calls, [
    "preflight",
    `acquire ${TASK}`,
    "stage",
    "launch",
    "probe",
    "eval",
    "screenshot",
    "cleanup",
  ]);
  assertEquals(report.payload.version, "12.21.1@159.0.1");
  assertEquals(report.cleanupClean, true);
  assert(files.has("/out/linux.png"));
  assert(files.has("/out/linux-report.json"));
  assertEquals(report.screenshot?.file, "linux.png");
});

Deno.test("platform QA: a step failure still cleans up and reads as failed", async () => {
  for (const failAt of ["stage", "launch", "probe", "screenshot"]) {
    const { target, calls } = fakeTarget({ failAt });
    const report = await runPlatformQa(target, RUN, memoryIo().io);
    assertEquals(report.outcome, "failed", failAt);
    assertEquals(report.reason, `${failAt}: ${failAt}-broke`);
    assertEquals(calls.at(-1), "cleanup", failAt);
  }
});

Deno.test("platform QA: an unready or unreachable target is blocked and never acquired", async () => {
  const busy = fakeTarget({
    preflights: [{ ready: false, blockers: ["lock-held"], holder: "other" }],
  });
  const report = await runPlatformQa(busy.target, RUN, memoryIo().io);
  assertEquals(report.outcome, "blocked");
  assertEquals(report.reason, "blocked: lock-held");
  assertEquals(busy.calls, ["preflight"]);
  assertEquals(report.cleanupClean, null);

  const down = fakeTarget({ failAt: "preflight" });
  const downReport = await runPlatformQa(down.target, RUN, memoryIo().io);
  assertEquals(downReport.outcome, "blocked");
  assertEquals(down.calls, ["preflight"]);
});

Deno.test("platform QA: leftovers of this task are removed before a rerun", async () => {
  const { target, calls } = fakeTarget({
    preflights: [
      { ready: false, blockers: ["container-held"], holder: TASK },
      {},
    ],
  });
  const report = await runPlatformQa(target, RUN, memoryIo().io);
  assertEquals(report.outcome, "passed");
  assertEquals(calls.slice(0, 3), ["preflight", "cleanup", "preflight"]);
});

Deno.test("platform QA: a missing image is built, then preflight is repeated", async () => {
  const { target, calls } = fakeTarget({
    preflights: [
      { ready: false, blockers: ["image-missing"], needsPrepare: true },
      {},
    ],
  });
  const report = await runPlatformQa(target, RUN, memoryIo().io);
  assertEquals(report.outcome, "passed");
  assertEquals(calls.slice(0, 3), ["preflight", "prepare", "preflight"]);
});

Deno.test("platform QA: a changed payload fails before the target is touched", async () => {
  const { target, calls } = fakeTarget();
  const report = await runPlatformQa(
    target,
    RUN,
    memoryIo("b".repeat(64)).io,
  );
  assertEquals(report.outcome, "failed");
  assertEquals(report.reason, "payload: payload-sha256-changed");
  assertEquals(calls, []);
});

Deno.test("platform QA: probe, expect and dirty cleanup are failures", async () => {
  const wrongOs = fakeTarget({ probe: { os: "Darwin", windows: 1 } });
  const r1 = await runPlatformQa(wrongOs.target, RUN, memoryIo().io);
  assertEquals(r1.reason, "probe-os-mismatch");
  assertEquals(wrongOs.calls.at(-1), "cleanup");

  const noText = fakeTarget({ evalValue: { title: "New Tab" } });
  const r2 = await runPlatformQa(noText.target, {
    ...RUN,
    evalScript: "return {title: document.title};",
    expect: "Floorp",
  }, memoryIo().io);
  assertEquals(r2.outcome, "failed");
  assertEquals(r2.reason, "eval-expect-not-found");
  assertEquals(r2.eval?.value, { title: "New Tab" });

  const dirty = fakeTarget({ cleanClean: false });
  const r3 = await runPlatformQa(dirty.target, RUN, memoryIo().io);
  assertEquals(r3.outcome, "failed");
  assertEquals(r3.reason, "cleanup: target not clean");
});

Deno.test("platform QA: host names are redacted from the report", async () => {
  const { target } = fakeTarget({ failAt: "launch" });
  const { io, files } = memoryIo();
  const report = await runPlatformQa(target, RUN, io, ["100.86.32.44"]);
  const text = new TextDecoder().decode(files.get("/out/linux-report.json"));
  assert(!text.includes("100.86.32.44"));
  assertStringIncludes(text, "<redacted>");
  assertEquals(report.outcome, "failed");
  assertEquals(redact({ a: "x@h.example" }, ["h.example"]), {
    a: "x@<redacted>",
  });
});

Deno.test("platform QA: helpers", () => {
  assertEquals(payloadKindFor("macos", "/a/Floorp.DMG"), "dmg");
  assertEquals(payloadKindFor("linux", "f.tar.bz2"), "tar.bz2");
  let threw = false;
  try {
    payloadKindFor("macos", "f.tar.xz");
  } catch {
    threw = true;
  }
  assert(threw);
  assertEquals(probeProblems("macos", { os: "Darwin", windows: 2 }), []);
  assertEquals(probeProblems("linux", { os: "Linux", windows: 0 }), [
    "probe-no-browser-window",
  ]);
  assertEquals(probeProblems("linux", null), ["probe-not-object"]);
  assertEquals(evalText("Floorp"), "Floorp");
  assertEquals(evalText({ a: 1 }), '{"a":1}');
});

Deno.test("platform QA CLI: bad arguments exit 2 before anything is read", async () => {
  let reads = 0;
  const deps = {
    readTextFile: () => {
      reads++;
      return Promise.resolve("{}");
    },
  };
  for (
    const argv of [
      ["run", "--platform=windows", "--task-id=t1"],
      ["run", "--platform=linux", "--task-id=t1", "--target=c.json"],
      ["run", "--bogus=1"],
      ["cleanup", "--platform=linux", "--task-id=../x", "--target=c.json"],
      ["explode"],
    ]
  ) {
    const result = await runPlatformQaCli(argv, deps);
    assertEquals(result.exitCode, 2, argv.join(" "));
  }
  assertEquals(reads, 0);
  assertEquals((await runPlatformQaCli(["--help"])).exitCode, 0);
});

Deno.test("platform QA CLI: a changed target config fails without contacting it", async () => {
  let created = 0;
  const result = await runPlatformQaCli([
    "run",
    "--platform=linux",
    `--task-id=${TASK}`,
    "--target=c.json",
    `--target-sha256=${SHA}`,
    "--payload=f.tar.xz",
    `--payload-sha256=${SHA}`,
    "--out=/out",
  ], {
    readTextFile: () => Promise.resolve("{}"),
    createTarget: () => {
      created++;
      throw new Error("unreachable");
    },
  });
  assertEquals(result.exitCode, 1);
  assertStringIncludes(result.stdout, "target config changed");
  assertEquals(created, 0);
});

Deno.test("platform QA CLI: run prints the summary and exits by outcome", async () => {
  const config = "{}";
  const configSha = Array.from(
    new Uint8Array(
      await crypto.subtle.digest("SHA-256", new TextEncoder().encode(config)),
    ),
  ).map((b) => b.toString(16).padStart(2, "0")).join("");
  const argv = [
    "run",
    "--platform=linux",
    `--task-id=${TASK}`,
    "--target=c.json",
    `--target-sha256=${configSha}`,
    "--payload=f.tar.xz",
    `--payload-sha256=${SHA}`,
    "--out=/out",
    "--eval=return 'x';",
    "--expect=Floorp",
  ];
  const run = async (evalValue: unknown) =>
    await runPlatformQaCli(argv, {
      io: memoryIo().io,
      readTextFile: () => Promise.resolve(config),
      createTarget: () =>
        Promise.resolve({
          target: fakeTarget({ evalValue }).target,
          redactions: [],
        }),
    });
  const pass = await run("Floorp 12");
  assertEquals(pass.exitCode, 0);
  const summary = JSON.parse(pass.stdout);
  assertEquals(summary.outcome, "passed");
  assertEquals(summary.eval, { value: "Floorp 12", expect: "Floorp" });
  const fail = await run("nope");
  assertEquals(fail.exitCode, 1);
  assertEquals(JSON.parse(fail.stdout).reason, "eval-expect-not-found");
});
