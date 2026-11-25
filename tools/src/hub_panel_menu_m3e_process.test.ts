// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertThrows } from "@std/assert";
import { BIN_PATH_EXE } from "./defines.ts";
import {
  serializeHubPanelMenuM3eReport,
  writeReport,
} from "./hub_panel_menu_m3e_acceptance.ts";
import {
  assertHubPanelMenuM3eProcessProfile,
  assertHubPanelMenuM3eUserJsDoesNotOverride,
  captureHubPanelMenuM3eLaunchProcesses,
  cleanupHubPanelMenuM3eLaunches,
  hubPanelMenuM3eFailure,
  hubPanelMenuM3eLaunchEnvironment,
  hubPanelMenuM3eLaunchPids,
  hubPanelMenuM3eProcessExited,
  hubPanelMenuM3eStatusStages,
  preexistingHubPanelMenuM3eProcesses,
  readHubPanelMenuM3eBoundedOutput,
  readHubPanelMenuM3eListeningPorts,
  refreshHubPanelMenuM3eOwnedProcesses,
  refreshHubPanelMenuM3eProcessSnapshot,
  seedHubPanelMenuM3eProfile,
  serializeHubPanelMenuM3eEvidence,
  superviseHubPanelMenuM3eOwnedCommandOperations,
  withHubPanelMenuM3eCleanup,
  writeHubPanelMenuM3eCommandArtifacts,
} from "./hub_panel_menu_m3e_process.ts";
import { createDevServerLog } from "./dev_launch.ts";
import type {
  DevProcessIdentity,
  DevProcessInfo,
} from "./dev_process_types.ts";
import type { HubPanelMenuM3eOwnedCommandOperations } from "./hub_panel_menu_m3e_process.types.ts";

const encode = (value: string): Uint8Array => new TextEncoder().encode(value);

Deno.test("M3e status diagnostics retain only bounded allowlisted stages, never error text or session identifiers", async () => {
  assertEquals(hubPanelMenuM3eStatusStages(""), []);
  const stderr =
    "SECRET_SENTINEL sessionId=SECRET_SENTINEL\n[m3e-status] connect\n[m3e-status] handshake\r\n[m3e-status] command\n[m3e-status] timeout\n[m3e-status] SECRET_SENTINEL\n[m3e-status] command SECRET_SENTINEL";
  assertEquals(hubPanelMenuM3eStatusStages(stderr), [
    "connect",
    "handshake",
    "command",
    "timeout",
  ]);
  assertEquals(
    hubPanelMenuM3eStatusStages("[m3e-status] command\n".repeat(100)).length,
    32,
  );
  const root = await Deno.makeTempDir();
  try {
    await writeHubPanelMenuM3eCommandArtifacts(
      {
        completed: true,
        success: false,
        cleanupProven: true,
        stdout: "SECRET_SENTINEL",
        stderr,
      },
      `${root}/out`,
      `${root}/err`,
      1,
    );
    assertEquals(JSON.parse(await Deno.readTextFile(`${root}/err`)), {
      omitted: "raw output",
      stages: ["connect", "handshake", "command", "timeout"],
    });
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("M3e launch ownership requires the unique kernel environment token and unchanged start ticks", async () => {
  const process: DevProcessInfo = {
    pid: 81,
    ppid: 1,
    name: "deno",
    commandLine: "same checkout launch",
    startTime: "boot:100",
  };
  const rows = [process, { ...process, pid: 82 }, { ...process, pid: 83 }];
  const readText = (path: string): Promise<string> => {
    if (path.includes("/82/")) {
      return Promise.resolve("FLOORP_M3E_LAUNCH_TOKEN=peer\0");
    }
    if (path.endsWith("environ")) {
      return Promise.resolve("FLOORP_M3E_LAUNCH_TOKEN=owned\0");
    }
    return Promise.resolve(
      `81 (name with spaces) S ${Array(18).fill("0").join(" ")} ${
        path.includes("/83/") ? "200" : "100"
      }`,
    );
  };
  assertEquals(await hubPanelMenuM3eLaunchPids(rows, "owned", readText), [81]);
  assertEquals(await hubPanelMenuM3eLaunchPids([], "owned", readText), []);
  assertEquals(
    await hubPanelMenuM3eLaunchPids(
      rows,
      "owned",
      () => Promise.reject(new Error("unreadable")),
    ),
    [],
  );
  await assertRejects(() => hubPanelMenuM3eLaunchPids(rows, "", readText));
});

Deno.test("M3e final cleanup retains failed-start launches and continues after a stop failure", async () => {
  const launches = ["failed-start", "active", "already-stopped"];
  const attempted: string[] = [];
  assertEquals(
    await cleanupHubPanelMenuM3eLaunches(launches, (launch) => {
      attempted.push(launch);
      return launch === "failed-start"
        ? Promise.reject(new Error("retry required"))
        : Promise.resolve(true);
    }),
    false,
  );
  assertEquals(attempted, launches);
  assertEquals(
    await cleanupHubPanelMenuM3eLaunches(launches, () => Promise.resolve(true)),
    true,
  );
  assertEquals(
    await cleanupHubPanelMenuM3eLaunches([], () => Promise.resolve(false)),
    true,
  );
});

Deno.test("M3e snapshots discard only kernel-proven exited rows and fail closed on unreadable state", async () => {
  const live: DevProcessInfo = {
    pid: 81,
    ppid: 1,
    startTime: "boot-id:100",
    commandLine: "owned",
    name: "deno",
  };
  const rows = [live, { ...live, pid: 82 }, { ...live, pid: 83 }];
  assertEquals(
    await refreshHubPanelMenuM3eProcessSnapshot(rows, (pid) =>
      Promise.resolve(
        pid === 81 ? "81 (deno) S 1" : pid === 82 ? "82 (deno) Z 1" : null,
      )),
    [live],
  );
  assertEquals(
    await refreshHubPanelMenuM3eProcessSnapshot(
      [],
      () => Promise.resolve(null),
    ),
    [],
  );
  await assertRejects(
    () =>
      refreshHubPanelMenuM3eProcessSnapshot(
        rows,
        () => Promise.reject(new Error("unreadable kernel state")),
      ),
    Error,
    "unreadable kernel state",
  );
});

Deno.test("M3e lsof inventory accepts documented empty exit 1 but rejects errors and incomplete proof", () => {
  const empty = {
    completed: true,
    success: false,
    exitCode: 1,
    stdout: "",
    stderr: "",
    cleanupProven: true,
  };
  assertEquals(readHubPanelMenuM3eListeningPorts(empty), []);
  assertEquals(
    readHubPanelMenuM3eListeningPorts({
      ...empty,
      success: true,
      exitCode: 0,
      stdout: "p81\nn127.0.0.1:2828\nn[::1]:5181\nn127.0.0.1:2828\n",
    }),
    [2828, 5181],
  );
  for (
    const change of [
      { exitCode: 2 },
      { stderr: "error" },
      { stdout: "partial" },
      { completed: false },
      { cleanupProven: false },
    ]
  ) {
    assertThrows(() =>
      readHubPanelMenuM3eListeningPorts({ ...empty, ...change })
    );
  }
});

Deno.test("M3e orderly exit proof uses kernel lifetime and state rather than mutable command text", () => {
  const identity = {
    pid: 81,
    startTime: "boot-id:100",
    commandLine: "floorp-bin --profile /owned",
  };
  const stat = (state: string, ticks = "100", pid = 81) => {
    const fields = Array<string>(20).fill("0");
    fields[0] = state;
    fields[19] = ticks;
    return `${pid} (floorp-bin) ${fields.join(" ")}`;
  };
  assertEquals(hubPanelMenuM3eProcessExited(identity, stat("R")), false);
  assertEquals(hubPanelMenuM3eProcessExited(identity, stat("S")), false);
  assertEquals(hubPanelMenuM3eProcessExited(identity, stat("Z")), true);
  assertEquals(hubPanelMenuM3eProcessExited(identity, stat("X")), true);
  assertEquals(hubPanelMenuM3eProcessExited(identity, stat("R", "101")), true);
  assertEquals(hubPanelMenuM3eProcessExited(identity, null), true);
  assertThrows(() => hubPanelMenuM3eProcessExited(identity, "invalid"));
  assertThrows(() =>
    hubPanelMenuM3eProcessExited(identity, stat("R", "100", 82))
  );
  assertThrows(() =>
    hubPanelMenuM3eProcessExited(
      { ...identity, startTime: "unverified" },
      stat("R"),
    )
  );
});

function outputStream(chunks: readonly string[]): ReadableStream<Uint8Array> {
  return new ReadableStream({
    start(controller) {
      for (const chunk of chunks) controller.enqueue(encode(chunk));
      controller.close();
    },
  });
}

function operationSet(
  stdout: ReadableStream<Uint8Array>,
  stderr: ReadableStream<Uint8Array>,
): HubPanelMenuM3eOwnedCommandOperations {
  return {
    stdout: readHubPanelMenuM3eBoundedOutput(stdout, 4),
    stderr: readHubPanelMenuM3eBoundedOutput(stderr, 32),
    status: Promise.resolve({ success: true }),
    terminate: () => {},
  };
}

Deno.test("M3e profile seeding writes only the two explicit boolean prefs", async () => {
  const root = await Deno.makeTempDir();
  const profile = `${root}/owned-profile`;
  try {
    await seedHubPanelMenuM3eProfile(profile, true, false);
    assertEquals(
      await Deno.readTextFile(`${profile}/prefs.js`),
      'user_pref("nora.dev.hub_menu_feature", true);\n' +
        'user_pref("nora.dev.hub_menu_manual_host", false);\n',
    );
    await assertHubPanelMenuM3eUserJsDoesNotOverride(profile);
    await Deno.writeTextFile(
      `${profile}/user.js`,
      'user_pref("nora.dev.hub_menu_manual_host", true);\n',
    );
    await assertRejects(
      () => assertHubPanelMenuM3eUserJsDoesNotOverride(profile),
      Error,
      "overrides",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("M3e process profile assertion requires one exact profile argument", () => {
  const profilePath = "/owned/m3e/profile";
  const process: DevProcessInfo = {
    pid: 42,
    ppid: 1,
    name: "floorp-bin",
    commandLine: `${BIN_PATH_EXE} --profile ${profilePath}`,
    startTime: "owned",
    argv: [BIN_PATH_EXE, "--profile", profilePath],
  };
  assertEquals(
    assertHubPanelMenuM3eProcessProfile([process], profilePath).pid,
    process.pid,
  );
  let rejected = false;
  try {
    assertHubPanelMenuM3eProcessProfile([process], `${profilePath}-other`);
  } catch {
    rejected = true;
  }
  assertEquals(rejected, true);
});

Deno.test("M3e bounded command preserves partial stdout and stderr on overflow", async () => {
  const result = await superviseHubPanelMenuM3eOwnedCommandOperations(
    operationSet(outputStream(["abc", "de"]), outputStream(["warning"])),
    100,
    100,
  );
  assertEquals(result.completed, false);
  assertEquals(result.success, false);
  assertEquals(result.stdout, "abcd");
  assertEquals(result.stderr, "warning");
  assertEquals(result.cleanupProven, true);
});

Deno.test("M3e unsettled cancellation is bounded and retains partial output", async () => {
  const pending = new ReadableStream<Uint8Array>({
    start(controller) {
      controller.enqueue(encode("part"));
    },
    cancel() {
      return new Promise<void>(() => {});
    },
  });
  let deadline: ReturnType<typeof setTimeout> | undefined;
  const timeout = new Promise<never>((_, reject) => {
    deadline = setTimeout(
      () => reject(new Error("supervisor was unbounded")),
      500,
    );
  });
  try {
    const result = await Promise.race([
      superviseHubPanelMenuM3eOwnedCommandOperations(
        operationSet(pending, outputStream([])),
        5,
        10,
      ),
      timeout,
    ]);
    assertEquals(result.completed, false);
    assertEquals(result.cleanupProven, false);
    assertEquals(result.stdout, "part");
  } finally {
    if (deadline !== undefined) clearTimeout(deadline);
  }
});

Deno.test("M3e process ownership refuses checkout runtimes, not unrelated identities", () => {
  const unrelated: DevProcessInfo = {
    pid: 71,
    ppid: 1,
    name: "other-browser",
    commandLine: "/other/browser --profile /other/profile",
    startTime: "same-start",
    argv: ["/other/browser", "--profile", "/other/profile"],
  };
  const staleIdentity: DevProcessIdentity = {
    pid: unrelated.pid,
    startTime: "old-start",
    commandLine: unrelated.commandLine,
  };
  assertEquals(
    preexistingHubPanelMenuM3eProcesses([unrelated], [staleIdentity]).length,
    0,
  );

  const checkoutRuntime: DevProcessInfo = {
    ...unrelated,
    pid: 72,
    name: "floorp-bin",
    commandLine: `${BIN_PATH_EXE} --profile /other/profile`,
    argv: [BIN_PATH_EXE, "--profile", "/other/profile"],
  };
  assertEquals(
    preexistingHubPanelMenuM3eProcesses([checkoutRuntime], []).map((item) =>
      item.pid
    ),
    [checkoutRuntime.pid],
  );
  assertEquals(
    preexistingHubPanelMenuM3eProcesses([unrelated], [{
      pid: unrelated.pid,
      startTime: unrelated.startTime,
      commandLine: unrelated.commandLine,
    }]).map((item) => item.pid),
    [unrelated.pid],
  );

  const before: DevProcessInfo[] = [unrelated];
  const after: DevProcessInfo[] = [unrelated, {
    pid: 73,
    ppid: 1,
    name: "floorp-bin",
    commandLine: `${BIN_PATH_EXE} --profile /unique/m3e-profile`,
    startTime: "new-start",
    argv: [BIN_PATH_EXE, "--profile", "/unique/m3e-profile"],
  }];
  const registered: DevProcessIdentity[] = [{
    pid: 73,
    startTime: "new-start",
    commandLine: `${BIN_PATH_EXE} --profile /unique/m3e-profile`,
  }];
  assertEquals(
    captureHubPanelMenuM3eLaunchProcesses(
      before,
      after,
      "/unique/m3e-profile",
      registered,
      [73],
    ).map((identity) => identity.pid),
    [73],
  );
  const peer = {
    ...after[1],
    pid: 74,
    commandLine: `${BIN_PATH_EXE} --profile /peer`,
    argv: [BIN_PATH_EXE, "--profile", "/peer"],
  };
  assertEquals(
    captureHubPanelMenuM3eLaunchProcesses(
      before,
      [...after, peer],
      "/unique/m3e-profile",
      [...registered, peer],
      [73],
    ).map((identity) => identity.pid),
    [73],
  );
  assertEquals(
    captureHubPanelMenuM3eLaunchProcesses(
      before,
      after,
      "/unique/m3e-profile",
      registered,
    ),
    [],
  );
});

Deno.test("M3e environment and all retained command artifacts exclude session and secret sentinels", async () => {
  const marker = "SESSION_SECRET_SENTINEL";
  assertEquals(
    hubPanelMenuM3eLaunchEnvironment({
      PATH: "/bin",
      DISPLAY: ":0",
      API_KEY: marker,
      MARIONETTE_SESSION: marker,
    }),
    { PATH: "/bin", DISPLAY: ":0" },
  );
  const root = await Deno.makeTempDir();
  try {
    const report: Parameters<typeof writeReport>[1] = {
      schemaVersion: 1,
      runId: "test",
      startedAt: "test",
      completedAt: null,
      passed: false,
      cases: [{
        id: "reopen-enabled-reverified",
        status: "failed",
        startedAt: null,
        completedAt: null,
        failure: hubPanelMenuM3eFailure("run", new Error(marker)),
        observations: {
          applicationQuit: JSON.stringify({
            browserIdentities: [{ commandLine: marker }],
          }),
          launch: JSON.stringify({
            identities: [{ commandLine: marker }],
            sessionId: marker,
          }),
        },
      }],
      launchFailures: [
        hubPanelMenuM3eFailure("launch", new Error(marker), "C"),
      ],
      runFailure: hubPanelMenuM3eFailure("run", new Error(marker)),
      sharedProfileBefore: null,
      sharedProfileAfter: null,
      cleanup: {
        launchOwnershipProven: false,
        ownedProcessesStopped: false,
        ownedPortsClosed: false,
        fixtureClosed: false,
        fixtureObservationsBounded: false,
        profilesRemoved: false,
        sharedProfileUnchanged: false,
        ownedCommandCleanupProven: false,
        reportWritten: false,
      },
      launches: [{
        rowId: "C",
        profilePath: "/owned",
        identities: [{ pid: 81, startTime: "boot:100", commandLine: marker }],
        ports: [],
        failure: hubPanelMenuM3eFailure("launch", new Error(marker), "C"),
      }],
      packageMetadata: [],
      policyBinding: null,
      reportPath: `${root}/report.json`,
    };
    await writeReport(`${root}/report.json`, report);
    await Deno.writeTextFile(
      `${root}/malicious-failures.json`,
      serializeHubPanelMenuM3eEvidence({
        runFailure: marker,
        failure: { category: marker, kind: marker, message: marker },
        launchFailures: [marker],
      }),
    );
    const vite = await createDevServerLog(
      `${root}/vite-main.log`,
      true,
      "main",
      5173,
      81,
    );
    await vite.write(new TextEncoder().encode(marker));
    await vite.finish("failed", 1);
    await Deno.writeTextFile(
      `${root}/orderly-restarts.json`,
      serializeHubPanelMenuM3eEvidence([{
        browserIdentities: [{ commandLine: marker }],
      }]),
    );
    await Deno.writeTextFile(
      `${root}/console.json`,
      serializeHubPanelMenuM3eReport(report),
    );
    await writeHubPanelMenuM3eCommandArtifacts(
      {
        completed: true,
        success: true,
        cleanupProven: true,
        stdout: `Connected to browser. ${marker}`,
        stderr: marker,
      },
      `${root}/stdout`,
      `${root}/stderr`,
      0,
    );
    for await (const entry of Deno.readDir(root)) {
      assertEquals(
        (await Deno.readTextFile(`${root}/${entry.name}`)).includes(marker),
        false,
      );
    }
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("M3e termination runs even when diagnostics or failed-start evidence writing rejects", async () => {
  let stopped = 0;
  for (
    const diagnostic of [
      () => Promise.resolve(),
      () => Promise.reject(new Error("ENOSPC")),
    ]
  ) {
    try {
      await withHubPanelMenuM3eCleanup(diagnostic, () => {
        stopped++;
        return Promise.resolve();
      });
    } catch { /* Expected diagnostic failure. */ }
  }
  assertEquals(stopped, 2);
});

Deno.test("M3e teardown refreshes descendants only under verified owned roots", () => {
  const root: DevProcessInfo = {
    pid: 81,
    ppid: 1,
    name: "floorp-bin",
    commandLine: `${BIN_PATH_EXE} --profile /owned/profile`,
    startTime: "boot-id:100",
    argv: [BIN_PATH_EXE, "--profile", "/owned/profile"],
  };
  const ownedRoot: DevProcessIdentity = {
    pid: root.pid,
    startTime: root.startTime,
    commandLine: root.commandLine,
  };
  const descendant: DevProcessInfo = {
    pid: 82,
    ppid: root.pid,
    name: "floorp-content",
    commandLine: "floorp-content --contentproc",
    startTime: "boot-id:101",
  };
  const grandchild: DevProcessInfo = {
    pid: 83,
    ppid: descendant.pid,
    name: "floorp-utility",
    commandLine: "floorp-utility --utility",
    startTime: "boot-id:102",
  };
  const unrelated: DevProcessInfo = {
    pid: 84,
    ppid: 1,
    name: "other-browser",
    commandLine: "/other/browser",
    startTime: "boot-id:102",
  };
  const refreshed = refreshHubPanelMenuM3eOwnedProcesses(
    [ownedRoot],
    [root, descendant, grandchild, unrelated],
  );
  assertEquals(refreshed.ownershipProven, true);
  assertEquals(refreshed.identities.map((identity) => identity.pid), [
    81,
    82,
    83,
  ]);
  assertEquals(refreshed.liveIdentities.map((identity) => identity.pid), [
    81,
    82,
    83,
  ]);

  const reusedRoot: DevProcessInfo = {
    ...root,
    commandLine: `${BIN_PATH_EXE} --profile /unrelated/profile`,
    startTime: "boot-id:200",
  };
  const reusedChild: DevProcessInfo = {
    ...descendant,
    ppid: reusedRoot.pid,
    startTime: "boot-id:201",
  };
  const reused = refreshHubPanelMenuM3eOwnedProcesses(
    [ownedRoot],
    [reusedRoot, reusedChild],
  );
  assertEquals(reused.ownershipProven, false);
  assertEquals(reused.identities, [ownedRoot]);
  assertEquals(reused.liveIdentities, []);
});
