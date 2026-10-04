// SPDX-License-Identifier: MPL-2.0

import {
  assertEquals,
  assertNotEquals,
  assertRejects,
  assertThrows,
} from "@std/assert";
import * as path from "@std/path";
import {
  browserCommand,
  createIsolatedBrowserLaunch,
  createIsolatedBrowserPairLaunch,
  type IsolatedBrowserChild,
  type IsolatedBrowserProcessControl,
  startIsolatedBrowser,
  startIsolatedBrowserPair,
} from "./browser_launcher.ts";
import { PATHS } from "./defines.ts";

function assertPrivateMode(filePath: string): void {
  if (Deno.build.os === "windows") return;
  const mode = Deno.lstatSync(filePath).mode;
  if (mode === null) {
    throw new Error(`Expected a POSIX mode for ${filePath}`);
  }
  assertEquals(mode & 0o077, 0);
}

function mockProcessControl(
  stop: (child: IsolatedBrowserChild) => Promise<void> = (child) => {
    child.kill("SIGTERM");
    return Promise.resolve();
  },
): IsolatedBrowserProcessControl {
  return {
    capture(child, _launch, platform) {
      return Promise.resolve({ platform, rootPid: child.pid });
    },
    abort(child) {
      child.kill("SIGTERM");
      return Promise.resolve();
    },
    stop(child) {
      return stop(child);
    },
  };
}

function removeProfileForTest(profilePath: string): void {
  try {
    Deno.removeSync(profilePath, { recursive: true });
  } catch (error) {
    if (!(error instanceof Deno.errors.NotFound)) {
      throw error;
    }
  }
}

Deno.test("isolated browser launch owns a fresh private profile and Marionette port", () => {
  const launch = createIsolatedBrowserLaunch({
    binaryPath: "/opt/floorp/Floorp",
    port: 28291,
  });
  try {
    assertEquals(launch.port, 28291);
    assertNotEquals(launch.profilePath, PATHS.profile_test);
    assertEquals(launch.command, [
      "/opt/floorp/Floorp",
      "--profile",
      launch.profilePath,
      "--marionette",
      "--remote-allow-system-access",
      "--no-remote",
    ]);
    assertEquals(Object.isFrozen(launch), true);
    assertEquals(Object.isFrozen(launch.command), true);
    assertPrivateMode(launch.profilePath);

    const userJsPath = path.join(launch.profilePath, "user.js");
    assertPrivateMode(userJsPath);
    assertEquals(
      Deno.readTextFileSync(userJsPath),
      [
        'user_pref("remote.active-protocols", 0);',
        'user_pref("marionette.enabled", true);',
        'user_pref("marionette.port", 28291);',
        "",
      ].join("\n"),
    );
  } finally {
    removeProfileForTest(launch.profilePath);
  }
});

Deno.test("isolated launcher rejects invalid Marionette ports", () => {
  for (const port of [0, 1_023, 65_536]) {
    assertThrows(
      () =>
        createIsolatedBrowserLaunch({
          binaryPath: "/opt/floorp/Floorp",
          port,
        }),
      Error,
      "Marionette port",
    );
  }
});

Deno.test("isolated launcher cleanup is idempotent and removes its profile", () => {
  const launch = createIsolatedBrowserLaunch({
    binaryPath: "/opt/floorp/Floorp",
    port: 28292,
  });

  launch.cleanup();
  launch.cleanup();

  assertThrows(
    () => Deno.lstatSync(launch.profilePath),
    Deno.errors.NotFound,
  );
});

Deno.test("isolated browser pair rejects shared ports and owns distinct profiles", () => {
  assertThrows(
    () =>
      createIsolatedBrowserPairLaunch({
        first: {
          binaryPath: "/opt/floorp/Floorp",
          port: 28291,
        },
        second: {
          binaryPath: "/opt/floorp/Floorp",
          port: 28291,
        },
      }),
    Error,
    "must use distinct ports",
  );

  const pair = createIsolatedBrowserPairLaunch({
    first: {
      binaryPath: "/opt/floorp/Floorp",
      port: 28291,
    },
    second: {
      binaryPath: "/opt/floorp/Floorp",
      port: 28292,
    },
  });
  try {
    assertEquals(pair.first.port, 28291);
    assertEquals(pair.second.port, 28292);
    assertNotEquals(pair.first.profilePath, pair.second.profilePath);
    assertNotEquals(pair.first.profilePath, PATHS.profile_test);
    assertNotEquals(pair.second.profilePath, PATHS.profile_test);
    assertPrivateMode(pair.first.profilePath);
    assertPrivateMode(pair.second.profilePath);
  } finally {
    pair.first.cleanup();
    pair.second.cleanup();
  }
});

Deno.test("isolated browser process clears inherited secrets and cleans its profile", async () => {
  const launch = createIsolatedBrowserLaunch({
    binaryPath: "/opt/floorp/Floorp",
    port: 28291,
  });
  const killed: string[] = [];
  let captured:
    | {
      command: readonly string[];
      options: {
        clearEnv: boolean;
        env: Record<string, string>;
        stderr: string;
        stdin: string;
        stdout: string;
      };
    }
    | undefined;

  const running = await startIsolatedBrowser(launch, {
    environment: {
      FLOORP_FXA_TEST_SECRET: "must-not-reach-browser",
      HOME: "/private/tmp/floorp-home",
      PATH: "/usr/bin:/bin",
    },
    processControl: mockProcessControl(),
    spawn: (command, options) => {
      captured = { command, options };
      return {
        pid: 42,
        kill: (signal) => killed.push(signal ?? "SIGTERM"),
        status: Promise.resolve({ code: 0, signal: null, success: true }),
      };
    },
  });
  try {
    if (captured === undefined) {
      throw new Error("isolated browser was not spawned");
    }
    assertEquals(captured.command, launch.command);
    assertEquals(captured.options.clearEnv, true);
    assertEquals(captured.options.stdin, "null");
    assertEquals(captured.options.stdout, "null");
    assertEquals(captured.options.stderr, "null");
    assertEquals(captured.options.env.FLOORP_FXA_TEST_SECRET, undefined);
    assertEquals(captured.options.env.HOME, "/private/tmp/floorp-home");
    assertEquals(captured.options.env.PATH, "/usr/bin:/bin");

    await running.stop();
    assertEquals(killed, ["SIGTERM"]);
    assertThrows(
      () => Deno.lstatSync(launch.profilePath),
      Deno.errors.NotFound,
    );
  } finally {
    await running.stop();
  }
});

Deno.test("isolated Windows browser normalizes safe environment names and delegates owned-tree termination", async () => {
  const launch = createIsolatedBrowserLaunch({
    binaryPath: "/opt/floorp/Floorp",
    port: 28293,
  });
  const terminatedPIDs: number[] = [];
  const killed: string[] = [];
  let capturedEnvironment: Record<string, string> | undefined;
  let resolveStatus: ((status: Deno.CommandStatus) => void) | undefined;

  const running = await startIsolatedBrowser(launch, {
    environment: {
      ComSpec: "C:\\Windows\\System32\\cmd.exe",
      DISPLAY: ":99",
      FLOORP_FXA_TEST_SECRET: "must-not-reach-browser",
      HTTP_PROXY: "http://must-not-reach-browser.invalid",
      Path: "C:\\Windows\\System32",
      SystemRoot: "C:\\Windows",
      UNRELATED: "must-not-reach-browser",
      WAYLAND_DISPLAY: "wayland-0",
      XAUTHORITY: "/tmp/floorp-xauthority",
      XDG_RUNTIME_DIR: "/tmp/floorp-runtime",
    },
    platform: "windows",
    processControl: mockProcessControl((child) => {
      terminatedPIDs.push(child.pid);
      if (resolveStatus === undefined) {
        throw new Error("isolated browser status resolver was not installed");
      }
      resolveStatus({ code: 0, signal: null, success: true });
      return Promise.resolve();
    }),
    spawn: (_command, options) => {
      capturedEnvironment = options.env;
      return {
        pid: 43,
        kill: (signal) => killed.push(signal ?? "SIGTERM"),
        status: new Promise((resolve) => {
          resolveStatus = resolve;
        }),
      };
    },
  });
  try {
    assertEquals(capturedEnvironment, {
      COMSPEC: "C:\\Windows\\System32\\cmd.exe",
      DISPLAY: ":99",
      PATH: "C:\\Windows\\System32",
      SYSTEMROOT: "C:\\Windows",
      WAYLAND_DISPLAY: "wayland-0",
      XAUTHORITY: "/tmp/floorp-xauthority",
      XDG_RUNTIME_DIR: "/tmp/floorp-runtime",
    });
    await running.stop();
    assertEquals(terminatedPIDs, [43]);
    assertEquals(killed, []);
  } finally {
    removeProfileForTest(launch.profilePath);
  }
});

Deno.test("isolated browser retains its profile when the owned process cannot be confirmed stopped", async () => {
  const launch = createIsolatedBrowserLaunch({
    binaryPath: "/opt/floorp/Floorp",
    port: 28294,
  });
  const killed: string[] = [];
  const running = await startIsolatedBrowser(launch, {
    environment: {},
    processControl: mockProcessControl(() =>
      Promise.reject(new Error("owned process did not exit"))
    ),
    spawn: () => ({
      pid: 44,
      kill: (signal) => killed.push(signal ?? "SIGTERM"),
      status: new Promise<Deno.CommandStatus>(() => undefined),
    }),
  });
  try {
    await assertRejects(
      () => running.stop(),
      Error,
      "did not exit",
    );
    assertEquals(killed, []);
    assertEquals(Deno.lstatSync(launch.profilePath).isDirectory, true);
  } finally {
    removeProfileForTest(launch.profilePath);
  }
});

Deno.test("isolated browser retries a failed verified stop", async () => {
  const launch = createIsolatedBrowserLaunch({
    binaryPath: "/opt/floorp/Floorp",
    port: 28302,
  });
  let stopAttempts = 0;
  const running = await startIsolatedBrowser(launch, {
    environment: {},
    processControl: mockProcessControl(() => {
      stopAttempts += 1;
      return stopAttempts === 1
        ? Promise.reject(new Error("first verified stop failed"))
        : Promise.resolve();
    }),
    spawn: () => ({
      pid: 47,
      kill: () => undefined,
      status: new Promise<Deno.CommandStatus>(() => undefined),
    }),
  });
  try {
    await assertRejects(
      () => running.stop(),
      Error,
      "first verified stop failed",
    );
    assertEquals(Deno.lstatSync(launch.profilePath).isDirectory, true);
    await running.stop();
    assertEquals(stopAttempts, 2);
    assertThrows(
      () => Deno.lstatSync(launch.profilePath),
      Deno.errors.NotFound,
    );
  } finally {
    removeProfileForTest(launch.profilePath);
  }
});

Deno.test("isolated browser retains its profile when ownership capture fails after spawn", async () => {
  const launch = createIsolatedBrowserLaunch({
    binaryPath: "/opt/floorp/Floorp",
    port: 28298,
  });
  const killed: string[] = [];
  const processControl: IsolatedBrowserProcessControl = {
    capture: () => Promise.reject(new Error("could not capture owned process")),
    abort: (child) => {
      child.kill("SIGTERM");
      return Promise.reject(new Error("could not abort owned process"));
    },
    stop: () => Promise.resolve(),
  };
  try {
    await assertRejects(
      () =>
        startIsolatedBrowser(launch, {
          environment: {},
          processControl,
          spawn: () => ({
            pid: 45,
            kill: (signal) => killed.push(signal ?? "SIGTERM"),
            status: new Promise<Deno.CommandStatus>(() => undefined),
          }),
        }),
      Error,
      "could not capture",
    );
    assertEquals(killed, ["SIGTERM"]);
    assertEquals(Deno.lstatSync(launch.profilePath).isDirectory, true);
  } finally {
    removeProfileForTest(launch.profilePath);
  }
});

Deno.test("isolated browser cleans its profile when capture abort is verified", async () => {
  const launch = createIsolatedBrowserLaunch({
    binaryPath: "/opt/floorp/Floorp",
    port: 28305,
  });
  const abortedPIDs: number[] = [];
  const processControl: IsolatedBrowserProcessControl = {
    capture: () => Promise.reject(new Error("could not capture owned process")),
    abort: (child) => {
      abortedPIDs.push(child.pid);
      return Promise.resolve();
    },
    stop: () => Promise.resolve(),
  };
  try {
    await assertRejects(
      () =>
        startIsolatedBrowser(launch, {
          environment: {},
          processControl,
          spawn: () => ({
            pid: 48,
            kill: () => undefined,
            status: new Promise<Deno.CommandStatus>(() => undefined),
          }),
        }),
      Error,
      "could not capture",
    );
    assertEquals(abortedPIDs, [48]);
    assertThrows(
      () => Deno.lstatSync(launch.profilePath),
      Deno.errors.NotFound,
    );
  } finally {
    removeProfileForTest(launch.profilePath);
  }
});

Deno.test("isolated browser prevents external profile cleanup after spawn", async () => {
  const launch = createIsolatedBrowserLaunch({
    binaryPath: "/opt/floorp/Floorp",
    port: 28301,
  });
  let resolveCapture:
    | ((ownership: { platform: typeof Deno.build.os; rootPid: number }) => void)
    | undefined;
  const processControl: IsolatedBrowserProcessControl = {
    capture: () =>
      new Promise((resolve) => {
        resolveCapture = resolve;
      }),
    abort: () => Promise.resolve(),
    stop: () => Promise.resolve(),
  };
  const starting = startIsolatedBrowser(launch, {
    environment: {},
    processControl,
    spawn: () => ({
      pid: 46,
      kill: () => undefined,
      status: new Promise<Deno.CommandStatus>(() => undefined),
    }),
  });
  try {
    assertThrows(() => launch.cleanup(), Error, "running");
    if (resolveCapture === undefined) {
      throw new Error("ownership capture was not started");
    }
    resolveCapture({ platform: Deno.build.os, rootPid: 46 });
    const running = await starting;
    assertEquals("cleanup" in running.launch, false);
    await running.stop();
  } finally {
    removeProfileForTest(launch.profilePath);
  }
});

Deno.test("isolated browser pair removes the first profile if the second spawn fails", async () => {
  const commands: string[][] = [];
  await assertRejects(
    () =>
      startIsolatedBrowserPair(
        {
          first: {
            binaryPath: "/opt/floorp/Floorp",
            port: 28291,
          },
          second: {
            binaryPath: "/opt/floorp/Floorp",
            port: 28292,
          },
        },
        {
          environment: {},
          processControl: mockProcessControl(),
          spawn: (command) => {
            commands.push([...command]);
            if (commands.length === 2) {
              throw new Error("second browser spawn failed");
            }
            return {
              pid: 41,
              kill: () => undefined,
              status: Promise.resolve({ code: 0, signal: null, success: true }),
            };
          },
        },
      ),
    Error,
    "second browser spawn failed",
  );
  assertEquals(commands.length, 2);
  const firstProfilePath = commands[0][2];
  assertThrows(
    () => Deno.lstatSync(firstProfilePath),
    Deno.errors.NotFound,
  );
});

Deno.test("isolated browser pair attempts both stops and retains only an unverified profile", async () => {
  const commands: string[][] = [];
  const pair = await startIsolatedBrowserPair(
    {
      first: {
        binaryPath: "/opt/floorp/Floorp",
        port: 28295,
      },
      second: {
        binaryPath: "/opt/floorp/Floorp",
        port: 28296,
      },
    },
    {
      environment: {},
      processControl: mockProcessControl((child) =>
        child.pid === 1
          ? Promise.reject(new Error("owned process did not exit"))
          : Promise.resolve()
      ),
      spawn: (command) => {
        commands.push([...command]);
        return {
          pid: commands.length,
          kill: () => undefined,
          status: commands.length === 1
            ? new Promise<Deno.CommandStatus>(() => undefined)
            : Promise.resolve({ code: 0, signal: null, success: true }),
        };
      },
    },
  );
  const firstProfilePath = commands[0][2];
  const secondProfilePath = commands[1][2];
  try {
    await assertRejects(() => pair.stop(), AggregateError, "Failed to stop");
    assertEquals(Deno.lstatSync(firstProfilePath).isDirectory, true);
    assertThrows(
      () => Deno.lstatSync(secondProfilePath),
      Deno.errors.NotFound,
    );
  } finally {
    removeProfileForTest(pair.first.launch.profilePath);
    removeProfileForTest(pair.second.launch.profilePath);
  }
});

Deno.test("isolated browser pair retries after one verified stop fails", async () => {
  const commands: string[][] = [];
  let firstStopAttempts = 0;
  const pair = await startIsolatedBrowserPair(
    {
      first: {
        binaryPath: "/opt/floorp/Floorp",
        port: 28303,
      },
      second: {
        binaryPath: "/opt/floorp/Floorp",
        port: 28304,
      },
    },
    {
      environment: {},
      processControl: mockProcessControl((child) => {
        if (child.pid === 1) {
          firstStopAttempts += 1;
          return firstStopAttempts === 1
            ? Promise.reject(new Error("first pair stop failed"))
            : Promise.resolve();
        }
        return Promise.resolve();
      }),
      spawn: (command) => {
        commands.push([...command]);
        return {
          pid: commands.length,
          kill: () => undefined,
          status: new Promise<Deno.CommandStatus>(() => undefined),
        };
      },
    },
  );
  try {
    await assertRejects(() => pair.stop(), AggregateError, "Failed to stop");
    await pair.stop();
    assertEquals(firstStopAttempts, 2);
    assertThrows(
      () => Deno.lstatSync(commands[0][2]),
      Deno.errors.NotFound,
    );
    assertThrows(
      () => Deno.lstatSync(commands[1][2]),
      Deno.errors.NotFound,
    );
  } finally {
    removeProfileForTest(pair.first.launch.profilePath);
    removeProfileForTest(pair.second.launch.profilePath);
  }
});

Deno.test("isolated browser pair retains a profile whose ownership capture fails", async () => {
  const commands: string[][] = [];
  const processControl: IsolatedBrowserProcessControl = {
    capture(child, _launch, platform) {
      return child.pid === 2
        ? Promise.reject(new Error("could not capture second owned process"))
        : Promise.resolve({ platform, rootPid: child.pid });
    },
    abort: () =>
      Promise.reject(new Error("could not abort second owned process")),
    stop: () => Promise.resolve(),
  };
  await assertRejects(
    () =>
      startIsolatedBrowserPair(
        {
          first: {
            binaryPath: "/opt/floorp/Floorp",
            port: 28299,
          },
          second: {
            binaryPath: "/opt/floorp/Floorp",
            port: 28300,
          },
        },
        {
          environment: {},
          processControl,
          spawn: (command) => {
            commands.push([...command]);
            return {
              pid: commands.length,
              kill: () => undefined,
              status: new Promise<Deno.CommandStatus>(() => undefined),
            };
          },
        },
      ),
    Error,
    "could not capture second",
  );
  const firstProfilePath = commands[0][2];
  const secondProfilePath = commands[1][2];
  try {
    assertThrows(
      () => Deno.lstatSync(firstProfilePath),
      Deno.errors.NotFound,
    );
    assertEquals(Deno.lstatSync(secondProfilePath).isDirectory, true);
  } finally {
    removeProfileForTest(secondProfilePath);
  }
});

Deno.test("isolated browser refuses to spawn without an ownership controller", async () => {
  const launch = createIsolatedBrowserLaunch({
    binaryPath: "/opt/floorp/Floorp",
    port: 28297,
  });
  let spawnCalled = false;
  try {
    await assertRejects(
      () =>
        startIsolatedBrowser(launch, {
          environment: {},
          spawn: () => {
            spawnCalled = true;
            throw new Error("must not spawn without ownership control");
          },
        }),
      Error,
      "ownership controller",
    );
    assertEquals(spawnCalled, false);
    assertThrows(
      () => Deno.lstatSync(launch.profilePath),
      Deno.errors.NotFound,
    );
  } finally {
    launch.cleanup();
  }
});

Deno.test("browser command retains the ordinary single-profile default", () => {
  const command = browserCommand({ marionette: false });
  assertEquals(command.includes("--marionette"), false);
  assertEquals(command.includes("--no-remote"), false);
});
