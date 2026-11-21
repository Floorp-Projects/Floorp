import {
  assertOwnedBrowserRoot,
  assertOwnedMarionetteListener,
  assertProcessRecoveryPlatform,
  matchesDurableBoundary,
  parseBrowserDriverResult,
  parseDurableMarker,
  parseLinuxProcStat,
  resolveExpectedRuntimeBuildId,
  selectOwnedProcessTree,
  signalVerifiedOwnedProcessTree,
  waitForDurableMarker,
} from "./hotfix_process_recovery.ts";
import type { LinuxProcessIdentity } from "./hotfix_process_recovery_types.ts";

function assert(condition: boolean, message: string): void {
  if (!condition) throw new Error(message);
}

async function assertRejects(
  action: () => Promise<unknown>,
  message: string,
): Promise<void> {
  try {
    await action();
  } catch {
    return;
  }
  throw new Error(message);
}

function identity(
  pid: number,
  sessionId: number,
  startTimeTicks = String(pid * 10),
): LinuxProcessIdentity {
  return {
    pid,
    parentPid: pid === sessionId ? 1 : sessionId,
    processGroupId: sessionId,
    sessionId,
    startTimeTicks,
    executable: "/usr/bin/floorp-bin",
    commandLine: ["/usr/bin/floorp-bin", "--profile", "/private/profile"],
  };
}

Deno.test("Linux proc stat parsing preserves session and start identity", () => {
  const fields = ["S", "1", "10", "10", ...Array(15).fill("0"), "12345"];
  const parsed = parseLinuxProcStat(
    10,
    `10 (floorp worker) ${fields.join(" ")}`,
  );
  assert(parsed.parentPid === 1, "parent PID parsed");
  assert(parsed.processGroupId === 10, "process group parsed");
  assert(parsed.sessionId === 10, "session parsed");
  assert(parsed.startTimeTicks === "12345", "start time parsed");
  const idle = parseLinuxProcStat(
    1,
    "1 (init) S 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
  );
  assert(
    idle.sessionId === 0 && idle.processGroupId === 0,
    "zero session allowed",
  );
});

Deno.test("process recovery rejects unsupported platforms explicitly", () => {
  let rejected = false;
  try {
    assertProcessRecoveryPlatform("darwin");
  } catch (error) {
    rejected = error instanceof Error && error.message.includes("Linux only");
  }
  assert(rejected, "unsupported platform must report a clear error");
});

Deno.test("owned browser validation fails closed on reuse or profile mismatch", () => {
  const expected = identity(10, 10);
  assertOwnedBrowserRoot(
    expected,
    expected,
    "/usr/bin/floorp-bin",
    "/private/profile",
  );
  for (
    const current of [
      { ...expected, startTimeTicks: "999" },
      { ...expected, executable: "/usr/bin/other" },
      { ...expected, processGroupId: 99 },
      {
        ...expected,
        commandLine: ["/usr/bin/floorp-bin", "--profile", "/foreign"],
      },
    ]
  ) {
    let rejected = false;
    try {
      assertOwnedBrowserRoot(
        expected,
        current,
        "/usr/bin/floorp-bin",
        "/private/profile",
      );
    } catch {
      rejected = true;
    }
    assert(rejected, "mismatched identity must not be signaled");
  }
});

Deno.test("owned process selection retains changed-session descendants", () => {
  const root = identity(10, 10);
  const child = { ...identity(11, 11), parentPid: 10, processGroupId: 11 };
  const grandchild = {
    ...identity(12, 12),
    parentPid: 11,
    processGroupId: 12,
  };
  const foreign = identity(20, 20);
  const selected = selectOwnedProcessTree(root, [
    root,
    child,
    grandchild,
    foreign,
  ]);
  assert(selected.length === 3, "changed-session descendants retained");
  assert(selected.some((item) => item.pid === child.pid), "child retained");
  assert(
    selected.some((item) => item.pid === grandchild.pid),
    "grandchild retained",
  );
});

Deno.test("owned kill emits no signal after PID reuse or identity mismatch", async () => {
  const root = identity(10, 10);
  const child = { ...identity(11, 11), parentPid: 10, processGroupId: 11 };
  const cases: Array<{
    previouslyOwned: LinuxProcessIdentity[];
    current: LinuxProcessIdentity[];
  }> = [
    { previouslyOwned: [root], current: [{ ...root, startTimeTicks: "999" }] },
    {
      previouslyOwned: [root, child],
      current: [root, { ...child, executable: "/usr/bin/foreign" }],
    },
  ];
  for (const { previouslyOwned, current } of cases) {
    let signals = 0;
    await assertRejects(
      () =>
        signalVerifiedOwnedProcessTree({
          root,
          profilePath: "/private/profile",
          previouslyOwned: previouslyOwned as LinuxProcessIdentity[],
          dependencies: {
            listProcesses: () => Promise.resolve([...current]),
            readProcess: (pid) =>
              Promise.resolve(current.find((item) => item.pid === pid) ?? null),
            signal: () => signals++,
          },
        }),
      "identity mismatch must fail closed",
    );
    assert(signals === 0, "no signal emitted before all identities validate");
  }
});

Deno.test("Marionette listener rejects foreign and ambiguous owners", () => {
  const root = identity(10, 10);
  for (const owners of [[identity(20, 20)], [root, identity(20, 20)]]) {
    let rejected = false;
    try {
      assertOwnedMarionetteListener(root, owners);
    } catch {
      rejected = true;
    }
    assert(rejected, "foreign or multiple owners rejected");
  }
});

Deno.test("durable boundary predicates distinguish operation and attempt phase", () => {
  const update = {
    desiredEnabled: true,
    pending: {
      op: "update",
      phase: "PREPARED",
      targetVersion: "v2",
      targetDigest: "digest-v2",
    },
    attempt: { attemptedDigests: ["digest-v1"] },
  };
  assert(matchesDurableBoundary("PREPARED", update), "update PREPARED matches");
  assert(
    !matchesDurableBoundary("OLD_STOPPED", update),
    "wrong phase rejected",
  );
  const fallback = {
    pending: {
      op: "update",
      phase: "OLD_STOPPED",
      targetVersion: "v1",
      targetDigest: "digest-v1",
    },
    attempt: { attemptedDigests: ["digest-v2"] },
  };
  assert(
    matchesDurableBoundary("fallback_pending_before_mark", fallback),
    "unattempted fallback matches",
  );
  assert(
    !matchesDurableBoundary("fallback_attempt_marked", fallback),
    "unattempted fallback is not marked",
  );
  assert(
    matchesDurableBoundary("fallback_attempt_marked", {
      ...fallback,
      attempt: { attemptedDigests: ["digest-v2", "digest-v1"] },
    }),
    "marked fallback matches",
  );
  assert(
    !matchesDurableBoundary("committed_pending_publish", {
      lastOutcome: {
        status: "committed_pending_publish",
        request: { op: "enable", target: null },
      },
    }),
    "enable commit is not an update commit boundary",
  );
  assert(
    matchesDurableBoundary("committed_pending_publish", {
      lastOutcome: {
        status: "committed_pending_publish",
        request: { op: "update", target: "v2" },
      },
    }),
    "committed update boundary matches its own transaction",
  );
});

Deno.test("durable marker parsing binds current launch and boundary", () => {
  const marker = {
    schemaVersion: 1,
    runNonce: "nonce",
    launchId: 2,
    boundary: "PREPARED",
    evaluationCount: 0,
    snapshot: { stateRevision: 2 },
  };
  assert(
    parseDurableMarker(JSON.stringify(marker), "nonce", 2, "PREPARED")
      .launchId === 2,
    "valid marker parsed",
  );
  for (
    const raw of [
      "{",
      JSON.stringify({ ...marker, runNonce: "old" }),
      JSON.stringify({ ...marker, launchId: 0 }),
      JSON.stringify({ ...marker, evaluationCount: -1 }),
      JSON.stringify({ ...marker, snapshot: [] }),
    ]
  ) {
    let rejected = false;
    try {
      parseDurableMarker(raw, "nonce", 2, "PREPARED");
    } catch {
      rejected = true;
    }
    assert(rejected, "malformed or stale marker rejected");
  }
  for (
    const [launchId, boundary] of [[3, "PREPARED"], [2, "OLD_STOPPED"]] as const
  ) {
    let rejected = false;
    try {
      parseDurableMarker(JSON.stringify(marker), "nonce", launchId, boundary);
    } catch {
      rejected = true;
    }
    assert(rejected, "replayed launch or boundary rejected");
  }
});

Deno.test("driver result binds nonce launch and BuildID", () => {
  const result = {
    schemaVersion: 1,
    runNonce: "nonce",
    launchId: 2,
    buildId: "20261112140843",
    evaluationCount: 0,
    evaluationCountBeforeRollback: 0,
    diagnostics: {},
    journal: {},
    outcomeBeforeRollback: null,
    rollbackResult: null,
    postRollbackJournal: null,
  };
  const expected = { runNonce: "nonce", launchId: 2, buildId: result.buildId };
  assert(
    parseBrowserDriverResult(JSON.stringify(result), expected).launchId === 2,
    "valid driver result parsed",
  );
  for (
    const altered of [
      { ...result, runNonce: "old" },
      { ...result, launchId: 1 },
      { ...result, buildId: "20260101000000" },
      { ...result, evaluationCount: -1 },
    ]
  ) {
    let rejected = false;
    try {
      parseBrowserDriverResult(JSON.stringify(altered), expected);
    } catch {
      rejected = true;
    }
    assert(rejected, "malformed or replayed result rejected");
  }
});

Deno.test("runtime BuildID is bound to source and installed locks", () => {
  const encoder = new TextEncoder();
  const source = encoder.encode(JSON.stringify({
    artifacts: [{
      platform: "linux",
      architecture: "x86_64",
      buildId: "20261112140843",
    }],
  }));
  const installed = encoder.encode(JSON.stringify({
    platform: "linux",
    architecture: "x86_64",
    buildId: "20261112140843",
  }));
  assert(
    resolveExpectedRuntimeBuildId(source, installed) === "20261112140843",
    "BuildID read from lock metadata",
  );
  let rejected = false;
  try {
    resolveExpectedRuntimeBuildId(
      source,
      encoder.encode(JSON.stringify({
        platform: "linux",
        architecture: "x86_64",
        buildId: "20260101000000",
      })),
    );
  } catch {
    rejected = true;
  }
  assert(rejected, "mismatched installed BuildID rejected");
});

Deno.test("marker wait reports timeout and browser-driver error", async () => {
  const directory = await Deno.makeTempDir();
  try {
    await assertRejects(
      () =>
        waitForDurableMarker({
          markerPath: `${directory}/missing.json`,
          errorPath: `${directory}/error.json`,
          runNonce: "nonce",
          launchId: 1,
          boundary: "PREPARED",
          timeoutMs: 5,
          pollIntervalMs: 1,
        }),
      "missing marker should time out",
    );
    await Deno.writeTextFile(`${directory}/error.json`, "driver failed");
    await assertRejects(
      () =>
        waitForDurableMarker({
          markerPath: `${directory}/missing.json`,
          errorPath: `${directory}/error.json`,
          runNonce: "nonce",
          launchId: 1,
          boundary: "PREPARED",
          timeoutMs: 5,
          pollIntervalMs: 1,
        }),
      "driver error should abort marker wait",
    );
    await Deno.writeTextFile(`${directory}/marker.json`, "{");
    await assertRejects(
      () =>
        waitForDurableMarker({
          markerPath: `${directory}/marker.json`,
          errorPath: `${directory}/absent.json`,
          runNonce: "nonce",
          launchId: 1,
          boundary: "PREPARED",
          timeoutMs: 5,
          pollIntervalMs: 1,
        }),
      "malformed marker should fail immediately",
    );
  } finally {
    await Deno.remove(directory, { recursive: true });
  }
});
