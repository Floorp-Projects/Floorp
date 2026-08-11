// SPDX-License-Identifier: MPL-2.0

import {
  assertEquals,
  assertNotEquals,
  assertThrows,
} from "@std/assert";
import * as path from "@std/path";
import {
  browserCommand,
  createIsolatedBrowserLaunch,
  createIsolatedBrowserPairLaunch,
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
    launch.cleanup();
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

Deno.test("browser command retains the ordinary single-profile default", () => {
  const command = browserCommand({ marionette: false });
  assertEquals(command.includes("--marionette"), false);
  assertEquals(command.includes("--no-remote"), false);
});
