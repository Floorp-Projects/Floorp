// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals } from "@std/assert";
import { runQaDiscoveryCli } from "./fdh_qa_discovery_cli.ts";
import { runQaCli } from "./fdh_qa_cli.ts";
import type {
  QaDiscoveryOptions,
  QaDiscoveryReport,
} from "./fdh_qa_discovery_types.ts";
const options: QaDiscoveryOptions = {
  sourceRunId: 123,
  sourceAttempt: 1,
  sourceHead: "a".repeat(40),
  windowsPolicy: "shadow-observation",
  maxAgeMs: 60000,
};
const pairs = [
  ["source-run-id", "123"],
  ["source-attempt", "1"],
  ["source-head", options.sourceHead],
  ["windows-policy", "shadow-observation"],
  ["max-age-ms", "60000"],
];
function argv(equals = true): string[] {
  return [
    "qa",
    "discover",
    ...pairs.flatMap(([k, v]) => equals ? ["--" + k + "=" + v] : ["--" + k, v]),
  ];
}
function matched(o: QaDiscoveryOptions): QaDiscoveryReport {
  return {
    scope: "read-only-point-in-time-qa-discovery",
    transportAuthenticityVerified: false,
    apiAuthenticityVerified: false,
    nativeQaVerified: false,
    liveApprovalVerified: false,
    publicationAuthorized: false,
    matches: true,
    sourceRunId: o.sourceRunId,
    sourceAttempt: o.sourceAttempt,
    sourceHead: o.sourceHead,
    qaRunId: 456,
    qaAttempt: 2,
    qaHead: "b".repeat(40),
    checkedAt: new Date().toISOString(),
    archiveBytesVerified: true,
    runsListed: 1,
    pagesFetched: 1,
    candidatesChecked: 1,
  };
}
function safe(text: string): Record<string, unknown> {
  assert(!text.includes("SECRET"));
  const parsed = JSON.parse(text);
  for (
    const k of [
      "transportAuthenticityVerified",
      "apiAuthenticityVerified",
      "nativeQaVerified",
      "liveApprovalVerified",
      "publicationAuthorized",
    ]
  ) assertEquals(parsed[k], false);
  return parsed;
}
for (const equals of [true, false]) {
  Deno.test("QA discovery CLI canonical " + equals, async () => {
    let calls = 0;
    const r = await runQaDiscoveryCli(argv(equals), {
      discover: (o) => {
        calls++;
        assertEquals(o, options);
        assert(Object.isFrozen(o));
        return Promise.resolve(matched(o));
      },
    });
    assertEquals(calls, 1);
    assertEquals(r.exitCode, 0);
    assertEquals(safe(r.stdout).qaHead, "b".repeat(40));
  });
}
Deno.test("QA discovery route delegates only discovery test seam", async () => {
  const r = await runQaCli(argv(), {
    collect: () => {
      throw Error("SECRET must not collect");
    },
    discover: (o) => Promise.resolve(matched(o)),
  });
  assertEquals(r.exitCode, 0);
  safe(r.stdout);
});
for (const help of ["--help", "-h"]) {
  Deno.test("QA discovery help no IO " + help, async () => {
    const r = await runQaDiscoveryCli(["qa", "discover", help]);
    assertEquals(r.exitCode, 0);
    assert(r.stdout.startsWith("Usage:"));
    assertEquals(await runQaCli(["qa", "discover", help]), r);
  });
}
const badArgs = [
  [],
  ["qa", "discover"],
  ...pairs.map(([key]) =>
    argv().filter((v) => !v.startsWith("--" + key + "="))
  ),
  ...[
    "qa-run-id=456",
    "qa-head=" + "b".repeat(40),
    "since=2026-11-14T00:00:00.000Z",
    "source-created-at=1",
    "token=SECRET",
    "url=SECRET",
    "clock=1",
    "transport=SECRET",
  ].map((v) => [...argv(), "--" + v]),
  ...["0", "01", "+1", "1e2", "1.0", "9007199254740992", "SECRET"].map((v) =>
    argv().map((a) =>
      a.startsWith("--source-run-id=") ? "--source-run-id=" + v : a
    )
  ),
  argv().map((a) =>
    a.startsWith("--source-head=") ? "--source-head=" + "A".repeat(40) : a
  ),
  argv().map((a) =>
    a.startsWith("--max-age-ms=") ? "--max-age-ms=86400001" : a
  ),
  argv().map((a) =>
    a.startsWith("--windows-policy=") ? "--windows-policy=ignore" : a
  ),
  [...argv(), "--source-attempt", "1"],
  [...argv(), "trailing"],
  [...argv(), "--help"],
];
for (const [index, args] of badArgs.entries()) {
  Deno.test("QA discovery invalid no IO " + index, async () => {
    let calls = 0;
    const r = await runQaDiscoveryCli(args, {
      discover: () => {
        calls++;
        throw Error("SECRET");
      },
    });
    assertEquals(calls, 0);
    assertEquals(r.exitCode, 2);
    assertEquals(safe(r.stdout).code, "invalidArguments");
    assertEquals(await runQaDiscoveryCli(args), r);
  });
}
for (
  const [key, value] of Object.entries({
    publicationAuthorized: true,
    nativeQaVerified: true,
    apiAuthenticityVerified: true,
    transportAuthenticityVerified: true,
    liveApprovalVerified: true,
    scope: "SECRET",
    sourceRunId: 999,
    sourceAttempt: 2,
    sourceHead: "c".repeat(40),
    qaRunId: 123,
    qaAttempt: 0,
    qaHead: "SECRET",
    archiveBytesVerified: false,
    checkedAt: "SECRET",
    runsListed: 301,
    pagesFetched: 4,
    candidatesChecked: 9,
  })
) {
  Deno.test("QA discovery projection rejects " + key, async () => {
    const r = await runQaDiscoveryCli(argv(), {
      discover: (o) =>
        Promise.resolve({ ...matched(o), [key]: value } as QaDiscoveryReport),
    });
    assertEquals(r.exitCode, 1);
    assertEquals(safe(r.stdout).code, "discoveryFailed");
  });
}
for (
  const checkedAt of [
    new Date(Date.now() + 3600000).toISOString(),
    new Date(Date.now() - 3600000).toISOString(),
  ]
) {
  Deno.test("QA discovery timestamp failclosed " + checkedAt, async () => {
    const r = await runQaDiscoveryCli(argv(), {
      discover: (o) => Promise.resolve({ ...matched(o), checkedAt }),
    });
    assertEquals(r.exitCode, 1);
    safe(r.stdout);
  });
}
Deno.test("QA discovery fixed failure or exception cannot disclose data", async () => {
  const r = await runQaDiscoveryCli(argv(), {
    discover: () => {
      throw Error("SECRET");
    },
  });
  assertEquals(r.exitCode, 1);
  safe(r.stdout);
  const failure = await runQaDiscoveryCli(argv(), {
    discover: () =>
      Promise.resolve({
        scope: "read-only-point-in-time-qa-discovery",
        transportAuthenticityVerified: false,
        apiAuthenticityVerified: false,
        nativeQaVerified: false,
        liveApprovalVerified: false,
        publicationAuthorized: false,
        matches: false,
        stage: "candidates",
        code: "ambiguous",
      }),
  });
  assertEquals(failure.exitCode, 1);
  assertEquals(safe(failure.stdout).code, "ambiguous");
});
