// SPDX-License-Identifier: MPL-2.0

/** Host tests for the W-04 `verify build` guard (no real processes). */

import { assertEquals, assertRejects, assertThrows } from "@std/assert";
import * as path from "@std/path";
import {
  assertSourceBuildAllowed,
  DevGuardRefusalError,
  devProcessesFileFor,
  liveDevProcesses,
  phaseCompilesSource,
  readDevRegistryStrict,
} from "./fdh_dev_guard.ts";
import type {
  DevProcessIdentity,
  DevProcessInfo,
} from "./dev_process_types.ts";

const WORKTREE = path.resolve("fdh-guard-worktree");

const SERVER: DevProcessIdentity = {
  pid: 4100,
  startTime: "2026-11-09T05:36:26.9867700Z",
  commandLine: "deno run -A tools/feles-build.ts dev --port-offset 0",
};

function running(identity: DevProcessIdentity): DevProcessInfo {
  return { ...identity, ppid: 1, name: "deno.exe" };
}

function deps(
  recorded: DevProcessIdentity[],
  live: DevProcessInfo[],
  calls: { list: number; files: string[] },
) {
  return {
    readRegistry: (file: string) => {
      calls.files.push(file);
      return recorded;
    },
    listProcesses: () => {
      calls.list += 1;
      return Promise.resolve(live);
    },
  };
}

Deno.test("phaseCompilesSource: only after-mach skips the source build", () => {
  assertEquals(phaseCompilesSource("all"), true);
  assertEquals(phaseCompilesSource("before-mach"), true);
  assertEquals(phaseCompilesSource("after-mach"), false);
});

Deno.test("liveDevProcesses reads this worktree's registry", async () => {
  const calls = { list: 0, files: [] as string[] };
  await liveDevProcesses(WORKTREE, deps([], [], calls));
  assertEquals(calls.files, [devProcessesFileFor(WORKTREE)]);
  assertEquals(
    devProcessesFileFor(WORKTREE),
    path.join(WORKTREE, "_dist", "dev-processes.json"),
  );
  // An empty registry never takes a process snapshot.
  assertEquals(calls.list, 0);
});

Deno.test("liveDevProcesses ignores stale entries and reused PIDs", async () => {
  const calls = { list: 0, files: [] as string[] };
  const reused = running({ ...SERVER, startTime: "2026-11-09T06:00:00Z" });
  const live = await liveDevProcesses(
    WORKTREE,
    deps([SERVER], [reused], calls),
  );
  assertEquals(live, []);
  assertEquals(calls.list, 1);
});

Deno.test("source build is refused while the dev server runs", async () => {
  const calls = { list: 0, files: [] as string[] };
  for (const phase of ["all", "before-mach"] as const) {
    await assertRejects(
      () =>
        assertSourceBuildAllowed(
          WORKTREE,
          phase,
          deps([SERVER], [running(SERVER)], calls),
        ),
      DevGuardRefusalError,
      "fdh dev down",
    );
  }
});

Deno.test("a failed process listing is not reported as a refusal", async () => {
  const error = await assertRejects(() =>
    assertSourceBuildAllowed(WORKTREE, "all", {
      readRegistry: () => [SERVER],
      listProcesses: () => Promise.reject(new Error("powershell: failed")),
    })
  );
  assertEquals(error instanceof DevGuardRefusalError, false);
});

Deno.test("after-mach is allowed while the dev server runs", async () => {
  const calls = { list: 0, files: [] as string[] };
  await assertSourceBuildAllowed(
    WORKTREE,
    "after-mach",
    deps([SERVER], [running(SERVER)], calls),
  );
  // The inject-only phase does not even inspect processes.
  assertEquals(calls.files, []);
});

Deno.test("source build is allowed when the dev server is stopped", async () => {
  const calls = { list: 0, files: [] as string[] };
  await assertSourceBuildAllowed(WORKTREE, "all", deps([SERVER], [], calls));
  assertEquals(calls.list, 1);
});

Deno.test("strict registry read: missing is empty, corrupt fails closed", async () => {
  const dir = await Deno.makeTempDir({ prefix: "fdh-guard-registry-" });
  try {
    const file = devProcessesFileFor(dir);
    assertEquals(readDevRegistryStrict(file), []);

    Deno.mkdirSync(path.dirname(file), { recursive: true });
    Deno.writeTextFileSync(
      file,
      JSON.stringify({ version: 1, processes: [SERVER] }),
    );
    assertEquals(readDevRegistryStrict(file), [SERVER]);

    Deno.writeTextFileSync(file, "{ not json");
    assertThrows(() => readDevRegistryStrict(file));
    // The guard surfaces it as a guard failure, not as a refusal or a pass.
    const error = await assertRejects(() =>
      assertSourceBuildAllowed(dir, "all", {
        listProcesses: () => Promise.resolve([]),
      })
    );
    assertEquals(error instanceof DevGuardRefusalError, false);

    Deno.writeTextFileSync(file, JSON.stringify({ version: 2, processes: [] }));
    assertThrows(() => readDevRegistryStrict(file), Error, "Invalid");
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});
