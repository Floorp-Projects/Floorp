// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";
import {
  matchesDevIdentity,
  normalizeDevPath,
  parseDevProcessRegistry,
  parseUnixDevProcesses,
  selectDevProcesses,
  splitDevCommandLine,
} from "./dev_process_selection.ts";
import type { DevProcessInfo, DevStopScope } from "./dev_process_types.ts";

const startTime = "2026-11-07T01:00:00.0000000Z";
const root = "E:\\dev\\Floorp One";
const profile = `${root}\\_dist\\profile\\test`;
const other = "E:\\dev\\Floorp Two";

function process(
  pid: number,
  overrides: Partial<DevProcessInfo> = {},
): DevProcessInfo {
  return {
    pid,
    ppid: 0,
    name: "deno.exe",
    commandLine: "deno run -A npm:vite",
    startTime,
    ...overrides,
  };
}

function scope(overrides: Partial<DevStopScope> = {}): DevStopScope {
  return {
    platform: "windows",
    projectRoot: root,
    profilePath: profile,
    recordedProcesses: [],
    ...overrides,
  };
}

Deno.test("stop selects only this checkout's tree, recorded orphans and exact browser profile", () => {
  const server = process(10, {
    name: "cmd.exe",
    commandLine:
      `cmd /c deno task feles-build dev > "${root}\\_dist\\dev-server.log"`,
  });
  const orphan = process(30);
  const processes = [
    server,
    process(11, { ppid: 10 }),
    process(12, { ppid: 11 }),
    orphan,
    process(31, { ppid: 30 }),
    process(40, {
      name: "floorp.exe",
      commandLine: `floorp --profile "${profile}" --marionette`,
    }),
    process(41, { ppid: 40, name: "floorp.exe" }),
    process(50, {
      commandLine: `deno run ${other}\\tools\\feles-build.ts dev`,
    }),
    process(51, { ppid: 50 }),
    process(60, {
      name: "floorp.exe",
      commandLine: `floorp --profile "${other}\\_dist\\profile\\test"`,
    }),
    process(70, {
      name: "floorp.exe",
      commandLine: "floorp --profile C:\\Users\\user\\regular",
    }),
  ];
  assertEquals(
    selectDevProcesses(
      processes,
      scope({ serverPid: 10, recordedProcesses: [server, orphan] }),
    ),
    [12, 11, 10, 31, 30, 41, 40],
  );
});

Deno.test("stop accepts Windows case, separators, dot segments and trailing separator", () => {
  const browser = process(1, {
    name: "FLOORP.EXE",
    commandLine:
      'floorp --profile="e:/DEV/Floorp One/_dist/profile/../profile/test/"',
  });
  assertEquals(selectDevProcesses([browser], scope()), [1]);
});

for (const suffix of ["-other", "\\child", " extra", '"suffix']) {
  Deno.test(`stop rejects a profile with suffix ${suffix}`, () => {
    const value = suffix === '"suffix'
      ? `"${profile}"suffix`
      : `"${profile}${suffix}"`;
    assertEquals(
      selectDevProcesses([
        process(1, {
          name: "floorp.exe",
          commandLine: `floorp --profile ${value}`,
        }),
      ], scope()),
      [],
    );
  });
}

Deno.test("stop requires a real profile option and Floorp executable", () => {
  assertEquals(
    selectDevProcesses([
      process(1, {
        name: "floorp.exe",
        commandLine:
          `floorp --new-window "https://example.org/?q= --profile ${profile}"`,
      }),
      process(2, { commandLine: `deno --profile "${profile}"` }),
      process(3, {
        name: "floorp.exe",
        commandLine: `floorp --other-profile "${profile}"`,
      }),
      process(4, {
        name: "floorp.exe",
        commandLine: "floorp --profile _dist/profile/test",
      }),
    ], scope()),
    [],
  );
});

Deno.test("stop rejects a reused recorded PID even with the same command line", () => {
  const old = process(1);
  const live = process(1, { startTime: "2026-11-07T02:00:00.0000000Z" });
  assertEquals(matchesDevIdentity(live, old), false);
  assertEquals(
    selectDevProcesses(
      [live, process(2, { ppid: 1 })],
      scope({ serverPid: 1, recordedProcesses: [old] }),
    ),
    [],
  );
});

Deno.test("stop rejects changed or missing identity", () => {
  const old = process(1);
  assertEquals(
    selectDevProcesses(
      [process(1, { commandLine: "unrelated" })],
      scope({ recordedProcesses: [old] }),
    ),
    [],
  );
  assertEquals(matchesDevIdentity(old, { ...old, startTime: "" }), false);
  assertEquals(
    matchesDevIdentity({ ...old, startTime: "" }, { ...old, startTime: "" }),
    false,
  );
});

Deno.test("stale PID file without checkout evidence cannot select an unrelated tree", () => {
  assertEquals(
    selectDevProcesses(
      [process(1), process(2, { ppid: 1 })],
      scope({ serverPid: 1 }),
    ),
    [],
  );
});

Deno.test("legacy Windows PID file is accepted with this checkout's absolute log path", () => {
  const commandLine =
    `cmd /c deno task feles-build dev > "${root}\\_dist\\dev-server.log"`;
  assertEquals(
    selectDevProcesses(
      [process(1, { commandLine }), process(2, { ppid: 1 })],
      scope({ serverPid: 1 }),
    ),
    [2, 1],
  );
});

Deno.test("Windows stale parent PID does not claim a child older than the parent", () => {
  const server = process(1, { startTime: "2026-11-07T02:00:00Z" });
  assertEquals(
    selectDevProcesses(
      [server, process(2, { ppid: 1 })],
      scope({ recordedProcesses: [server] }),
    ),
    [1],
  );
});

Deno.test("--all explicitly restores machine-wide Windows recovery and descendants", () => {
  const processes = [
    process(1, { commandLine: `deno run ${other}\\tools\\feles-build.ts dev` }),
    process(2, { ppid: 1 }),
    process(3, {
      name: "floorp.exe",
      commandLine: `floorp --profile "${other}\\_dist\\profile\\test"`,
    }),
    process(4, {
      name: "floorp.exe",
      commandLine: "floorp --profile C:\\regular",
    }),
  ];
  assertEquals(selectDevProcesses(processes, scope()), []);
  assertEquals(selectDevProcesses(processes, scope({ all: true })), [2, 1, 3]);
});

Deno.test("stop never selects excluded caller PIDs, even with --all", () => {
  const server = process(1, { ppid: 2, commandLine: "deno feles-build" });
  assertEquals(
    selectDevProcesses(
      [server, process(2, { ppid: 1 })],
      scope({ recordedProcesses: [server], all: true, excludePids: [2] }),
    ),
    [1],
  );
});

for (const platform of ["linux", "darwin"] as const) {
  Deno.test(`${platform}: mixed checkout trees and orphan identities stay isolated`, () => {
    const server = process(1, {
      name: "deno",
      commandLine: "deno task feles-build dev",
    });
    const orphan = process(3, { name: "deno" });
    const processes = [
      server,
      process(2, { ppid: 1 }),
      orphan,
      process(4, {
        name: "floorp-bin",
        commandLine:
          'floorp --profile "/home/dev/Floorp One/_dist/profile/test"',
      }),
      process(5, {
        name: "deno",
        commandLine: "deno /home/dev/other/tools/feles-build.ts",
      }),
      process(6, {
        name: "floorp",
        commandLine: "floorp --profile /home/dev/other/_dist/profile/test",
      }),
    ];
    const localScope = scope({
      platform,
      projectRoot: "/home/dev/Floorp One",
      profilePath: "/home/dev/Floorp One/_dist/profile/test",
      serverPid: 1,
      recordedProcesses: [server, orphan],
    });
    assertEquals(selectDevProcesses(processes, localScope), [2, 1, 3, 4]);
    assertEquals(selectDevProcesses(processes, { ...localScope, all: true }), [
      2,
      1,
      3,
      4,
      5,
      6,
    ]);
  });

  Deno.test(`${platform}: POSIX profiles remain case sensitive and exact`, () => {
    const localScope = scope({
      platform,
      profilePath: "/dev/floorp/_dist/profile/test",
    });
    const processes = [
      process(1, {
        name: "floorp",
        commandLine: "floorp --profile=/dev/floorp/_dist/profile/test",
      }),
      process(2, {
        name: "floorp",
        commandLine: "floorp --profile /dev/Floorp/_dist/profile/test",
      }),
      process(3, {
        name: "floorp",
        commandLine: "floorp --profile /dev/floorp/_dist/profile/test-other",
      }),
    ];
    assertEquals(selectDevProcesses(processes, localScope), [1]);
  });
}

Deno.test("Linux argv preserves profile argument boundaries including spaces", () => {
  const profilePath = "/home/dev/Floorp One/_dist/profile/test";
  const browser = process(1, {
    name: "floorp",
    argv: ["floorp", "--profile", profilePath],
  });
  assertEquals(
    selectDevProcesses([browser], scope({ platform: "linux", profilePath })),
    [1],
  );
  assertEquals(
    selectDevProcesses([{
      ...browser,
      argv: ["floorp", "--url", `--profile ${profilePath}`],
    }], scope({ platform: "linux", profilePath })),
    [],
  );
});

Deno.test("macOS unquoted ps profile with spaces requires a complete argument", () => {
  const profilePath = "/Users/dev/Floorp One/_dist/profile/test";
  const browser = process(1, {
    name: "floorp",
    commandLine: `floorp --profile ${profilePath} --marionette`,
  });
  const localScope = scope({ platform: "darwin", profilePath });
  assertEquals(selectDevProcesses([browser], localScope), [1]);
  assertEquals(
    selectDevProcesses([{
      ...browser,
      commandLine: `floorp --profile ${profilePath} other --marionette`,
    }], localScope),
    [],
  );
});

Deno.test("Windows argument parser handles quoted executable, empty args and escaped quotes", () => {
  assertEquals(
    splitDevCommandLine(
      '"C:\\Program Files\\Floorp\\floorp.exe" "" --profile "E:\\Floorp One\\test"',
      "windows",
    ),
    [
      "C:\\Program Files\\Floorp\\floorp.exe",
      "",
      "--profile",
      "E:\\Floorp One\\test",
    ],
  );
  assertEquals(splitDevCommandLine('app "a\\"b"', "windows"), ["app", 'a"b']);
});

Deno.test("registry parser rejects malformed entries and missing start times", () => {
  const identity = { pid: 1, startTime, commandLine: "deno vite" };
  assertEquals(parseDevProcessRegistry({ version: 1, processes: [identity] }), [
    identity,
  ]);
  for (
    const value of [
      null,
      [],
      {},
      { version: 2, processes: [] },
      { version: 1, processes: [{ ...identity, pid: -1 }] },
      { version: 1, processes: [{ ...identity, startTime: "" }] },
      { version: 1, processes: [{ pid: "1" }] },
    ]
  ) {
    assertThrows(() => parseDevProcessRegistry(value));
  }
});

Deno.test("Unix ps parser preserves spaces in command lines and single-digit dates", () => {
  assertEquals(
    parseUnixDevProcesses(
      " 42 1 Sat Nov  7 10:30:00 2026 /Applications/Floorp App/floorp --profile /Users/dev/Floorp One/_dist/profile/test\ninvalid\n",
    ),
    [{
      pid: 42,
      ppid: 1,
      startTime: "Sat Nov  7 10:30:00 2026",
      name: "",
      commandLine:
        "/Applications/Floorp App/floorp --profile /Users/dev/Floorp One/_dist/profile/test",
    }],
  );
});

Deno.test("normalization uses the supplied platform, independent of the test host", () => {
  assertEquals(
    normalizeDevPath("E:/DEV/one/../two/", "windows"),
    "e:\\dev\\two",
  );
  assertEquals(normalizeDevPath("/DEV/one/../two/", "linux"), "/DEV/two");
});
