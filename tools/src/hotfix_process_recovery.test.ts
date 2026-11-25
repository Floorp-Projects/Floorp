import {
  assertOwnedBrowserRoot,
  assertOwnedMarionetteListener,
  assertProcessRecoveryPlatform,
  hashUntrackedSourceFiles,
  isLinuxProcessDisappearanceError,
  matchesDurableBoundary,
  parseBrowserDriverResult,
  parseDurableMarker,
  parseFirstWindowContinuityResult,
  parseLinuxProcStat,
  resolveExpectedRuntimeBuildId,
  runtimeLockArchitectureForDenoArch,
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

Deno.test("Linux proc identity reads tolerate exited-process races only", () => {
  assert(
    isLinuxProcessDisappearanceError(
      new Error("No such process (os error 3): readfile '/proc/123/stat'"),
    ),
    "ESRCH while reading proc identity is treated as an exited process",
  );
  assert(
    !isLinuxProcessDisappearanceError(
      new Error("permission denied while reading proc identity"),
    ),
    "ownership verification errors are not treated as process exit",
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

Deno.test("owned kill tolerates a verified child exiting before SIGKILL", async () => {
  const root = identity(10, 10);
  const child = { ...identity(11, 11), parentPid: 10, processGroupId: 11 };
  const signals: number[] = [];
  const killed = await signalVerifiedOwnedProcessTree({
    root,
    profilePath: "/private/profile",
    previouslyOwned: [root, child],
    dependencies: {
      listProcesses: () => Promise.resolve([root, child]),
      readProcess: (pid) => Promise.resolve(pid === root.pid ? root : child),
      signal: (pid) => {
        if (pid === child.pid) throw new Deno.errors.NotFound("already exited");
        signals.push(pid);
      },
    },
  });
  assert(
    killed.length === 2 && signals.length === 1 && signals[0] === root.pid,
    "exited child is skipped while the verified root is still signaled",
  );
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

Deno.test("initial HUB default boundary matches only the absent-record write", () => {
  const initial = {
    featureId: "hub-panel-menu",
    selectedVersion: "hub-menu-v1",
    selectedDigest: "a".repeat(64),
    desiredEnabled: true,
    selectedHealth: "unverified",
    selectedHealthBuild: null,
    pending: null,
    lastOutcome: null,
    blockedReason: null,
  };
  assert(
    matchesDurableBoundary("initial_default", initial),
    "initial default record matches its durable boundary",
  );
  assert(
    !matchesDurableBoundary("initial_default", {
      ...initial,
      desiredEnabled: false,
    }),
    "mismatched default state is rejected",
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

Deno.test("first-window continuity result binds nonce launch build and action", () => {
  const result = {
    schemaVersion: 1,
    kind: "first-window-continuity",
    runNonce: "nonce",
    launchId: 4,
    buildId: "20261112140843",
    action: "setup-w1",
    observations: [{
      label: "w1-active",
      hostEpoch: "epoch",
      generation: 1,
      stateRevision: 4,
      desiredEnabled: true,
      selectedVersion: "v1",
      selectedHealth: "verified",
      journal: { stateRevision: 4 },
      windowIds: ["host-w1"],
      windows: [{
        windowId: "outer-w1",
        hostWindowId: "host-w1",
        windowType: "navigator:browser",
        private: false,
        delayedStartupFinished: true,
        marker: "epoch:1",
      }],
      resources: {
        resources: 1,
        childScopes: 1,
        tasks: 0,
        pendingTasks: 0,
        observedTaskRejections: 0,
        taskFailures: 0,
      },
      lifecycle: {
        observers: true,
        tracked: 1,
        pendingStartup: 0,
        readyWaiters: 0,
        admissionTasks: 0,
        pendingDetaches: 0,
        cleanupFailures: 0,
      },
    }],
    details: { installStatus: "succeeded" },
  };
  const expected = {
    runNonce: "nonce",
    launchId: 4,
    buildId: result.buildId,
    action: "setup-w1" as const,
  };
  assert(
    parseFirstWindowContinuityResult(result, expected).action === "setup-w1",
    "valid continuity action parsed",
  );
  for (
    const altered of [
      { ...result, runNonce: "old" },
      { ...result, launchId: 3 },
      { ...result, buildId: "20260101000000" },
      { ...result, action: "close-w1" },
      { ...result, observations: [] },
      {
        ...result,
        observations: [{
          ...result.observations[0],
          resources: { resources: -1 },
        }],
      },
    ]
  ) {
    let rejected = false;
    try {
      parseFirstWindowContinuityResult(altered, expected);
    } catch {
      rejected = true;
    }
    assert(rejected, "malformed or replayed continuity result rejected");
  }
});

Deno.test("runtime BuildID is bound to lock architecture", () => {
  const encoder = new TextEncoder();
  const source = encoder.encode(JSON.stringify({
    artifacts: [
      {
        platform: "linux",
        architecture: "x86_64",
        buildId: "20261112140843",
      },
      {
        platform: "linux",
        architecture: "aarch64",
        buildId: "20261212140843",
      },
    ],
  }));
  for (
    const [denoArch, buildId] of [
      ["x86_64", "20261112140843"],
      ["aarch64", "20261212140843"],
    ] as const
  ) {
    const architecture = runtimeLockArchitectureForDenoArch(denoArch);
    assert(architecture === denoArch, `${denoArch} maps to lock architecture`);
    const installed = encoder.encode(JSON.stringify({
      platform: "linux",
      architecture,
      buildId,
    }));
    assert(
      resolveExpectedRuntimeBuildId(source, installed, denoArch) === buildId,
      `${denoArch} BuildID read from matching lock artifact`,
    );
    const mismatch = encoder.encode(JSON.stringify({
      platform: "linux",
      architecture: denoArch === "x86_64" ? "aarch64" : "x86_64",
      buildId,
    }));
    let architectureRejected = false;
    try {
      resolveExpectedRuntimeBuildId(source, mismatch, denoArch);
    } catch {
      architectureRejected = true;
    }
    assert(
      architectureRejected,
      `${denoArch} rejects mismatched installed architecture`,
    );
  }
  let buildIdRejected = false;
  try {
    resolveExpectedRuntimeBuildId(
      source,
      encoder.encode(JSON.stringify({
        platform: "linux",
        architecture: "x86_64",
        buildId: "20260101000000",
      })),
      "x86_64",
    );
  } catch {
    buildIdRejected = true;
  }
  assert(buildIdRejected, "mismatched installed BuildID rejected");
  let unsupportedRejected = false;
  try {
    runtimeLockArchitectureForDenoArch("riscv64");
  } catch {
    unsupportedRejected = true;
  }
  assert(unsupportedRejected, "unsupported Deno architecture rejected");
});

Deno.test({
  name: "untracked source hashes preserve raw NUL-delimited paths",
  ignore: Deno.build.os === "windows",
  fn: async () => {
    const directory = await Deno.makeTempDir();
    const names = [
      "日本語 file.txt",
      "line\nbreak.txt",
      `quote"and's.txt`,
      " leading and trailing ",
    ];
    const encoder = new TextEncoder();
    try {
      for (const [index, name] of names.entries()) {
        await Deno.writeFile(
          `${directory}/${name}`,
          encoder.encode(`file-${index}`),
        );
      }
      const listing = encoder.encode(`${names.join("\0")}\0`);
      const hashes = await hashUntrackedSourceFiles(directory, listing);
      const reordered = await hashUntrackedSourceFiles(
        directory,
        encoder.encode(`${[...names].reverse().join("\0")}\0`),
      );
      assert(hashes.length === names.length, "every path gets a source hash");
      assert(
        new Set(hashes).size === names.length,
        "path names bind each hash",
      );
      assert(
        hashes.join("\n") === reordered.join("\n"),
        "source hashes are stable regardless of listing order",
      );
      await assertRejects(
        () =>
          hashUntrackedSourceFiles(directory, encoder.encode("unterminated")),
        "unterminated path list is rejected",
      );
    } finally {
      await Deno.remove(directory, { recursive: true });
    }
  },
});

Deno.test({
  name: "untracked symlink hashes use link metadata without following targets",
  ignore: Deno.build.os === "windows",
  fn: async () => {
    const directory = await Deno.makeTempDir();
    const outside = await Deno.makeTempDir();
    const target = `${outside}/outside.txt`;
    const link = `${directory}/outside-link`;
    try {
      await Deno.writeTextFile(target, "outside content one");
      await Deno.symlink(target, link);
      const listing = new TextEncoder().encode("outside-link\0");
      const before = await hashUntrackedSourceFiles(directory, listing);
      await Deno.writeTextFile(target, "outside content two");
      const after = await hashUntrackedSourceFiles(directory, listing);
      assert(
        before.join("\n") === after.join("\n"),
        "link fingerprint is independent of target file content",
      );
    } finally {
      await Deno.remove(directory, { recursive: true });
      await Deno.remove(outside, { recursive: true });
    }
  },
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
