// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals } from "@std/assert";
import {
  createQaGhTransportForTest,
  QA_GH_LIMITS,
} from "./fdh_qa_gh_transport.ts";
import type {
  QaGhErrorCode,
  QaGhOperation,
  QaGhResult,
  QaGhStatusClass,
  QaGhTestChild,
  QaGhTestDependencies,
} from "./fdh_qa_gh_transport_types.ts";

// Canonical unit suite is synthetic: no run/net/env/read/write grants.
const enc = new TextEncoder();
const OP: QaGhOperation = { kind: "repository" };
const ZIP: QaGhOperation = { kind: "selectedZip", artifactId: 7 };
const HEADER = "HTTP/2.0 200 OK\nContent-Type: application/json\r\n\r\n";
function wire(body = "{}", status = 200) {
  return enc.encode(HEADER.replace("200", String(status)) + body);
}
function deferred<T>() {
  let resolve!: (value: T) => void;
  let reject!: (reason: unknown) => void;
  const promise = new Promise<T>((yes, no) => {
    resolve = yes;
    reject = no;
  });
  return { promise, resolve, reject };
}
interface FixtureOptions {
  out?: unknown[];
  err?: unknown[];
  idle?: boolean;
  code?: number;
  killThrows?: boolean;
  cancelThrows?: boolean;
  cancelGate?: Promise<void>;
  outPull?: () => void;
  errPull?: () => void;
  streamError?: boolean;
  setupError?: boolean;
  streamGetterError?: "stdout" | "stderr";
  rejectStatus?: boolean;
}
function rig(options: FixtureOptions = {}) {
  let time = 0;
  const calls: readonly string[][] = [];
  const mutableCalls = calls as string[][];
  const timers = new Map<number, () => void>();
  const events: string[] = [];
  const children: QaGhTestChild[] = [];
  let timerId = 0;
  const deps: QaGhTestDependencies = {
    now: () => time,
    setTimer: (cb, _ms) => {
      timers.set(++timerId, cb);
      return timerId;
    },
    clearTimer: (id) => {
      timers.delete(id as number);
    },
    spawn: (args) => {
      mutableCalls.push([...args]);
      const done = deferred<{ success: boolean; code: number }>();
      const stream = (name: "out" | "err", chunks: unknown[]) => {
        let index = 0;
        return new ReadableStream<Uint8Array>({
          pull(controller) {
            (name === "out" ? options.outPull : options.errPull)?.();
            if (options.streamError && name === "out") {
              controller.error(new Error("SENSITIVE stream"));
              return;
            }
            if (index < chunks.length) {
              controller.enqueue(chunks[index++] as Uint8Array);
            } else if (!options.idle) controller.close();
          },
          async cancel() {
            events.push(name + "-cancel-start");
            if (options.cancelGate) await options.cancelGate;
            events.push(name + "-cancel-done");
            if (options.cancelThrows) throw new Error("SENSITIVE cancel");
          },
        });
      };
      const stdout = stream("out", options.out ?? [wire()]);
      const stderr = stream("err", options.err ?? []);
      if (options.setupError) {
        Object.defineProperty(stderr, "getReader", {
          value: () => {
            throw new Error("SENSITIVE setup");
          },
        });
      }
      const child: QaGhTestChild = {
        stdout,
        stderr,
        status: done.promise.then((s) => {
          events.push("status");
          return s;
        }),
        kill(signal) {
          assertEquals(signal, "SIGKILL");
          events.push("kill");
          done.resolve({ success: false, code: 1 });
          if (options.killThrows) throw new Error("SENSITIVE kill");
        },
      };
      if (options.streamGetterError) {
        Object.defineProperty(child, options.streamGetterError, {
          get() {
            events.push("getter-error");
            throw new Error("SENSITIVE acquisition");
          },
        });
      }
      if (options.rejectStatus) done.reject(new Error("SENSITIVE status"));
      else if (!options.idle) {
        done.resolve({
          success: (options.code ?? 0) === 0,
          code: options.code ?? 0,
        });
      }
      children.push(child);
      return child;
    },
  };
  const transport = createQaGhTransportForTest(deps);
  return {
    transport,
    deps,
    calls,
    events,
    children,
    timers,
    setTime: (n: number) => {
      time = n;
    },
    tick: (n: number) => {
      time += n;
    },
    fire: () => {
      for (const cb of [...timers.values()]) cb();
    },
  };
}
function error(
  result: QaGhResult,
  code: QaGhErrorCode,
  statusClass: QaGhStatusClass = "none",
) {
  assertEquals(result, { ok: false, code, statusClass });
  assert(!JSON.stringify(result).includes("SENSITIVE"));
}
const endpoints: [QaGhOperation, string][] = [
  [OP, ""],
  [
    { kind: "workflow", workflow: "source" },
    "/actions/workflows/package_and_publish_release.yml",
  ],
  [
    { kind: "workflow", workflow: "qa" },
    "/actions/workflows/validate-final-package-qa.yml",
  ],
  [{ kind: "currentRun", runId: 11 }, "/actions/runs/11"],
  [
    { kind: "attemptRun", runId: 11, attempt: 2 },
    "/actions/runs/11/attempts/2",
  ],
  [
    { kind: "jobsPage", runId: 11, attempt: 2, page: 10 },
    "/actions/runs/11/attempts/2/jobs?per_page=100&page=10",
  ],
  [
    { kind: "artifactsPage", runId: 11, page: 2 },
    "/actions/runs/11/artifacts?per_page=100&page=2",
  ],
  [{ kind: "artifactMetadata", artifactId: 7 }, "/actions/artifacts/7"],
  [ZIP, "/actions/artifacts/7/zip"],
  [
    { kind: "pinnedContents", file: "package", head: "a".repeat(40) },
    "/contents/package.json?ref=" + "a".repeat(40),
  ],
  [
    { kind: "pinnedContents", file: "runtimeLock", head: "b".repeat(40) },
    "/contents/floorp-runtime.lock.json?ref=" + "b".repeat(40),
  ],
  [
    { kind: "pendingDeployments", sourceRunId: 11 },
    "/actions/runs/11/pending_deployments",
  ],
  [{ kind: "releaseTag", version: "12.21.2" }, "/releases/tags/v12.21.2"],
];
for (const [operation, path] of endpoints) {
  Deno.test("canonical GET " + operation.kind + path, async () => {
    const r = rig();
    const result = await r.transport.request(operation);
    assert(result.ok);
    assertEquals(r.calls, [[
      "api",
      "--hostname",
      "github.com",
      "--method",
      "GET",
      "--include",
      "--header",
      "Accept: application/vnd.github+json",
      "--header",
      "X-GitHub-Api-Version: 2022-11-28",
      "repos/Floorp-Projects/Floorp" + path,
    ]]);
    assertEquals(result.publicationAuthorized, false);
    assertEquals(r.timers.size, 0);
    assertEquals(r.children[0].stdout.locked, false);
    assertEquals(r.children[0].stderr.locked, false);
  });
}
const invalid: unknown[] = [
  null,
  {},
  { kind: "api", url: "https://attacker" },
  { kind: "repository", repo: "x" },
  { kind: "repository", args: [] },
  { kind: "repository", binary: "x" },
  { kind: "workflow", workflow: "other" },
  { kind: "currentRun", runId: 0 },
  { kind: "currentRun", runId: -1 },
  { kind: "currentRun", runId: "1" },
  { kind: "currentRun", runId: 1.1 },
  { kind: "currentRun", runId: Number.MAX_SAFE_INTEGER + 1 },
  { kind: "attemptRun", runId: 1, attempt: 0 },
  { kind: "jobsPage", runId: 1, attempt: 1, page: 11 },
  { kind: "artifactsPage", runId: 1, page: 0 },
  { kind: "artifactMetadata", artifactId: NaN },
  { kind: "pinnedContents", file: "../x", head: "a".repeat(40) },
  { kind: "pinnedContents", file: "package", head: "A".repeat(40) },
  { kind: "pinnedContents", file: "package", head: "a".repeat(40) + "&x=y" },
  { kind: "releaseTag", version: "1.2.3/../x" },
  { kind: "releaseTag", version: "--method=POST" },
  { kind: "selectedZip", artifactId: 1, url: "https://signed" },
];
for (const [i, operation] of invalid.entries()) {
  Deno.test("invalid operation before spawn " + i, async () => {
    const r = rig();
    error(
      await r.transport.request(operation as QaGhOperation),
      "invalidOperation",
    );
    assertEquals(r.calls.length, 0);
  });
}
Deno.test("operation accessors rejected without reading, kind descriptor once", async () => {
  let reads = 0;
  const input = {
    kind: "currentRun",
    get runId() {
      reads++;
      return 1;
    },
  };
  const r = rig();
  error(await r.transport.request(input as QaGhOperation), "invalidOperation");
  assertEquals(reads, 0);
  const counts = new Map<PropertyKey, number>();
  const operation = new Proxy({ kind: "currentRun", runId: 1 }, {
    getOwnPropertyDescriptor(target, key) {
      counts.set(key, (counts.get(key) ?? 0) + 1);
      return Reflect.getOwnPropertyDescriptor(target, key);
    },
  });
  assert((await r.transport.request(operation as QaGhOperation)).ok);
  assertEquals(counts.get("kind"), 1);
  assertEquals(counts.get("runId"), 1);
});
Deno.test("operation primitive snapshot survives late mutation", async () => {
  const r = rig();
  const operation: QaGhOperation = { kind: "currentRun", runId: 11 };
  const pending = r.transport.request(operation);
  Object.assign(operation, { runId: 99 });
  assert((await pending).ok);
  assertEquals(
    r.calls[0].at(-1),
    "repos/Floorp-Projects/Floorp/actions/runs/11",
  );
});
for (
  const [status, classification] of [
    [401, "unauthorized"],
    [403, "forbidden"],
    [404, "notFound"],
    [422, "clientError"],
    [500, "serverError"],
    [302, "other"],
  ] as const
) {
  Deno.test("HTTP classification only " + status, async () => {
    const r = rig({
      out: [wire('{"SENSITIVE":"https://signed"}', status)],
      err: [enc.encode("SENSITIVE stderr (HTTP 200)")],
      code: 1,
    });
    error(await r.transport.request(OP), "http", classification);
  });
}
Deno.test("nonzero process with success HTTP is not metadata success", async () => {
  const r = rig({ code: 1 });
  error(await r.transport.request(OP), "process", "success");
});
for (
  const [name, bytes] of [
    ["missing", enc.encode("{}")],
    ["malformed", enc.encode("HTTP/2.0 xyz\n\n{}")],
    ["duplicate", enc.encode("HTTP/2.0 200 OK\nHTTP/2.0 404 NO\n\n{}")],
    [
      "oversized",
      enc.encode(
        "HTTP/2.0 200 OK\nX: " + "a".repeat(QA_GH_LIMITS.headerBytes) +
          "\n\n{}",
      ),
    ],
  ] as const
) {
  Deno.test(
    "header rejects " + name,
    async () =>
      error(await rig({ out: [bytes] }).transport.request(OP), "protocol"),
  );
}
for (
  const header of [
    "HTTP/2.0 200 OK\n\r\n",
    "HTTP/1.1 200 OK\r\nX: y\r\n\r\n",
    "HTTP/2.0 200 OK\nX: y\n\n",
    HEADER,
  ]
) {
  Deno.test("header newline encoding " + JSON.stringify(header), async () => {
    const r = rig({ out: [enc.encode(header + "{}")] });
    assert((await r.transport.request(OP)).ok);
  });
}
for (
  const [name, body] of [
    ["invalidJSON", enc.encode("{no}")],
    ["UTF8", new Uint8Array([0xff])],
    ["depth65", enc.encode("[".repeat(65) + "0" + "]".repeat(65))],
    ["mismatched", enc.encode("{]")],
    ["trailing", enc.encode("{} SENSITIVE")],
  ] as const
) {
  Deno.test("metadata rejects " + name, async () => {
    error(
      await rig({ out: [enc.encode(HEADER), body] }).transport.request(OP),
      "metadata",
    );
  });
}
Deno.test("metadata accepts depth64 and quoted delimiters", async () => {
  const body = "[".repeat(64) + JSON.stringify('quote" " [{}]') +
    "]".repeat(64);
  const result = await rig({ out: [wire(body)] }).transport.request(OP);
  assert(result.ok && result.kind === "metadata");
  assertEquals(result.jsonText, body);
});
Deno.test("header line cap enforced", async () => {
  const r = rig({
    out: [
      enc.encode(
        "HTTP/2.0 200 OK\n" + "X: y\r\n".repeat(QA_GH_LIMITS.headerLines) +
          "\r\n{}",
      ),
    ],
  });
  error(await r.transport.request(OP), "protocol");
});
Deno.test("never scan HTTP-like ZIP body as response headers", async () => {
  const body = enc.encode("HTTP/2.0 404 NO\r\nLocation: SENSITIVE\r\n\r\n");
  const result = await rig({ out: [enc.encode(HEADER), body] }).transport
    .request(ZIP);
  assert(result.ok && result.kind === "zip");
  assertEquals(result.bytes, body);
});
Deno.test("many tiny native chunks accumulate with byte preservation", async () => {
  const bytes = wire(JSON.stringify({ value: "abc".repeat(100) }));
  const result = await rig({
    out: Array.from(bytes, (b) => new Uint8Array([b])),
  }).transport.request(OP);
  assert(result.ok && result.kind === "metadata");
  assertEquals(result.jsonText, JSON.stringify({ value: "abc".repeat(100) }));
});
Deno.test("ZIP bytes stay binary and owned", async () => {
  const body = new Uint8Array([0, 255, 80, 75, 3, 4]);
  const result = await rig({ out: [enc.encode(HEADER), body] }).transport
    .request(ZIP);
  assert(result.ok && result.kind === "zip");
  body.fill(33);
  assertEquals(result.bytes, new Uint8Array([0, 255, 80, 75, 3, 4]));
});
for (const zip of [false, true]) {
  Deno.test("stdout exact wire cap " + zip, async () => {
    const cap = zip ? QA_GH_LIMITS.zipBytes : QA_GH_LIMITS.metadataBytes;
    const body = zip
      ? new Uint8Array(cap - enc.encode(HEADER).length)
      : enc.encode('"' + "a".repeat(cap - enc.encode(HEADER).length - 2) + '"');
    assert(
      (await rig({ out: [enc.encode(HEADER), body] }).transport.request(
        zip ? ZIP : OP,
      )).ok,
    );
  });
  Deno.test("stdout oversized native chunk before copy " + zip, async () => {
    const r = rig({
      out: [
        new Uint8Array(
          (zip ? QA_GH_LIMITS.zipBytes : QA_GH_LIMITS.metadataBytes) + 1,
        ),
      ],
      idle: true,
    });
    error(await r.transport.request(zip ? ZIP : OP), "stdoutLimit");
    assert(r.events.includes("kill"));
    assert(r.events.includes("out-cancel-done"));
    assert(r.events.includes("err-cancel-done"));
    assert(r.events.includes("status"));
  });
}
Deno.test("stderr drained concurrently and exact cap", async () => {
  assert(
    (await rig({ err: [new Uint8Array(QA_GH_LIMITS.stderrBytes)] }).transport
      .request(OP)).ok,
  );
  const r = rig({
    out: [],
    err: [new Uint8Array(QA_GH_LIMITS.stderrBytes + 1)],
    idle: true,
  });
  error(await r.transport.request(OP), "stderrLimit");
  assert(r.events.includes("out-cancel-done") && r.events.includes("status"));
});
for (const zip of [false, true]) {
  Deno.test(
    "aggregate wire budget persists across requests " + zip,
    async () => {
      const cap = zip ? QA_GH_LIMITS.zipBytes : QA_GH_LIMITS.metadataBytes;
      const body = zip
        ? new Uint8Array(cap - enc.encode(HEADER).length)
        : enc.encode(
          '"' + "a".repeat(cap - enc.encode(HEADER).length - 2) + '"',
        );
      const r = rig({ out: [enc.encode(HEADER), body] });
      const count = (zip
        ? QA_GH_LIMITS.zipSessionBytes
        : QA_GH_LIMITS.metadataSessionBytes) / cap;
      for (let i = 0; i < count; i++) {
        assert((await r.transport.request(zip ? ZIP : OP)).ok);
      }
      error(await r.transport.request(zip ? ZIP : OP), "sessionLimit");
    },
  );
}
Deno.test("failed HTTP stdout still charges session", async () => {
  const body = new Uint8Array(
    QA_GH_LIMITS.metadataBytes - enc.encode(HEADER).length,
  );
  const r = rig({
    out: [enc.encode(HEADER.replace("200", "404")), body],
    code: 1,
  });
  for (let i = 0; i < 8; i++) {
    error(await r.transport.request(OP), "http", "notFound");
  }
  error(await r.transport.request(OP), "sessionLimit");
});
Deno.test("native Proxy chunk rejected with no get/prototype traps", async () => {
  let traps = 0;
  const proxy = new Proxy(new Uint8Array(1), {
    get() {
      traps++;
      throw Error();
    },
    getPrototypeOf() {
      traps++;
      throw Error();
    },
  });
  error(
    await rig({ out: [proxy], idle: true }).transport.request(OP),
    "stream",
  );
  assertEquals(traps, 0);
});
for (
  const value of [
    new Uint16Array(1),
    new Uint8ClampedArray(1),
    new DataView(new ArrayBuffer(1)),
    {},
    new Uint8Array(new SharedArrayBuffer(1)),
    new Uint8Array(new ArrayBuffer(1, { maxByteLength: 2 })),
  ]
) {
  Deno.test(
    "reject nonordinary native byte chunk " +
      Object.prototype.toString.call(value),
    async () => {
      error(
        await rig({ out: [value], idle: true }).transport.request(OP),
        "stream",
      );
    },
  );
}
Deno.test("own size/buffer/iterator spoof ignored, real size cap", async () => {
  const value = new Uint8Array(QA_GH_LIMITS.metadataBytes + 1);
  let reads = 0;
  for (
    const key of [
      "length",
      "byteLength",
      "buffer",
      Symbol.iterator,
      Symbol.toStringTag,
    ]
  ) {
    Object.defineProperty(value, key, {
      get() {
        reads++;
        throw Error();
      },
    });
  }
  error(
    await rig({ out: [value], idle: true }).transport.request(OP),
    "stdoutLimit",
  );
  assertEquals(reads, 0);
});
Deno.test("native byteOffset honored without getters", async () => {
  const bytes = enc.encode("XX" + HEADER + "{}YY");
  const chunk = bytes.subarray(2, bytes.length - 2);
  assert((await rig({ out: [chunk] }).transport.request(OP)).ok);
});
Deno.test("busy prevents second child and abort exit1 keeps cause", async () => {
  const r = rig({ idle: true, out: [] });
  const abort = new AbortController();
  const pending = r.transport.request(OP, abort.signal);
  error(await r.transport.request(OP), "busy");
  assertEquals(r.calls.length, 1);
  abort.abort();
  error(await pending, "aborted");
  assertEquals(r.events.filter((x) => x === "kill").length, 1);
});
Deno.test("pre-aborted and expired sessions do not spawn", async () => {
  const r = rig();
  const abort = new AbortController();
  abort.abort();
  error(await r.transport.request(OP, abort.signal), "aborted");
  r.setTime(QA_GH_LIMITS.sessionMs);
  error(await r.transport.request(OP), "sessionTimeout");
  assertEquals(r.calls.length, 0);
});
for (const zip of [false, true]) {
  Deno.test("idle deadline timer " + zip, async () => {
    const r = rig({ idle: true, out: [] });
    const pending = r.transport.request(zip ? ZIP : OP);
    r.setTime(zip ? QA_GH_LIMITS.zipMs : QA_GH_LIMITS.metadataMs);
    r.fire();
    error(await pending, "timeout");
    assert(
      r.events.includes("status") && r.events.includes("out-cancel-done") &&
        r.events.includes("err-cancel-done"),
    );
  });
}
Deno.test("session deadline shared not renewed", async () => {
  const r = rig({ idle: true, out: [] });
  r.setTime(QA_GH_LIMITS.sessionMs - 10);
  const pending = r.transport.request(OP);
  r.setTime(QA_GH_LIMITS.sessionMs);
  r.fire();
  error(await pending, "sessionTimeout");
});
Deno.test("zero-byte microtask stream checks deadline without timer firing", async () => {
  const r = rig({
    idle: true,
    out: Array.from({ length: 100 }, () => new Uint8Array()),
    outPull: () => r.tick(1000),
  });
  error(await r.transport.request(OP), "timeout");
  assert(r.events.includes("status"));
});
Deno.test("monotonic clock regression kills and settles", async () => {
  const r = rig({
    idle: true,
    out: [new Uint8Array()],
    outPull: () => r.setTime(-1),
  });
  error(await r.transport.request(OP), "clock");
  assert(r.events.includes("status"));
});
Deno.test("cleanup awaits cancel settlement despite kill/cancel errors", async () => {
  const gate = deferred<void>();
  const r = rig({
    idle: true,
    out: [],
    killThrows: true,
    cancelThrows: true,
    cancelGate: gate.promise,
  });
  const abort = new AbortController();
  let settled = false;
  const pending = r.transport.request(OP, abort.signal).then((result) => {
    settled = true;
    return result;
  });
  abort.abort();
  await Promise.resolve();
  await Promise.resolve();
  assertEquals(settled, false);
  gate.resolve();
  error(await pending, "aborted");
  assert(
    r.events.includes("out-cancel-done") &&
      r.events.includes("err-cancel-done"),
  );
});
for (const mode of ["setupError", "streamError", "rejectStatus"] as const) {
  Deno.test("failure settlement " + mode, async () => {
    const r = rig({ idle: true, out: [], [mode]: true });
    error(
      await r.transport.request(OP),
      mode === "rejectStatus" ? "process" : "stream",
    );
    assert(r.events.includes("kill"));
    assertEquals(r.children[0].stdout.locked, false);
    assertEquals(r.children[0].stderr.locked, false);
    assert(r.events.includes("err-cancel-done"));
  });
}
for (const pipe of ["stdout", "stderr"] as const) {
  Deno.test("throwing stream getter cleanup " + pipe, async () => {
    const r = rig({ idle: true, out: [], streamGetterError: pipe });
    error(await r.transport.request(OP), "stream");
    assertEquals(
      r.events.filter((x) => x === "getter-error").length,
      1,
    );
    assertEquals(r.events.filter((x) => x === "kill").length, 1);
    assert(r.events.includes("status"));
    if (pipe === "stderr") assert(r.events.includes("out-cancel-done"));
  });
}
Deno.test("late metadata deadline does not recancel settled child work", async () => {
  const baseline = rig();
  let totalChecks = 0;
  baseline.deps.now = () => {
    totalChecks++;
    return 0;
  };
  assert((await baseline.transport.request(OP)).ok);
  const r = rig();
  let checks = 0;
  r.deps.now = () => ++checks >= totalChecks ? QA_GH_LIMITS.metadataMs : 0;
  error(await r.transport.request(OP), "timeout");
  assertEquals(
    r.events.filter((x) => x === "kill" || x.includes("cancel")).length,
    0,
  );
  assert(r.events.includes("status"));
  assertEquals(r.children[0].stdout.locked, false);
  assertEquals(r.children[0].stderr.locked, false);
});
Deno.test("spawn rejection returns fixed safe error and releases busy", async () => {
  const r = rig();
  r.deps.spawn = () => {
    throw new Error("SENSITIVE signed URL");
  };
  error(await r.transport.request(OP), "spawn");
  error(await r.transport.request(OP), "spawn");
});
