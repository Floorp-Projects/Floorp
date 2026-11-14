// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals } from "@std/assert";
import { runQaCli } from "./fdh_qa_cli.ts";
import type {
  QaCollectionReport,
  QaCollectorOptions,
} from "./fdh_final_package_qa_collector_types.ts";

const options: QaCollectorOptions = {
  sourceRunId: 123,
  sourceAttempt: 1,
  sourceHead: "a".repeat(40),
  qaRunId: 456,
  qaAttempt: 2,
  qaHead: "b".repeat(40),
  windowsPolicy: "require-success",
  maxAgeMs: 86_400_000,
};
const pairs = [
  ["source-run-id", "123"],
  ["source-attempt", "1"],
  ["source-head", options.sourceHead],
  ["qa-run-id", "456"],
  ["qa-attempt", "2"],
  ["qa-head", options.qaHead],
  ["windows-policy", "require-success"],
  ["max-age-ms", "86400000"],
];
function argv(equal = false): string[] {
  return [
    "qa",
    "collect",
    ...pairs.flatMap(([k, v]) => equal ? ["--" + k + "=" + v] : ["--" + k, v]),
  ];
}
function changed(key: string, value: string): string[] {
  return [
    "qa",
    "collect",
    ...pairs.map(([k, v]) => "--" + k + "=" + (k === key ? value : v)),
  ];
}
const scope = {
  scope: "read-only-point-in-time-qa-collection",
  transportAuthenticityVerified: false,
  apiAuthenticityVerified: false,
  nativeQaVerified: false,
  liveApprovalVerified: false,
  publicationAuthorized: false,
} as const;
function matched(o: QaCollectorOptions): QaCollectionReport {
  return {
    ...scope,
    matches: true,
    sourceRunId: o.sourceRunId,
    sourceAttempt: o.sourceAttempt,
    qaRunId: o.qaRunId,
    qaAttempt: o.qaAttempt,
    checkedAt: "2026-11-14T00:00:00.000Z",
    archiveBytesVerified: true,
  };
}
function safeJson(text: string): Record<string, unknown> {
  assert(new TextEncoder().encode(text + "\n").length <= 256 * 1024);
  assert(!text.includes("SECRET"));
  const summary = JSON.parse(text);
  for (
    const key of [
      "transportAuthenticityVerified",
      "apiAuthenticityVerified",
      "nativeQaVerified",
      "liveApprovalVerified",
      "publicationAuthorized",
    ]
  ) assertEquals(summary[key], false);
  return summary;
}
async function invalid(args: string[]): Promise<void> {
  let calls = 0;
  const result = await runQaCli(args, {
    collect: () => {
      calls++;
      throw new Error("SECRET");
    },
  });
  assertEquals(calls, 0);
  assertEquals(result.exitCode, 2);
  assertEquals(safeJson(result.stdout).code, "invalidArguments");
  // Default route must also reject without loading collector/transport, no grants.
  assertEquals(await runQaCli(args), result);
}
for (const equal of [false, true]) {
  Deno.test(
    "QA CLI accepts canonical " + (equal ? "equals" : "space") + " form",
    async () => {
      let calls = 0;
      const result = await runQaCli(argv(equal), {
        collect: (o) => {
          calls++;
          assertEquals(o, options);
          assert(Object.isFrozen(o));
          return Promise.resolve(matched(o));
        },
      });
      assertEquals(calls, 1);
      assertEquals(result.exitCode, 0);
      assertEquals(safeJson(result.stdout).matches, true);
    },
  );
}
Deno.test("QA CLI accepts mixed forms, maximum safe IDs and shadow policy", async () => {
  const args = changed("source-run-id", "9007199254740991");
  args.splice(3, 1, "--source-attempt", "1");
  args[args.length - 2] = "--windows-policy=shadow-observation";
  const result = await runQaCli(args, {
    collect: (o) => {
      assertEquals(o.sourceRunId, Number.MAX_SAFE_INTEGER);
      assertEquals(o.windowsPolicy, "shadow-observation");
      return Promise.resolve(matched(o));
    },
  });
  assertEquals(result.exitCode, 0);
  safeJson(result.stdout);
});
for (
  const args of [["qa", "--help"], ["qa", "-h"], ["qa", "help"], [
    "qa",
    "collect",
    "--help",
  ], ["qa", "collect", "-h"]]
) {
  Deno.test("QA CLI help no collector " + args.join(" "), async () => {
    let calls = 0;
    const result = await runQaCli(args, {
      collect: () => {
        calls++;
        throw new Error("SECRET");
      },
    });
    assertEquals(calls, 0);
    assertEquals(result.exitCode, 0);
    assert(result.stdout.startsWith("Usage:"));
    assertEquals(await runQaCli(args), result);
  });
}
for (const [key] of pairs) {
  Deno.test(
    "QA CLI rejects missing option " + key,
    () =>
      invalid([
        "qa",
        "collect",
        ...pairs.filter(([k]) => k !== key).map(([k, v]) => "--" + k + "=" + v),
      ]),
  );
  for (const form of ["same", "mixed", "reverse"]) {
    Deno.test("QA CLI rejects duplicate " + key + " " + form, () =>
      invalid([
        ...argv(form !== "reverse"),
        ...(form === "same" ? ["--" + key + "=1"] : ["--" + key, "1"]),
      ]));
  }
}
for (
  const key of [
    "source-run-id",
    "source-attempt",
    "qa-run-id",
    "qa-attempt",
    "max-age-ms",
  ]
) {
  for (
    const value of [
      "0",
      "-1",
      "+1",
      "01",
      "1.0",
      "1e2",
      "0x10",
      " 1",
      "1 ",
      "1\n",
      "NaN",
      "Infinity",
      "9007199254740992",
      "99999999999999999",
      "1_000",
      "",
    ]
  ) {
    Deno.test(
      "QA CLI rejects noncanonical number " + key + " " + JSON.stringify(value),
      () => invalid(changed(key, value)),
    );
  }
}
for (const key of ["source-head", "qa-head"]) {
  for (
    const value of [
      "a".repeat(39),
      "a".repeat(41),
      "A".repeat(40),
      "g".repeat(40),
      "a".repeat(40) + "\n",
      "SECRET",
      "",
    ]
  ) {
    Deno.test(
      "QA CLI rejects invalid head " + key + " " + JSON.stringify(value),
      () => invalid(changed(key, value)),
    );
  }
}
for (
  const key of [
    "repo",
    "host",
    "url",
    "token",
    "raw-token",
    "gh-executable",
    "executable",
    "transport",
    "now",
    "test-deps",
    "header",
    "out",
    "json",
  ]
) {
  Deno.test(
    "QA CLI rejects public override " + key,
    () => invalid([...argv(true), "--" + key + "=SECRET"]),
  );
}
for (
  const args of [
    [],
    ["qa"],
    ["qa", "collect"],
    ["qa", "bogus"],
    ...[
      ["--help"],
      ["-h"],
      ["--"],
      ["extra"],
      ["--source-run-id"],
      ["--unknown=SECRET"],
    ].map((extra) => [...argv(true), ...extra]),
    changed("max-age-ms", "86400001"),
    changed("windows-policy", "default"),
    changed("qa-run-id", "123"),
  ]
) {
  Deno.test(
    "QA CLI rejects malformed route " + JSON.stringify(args),
    () => invalid(args),
  );
}
Deno.test("QA CLI reports not ready and fixed failure codes", async () => {
  const result = await runQaCli(argv(), {
    collect: () =>
      Promise.resolve({
        ...scope,
        matches: false,
        stage: "eligibility",
        code: "sourceNotReady",
      }),
  });
  assertEquals(result.exitCode, 1);
  assertEquals(safeJson(result.stdout).code, "sourceNotReady");
});
Deno.test("QA CLI never forwards errors, unknown codes, authority or identity substitutions", async () => {
  const good = matched(options);
  const cases: unknown[] = [
    null,
    {},
    { ...good, publicationAuthorized: true },
    { ...good, sourceRunId: 999 },
    { ...good, matches: "SECRET" },
    { ...good, archiveBytesVerified: false },
    { ...scope, matches: false, stage: "SECRET", code: "sourceNotReady" },
    { ...scope, matches: false, stage: "identity", code: "SECRET" },
    new Proxy(good, {
      get() {
        throw new Error("SECRET");
      },
    }),
  ];
  for (const report of cases) {
    const result = await runQaCli(argv(), {
      collect: () => Promise.resolve(report as QaCollectionReport),
    });
    assertEquals(result.exitCode, 1);
    assertEquals(safeJson(result.stdout).code, "collectionFailed");
  }
  const thrown = await runQaCli(argv(), {
    collect: () => Promise.reject(new Error("SECRET https://signed.invalid/")),
  });
  assertEquals(thrown.exitCode, 1);
  assertEquals(safeJson(thrown.stdout).code, "collectionFailed");
});
Deno.test("QA CLI snapshots failure stage and code exactly once", async () => {
  let stages = 0;
  let codes = 0;
  const report = {
    ...scope,
    matches: false as const,
    get stage() {
      return ++stages === 1 ? "eligibility" : "SECRET";
    },
    get code() {
      return ++codes === 1 ? "sourceNotReady" : "SECRET";
    },
  };
  const result = await runQaCli(argv(), {
    collect: () => Promise.resolve(report as QaCollectionReport),
  });
  assertEquals(result.exitCode, 1);
  assertEquals(safeJson(result.stdout).code, "sourceNotReady");
  assertEquals(stages, 1);
  assertEquals(codes, 1);
});
Deno.test("QA CLI projection ignores arbitrary report payload and throwing toJSON", async () => {
  const report = {
    ...matched(options),
    checkedAt: "SECRET".repeat(100000),
    raw: "SECRET",
    toJSON() {
      throw new Error("SECRET");
    },
  };
  const result = await runQaCli(argv(), {
    collect: () => Promise.resolve(report),
  });
  assertEquals(result.exitCode, 0);
  safeJson(result.stdout);
});
