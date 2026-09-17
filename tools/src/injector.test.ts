// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertStringIncludes } from "@std/assert";
import {
  buildXhtmlInjectionArgs,
  type XhtmlInjectionOptions,
} from "./injector.ts";

const SCRIPT_PATH = "tools/scripts/xhtml.ts";
const BIN_PATH = "_dist/bin";

function argsFor(options?: XhtmlInjectionOptions): string[] {
  return buildXhtmlInjectionArgs(SCRIPT_PATH, BIN_PATH, options);
}

function assertLoaderPermission(
  options: XhtmlInjectionOptions | undefined,
  expected: boolean,
): void {
  assertEquals(
    argsFor(options).includes("--allow-browser-http-loader"),
    expected,
  );
}

Deno.test("XHTML injection defaults to denying the browser HTTP loader", () => {
  assertEquals(argsFor(), [
    "run",
    "--allow-read",
    "--allow-write",
    SCRIPT_PATH,
    BIN_PATH,
  ]);
  assertLoaderPermission(undefined, false);
});

Deno.test("XHTML injection maps development-only options deterministically", () => {
  assertEquals(
    argsFor({ devPages: true, allowBrowserHttpLoader: true }),
    [
      "run",
      "--allow-read",
      "--allow-write",
      SCRIPT_PATH,
      BIN_PATH,
      "--dev",
      "--allow-browser-http-loader",
    ],
  );
});

Deno.test("stage and production options keep the browser HTTP loader denied", () => {
  assertLoaderPermission(
    { devPages: true, allowBrowserHttpLoader: false },
    false,
  );
  assertLoaderPermission(
    { isCI: true, allowBrowserHttpLoader: false },
    false,
  );
});

Deno.test("feles-build and dev-tool use the explicit CSP option matrix", async () => {
  const felesBuild = await Deno.readTextFile("tools/feles-build.ts");
  const devTool = await Deno.readTextFile("tools/dev-tool.ts");

  for (const functionName of ["runDev", "runTest"]) {
    const start = felesBuild.indexOf(`async function ${functionName}`);
    const end = felesBuild.indexOf("\nasync function ", start + 1);
    const body = felesBuild.slice(start, end === -1 ? undefined : end);
    assertStringIncludes(body, "devPages: true");
    assertStringIncludes(body, "allowBrowserHttpLoader: true");
  }

  const stageStart = felesBuild.indexOf("async function runStage");
  const stageEnd = felesBuild.indexOf("\nasync function ", stageStart + 1);
  const stageBody = felesBuild.slice(stageStart, stageEnd);
  assertStringIncludes(stageBody, "devPages: true");
  assertStringIncludes(stageBody, "allowBrowserHttpLoader: false");

  const afterMachStart = felesBuild.indexOf(
    '} else if (optionsPhase === "after-mach")',
  );
  const afterMachBody = felesBuild.slice(
    afterMachStart,
    felesBuild.indexOf("\n  } else {", afterMachStart),
  );
  assertStringIncludes(afterMachBody, "isCI: true");
  assertStringIncludes(afterMachBody, "allowBrowserHttpLoader: false");

  const rebuildStart = devTool.indexOf("async function cmdRebuild");
  const rebuildEnd = devTool.indexOf("\nasync function ", rebuildStart + 1);
  const rebuildBody = devTool.slice(rebuildStart, rebuildEnd);
  assertStringIncludes(rebuildBody, "devPages: true");
  assertStringIncludes(rebuildBody, "allowBrowserHttpLoader: true");
});
