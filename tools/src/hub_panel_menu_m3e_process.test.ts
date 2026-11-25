// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertThrows } from "@std/assert";
import { BIN_PATH_EXE } from "./defines.ts";
import {
  assertHubPanelMenuM3eProcessProfile,
  assertHubPanelMenuM3eUserJsDoesNotOverride,
  captureHubPanelMenuM3eLaunchProcesses,
  hubPanelMenuM3eProcessExited,
  preexistingHubPanelMenuM3eProcesses,
  readHubPanelMenuM3eBoundedOutput,
  readHubPanelMenuM3eListeningPorts,
  refreshHubPanelMenuM3eOwnedProcesses,
  refreshHubPanelMenuM3eProcessSnapshot,
  seedHubPanelMenuM3eProfile,
  superviseHubPanelMenuM3eOwnedCommandOperations,
} from "./hub_panel_menu_m3e_process.ts";
import type {
  DevProcessIdentity,
  DevProcessInfo,
} from "./dev_process_types.ts";
import type { HubPanelMenuM3eOwnedCommandOperations } from "./hub_panel_menu_m3e_process.types.ts";

const encode = (value: string): Uint8Array => new TextEncoder().encode(value);

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
    ).map((identity) => identity.pid),
    [73],
  );
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
