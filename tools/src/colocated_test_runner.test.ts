// SPDX-License-Identifier: MPL-2.0

import {
  assertEquals,
  assertMatch,
  assertRejects,
  assertThrows,
} from "@std/assert";
import {
  detectLayer,
  escapeRegExp,
  isResultMatchTarget,
  isTestFile,
  normalizeBrowserResultPath,
  parseLayer,
} from "./colocated_test_utils.ts";
import {
  assertWindowsAutoStartPreflight,
  captureWindowsReadyBrowser,
  clearBrowserTestControlPrefs,
  createWindowsAutoStartState,
  parseOptions,
  parseWindowsListenerSnapshot,
  parseWindowsProcessSnapshot,
  reconcileWindowsFloorpOwnership,
  selectWindowsDenoIdentity,
  selectWindowsListenerRoot,
  stopWindowsAutoStartedBrowser,
  stopWindowsAutoStartedBrowserWithRootFallback,
  stopWindowsSpawnedChildRootOnly,
  type WindowsProcessControlDeps,
  type WindowsProcessRecord,
  writeBrowserTestControlPrefs,
} from "./colocated_test_runner.ts";

Deno.test("browser detection uses the abort-bounded TCP reachability helper", async () => {
  const source = await Deno.readTextFile(
    new URL("./colocated_test_runner.ts", import.meta.url),
  );
  const helperStart = source.indexOf("async function _isTcpPortReachable(");
  const detectionStart = source.indexOf(
    "async function hasRunningTestBrowser(",
  );
  const detectionEnd = source.indexOf(
    "export interface WindowsProcessRecord",
    detectionStart,
  );

  assertEquals(helperStart >= 0, true);
  assertEquals(detectionStart > helperStart, true);
  assertEquals(detectionEnd > detectionStart, true);

  const helperSource = source.slice(helperStart, detectionStart);
  const detectionSource = source.slice(detectionStart, detectionEnd);
  assertMatch(
    helperSource,
    /Deno\.connect\(\{[\s\S]*signal:\s*AbortSignal\.timeout\(timeoutMs\)/,
  );
  assertMatch(
    helperSource,
    /try\s*\{\s*return true;\s*\}\s*finally\s*\{\s*conn\.close\(\);\s*\}/,
  );
  assertMatch(
    detectionSource,
    /return await _isTcpPortReachable\(port\);/,
  );
  assertEquals(detectionSource.includes("Deno.connect"), false);
});

Deno.test("isTestFile detects supported test patterns", () => {
  assertEquals(isTestFile("foo/bar.test.ts"), true);
  assertEquals(isTestFile("foo/bar.spec.ts"), true);
  assertEquals(isTestFile("foo/bar.test.mts"), true);
  assertEquals(isTestFile("foo/bar.spec.mjs"), true);
  assertEquals(isTestFile("foo/bar.test.tsx"), true);
  assertEquals(isTestFile("foo/bar.test.jsx"), true);
  assertEquals(isTestFile("foo/test/unit/something.test.ts"), true);
  assertEquals(isTestFile("foo\\bar.test.ts"), true);
});

Deno.test("isTestFile ignores non-test and excluded paths", () => {
  assertEquals(isTestFile("foo/bar.ts"), false);
  assertEquals(isTestFile("foo/test/index.ts"), false);
  assertEquals(isTestFile("foo/test/helper.ts"), false);
  assertEquals(isTestFile("foo/test/setup.ts"), false);
  assertEquals(isTestFile("foo/_dist/bar.test.ts"), false);
  assertEquals(isTestFile("foo/node_modules/bar.test.ts"), false);
  assertEquals(isTestFile("foo/libs/@types/gecko/foo.test.ts"), false);
  assertEquals(isTestFile("foo\\_dist\\bar.test.ts"), false);
});

Deno.test("detectLayer resolves known layer roots", () => {
  assertEquals(
    detectLayer("browser-features/chrome/common/foo.test.ts"),
    "chrome",
  );
  assertEquals(
    detectLayer("browser-features/pages-llm-chat/foo.test.ts"),
    "pages",
  );
  assertEquals(
    detectLayer("browser-features/modules/modules/foo.test.mts"),
    "esm",
  );
  assertEquals(detectLayer("bridge/loader-features/loader/foo.test.ts"), "esm");
  assertEquals(
    detectLayer("browser-features\\chrome\\test\\foo.test.ts"),
    "chrome",
  );
});

Deno.test("detectLayer returns null for unknown paths", () => {
  assertEquals(detectLayer("tools/src/foo.test.ts"), null);
  assertEquals(detectLayer("libs/some-lib/foo.test.ts"), null);
});

Deno.test("parseLayer accepts aliases and rejects invalid values", () => {
  assertEquals(parseLayer(undefined), "all");
  assertEquals(parseLayer("all"), "all");
  assertEquals(parseLayer("chrome"), "chrome");
  assertEquals(parseLayer("Chrome"), "chrome");
  assertEquals(parseLayer("esm"), "esm");
  assertEquals(parseLayer("pages"), "pages");
  assertEquals(parseLayer("page"), "pages");
  assertEquals(parseLayer("built-in-pages"), "pages");
  assertEquals(parseLayer("builtin-pages"), "pages");
  assertEquals(parseLayer("builtin"), "pages");

  assertThrows(() => parseLayer("invalid"), Error, "Invalid --layer value");
});

Deno.test("escapeRegExp escapes regex control characters", () => {
  assertEquals(escapeRegExp("foo.bar"), "foo\\.bar");
  assertEquals(escapeRegExp("file[1]"), "file\\[1\\]");
  assertEquals(escapeRegExp("a+b(c)"), "a\\+b\\(c\\)");
  assertEquals(escapeRegExp("normal"), "normal");
  assertEquals(escapeRegExp("a*b?c"), "a\\*b\\?c");
  assertEquals(escapeRegExp("$100"), "\\$100");
});

Deno.test("normalizeBrowserResultPath maps known aliases and prefixes", () => {
  assertEquals(
    normalizeBrowserResultPath("#features-chrome/common/test/foo.test.ts"),
    "browser-features/chrome/common/test/foo.test.ts",
  );
  assertEquals(
    normalizeBrowserResultPath("#features-modules/modules/foo.test.mts"),
    "browser-features/modules/modules/foo.test.mts",
  );
  assertEquals(
    normalizeBrowserResultPath("/link-features-chrome/common/foo.test.ts"),
    "browser-features/chrome/common/foo.test.ts",
  );
  assertEquals(
    normalizeBrowserResultPath("C:\\repo\\browser-features\\chrome\\x.test.ts"),
    "browser-features/chrome/x.test.ts",
  );
  assertEquals(
    normalizeBrowserResultPath("#/unexpected-alias/path.test.ts"),
    "[unknown-alias] #/unexpected-alias/path.test.ts",
  );
});

Deno.test("isResultMatchTarget matches normalized browser paths", () => {
  assertEquals(
    isResultMatchTarget(
      "#features-chrome/common/foo.test.ts",
      "browser-features/chrome/common/foo.test.ts",
    ),
    true,
  );
  assertEquals(
    isResultMatchTarget(
      "C:\\workspace\\browser-features\\modules\\x.test.mts",
      "browser-features/modules/x.test.mts",
    ),
    true,
  );
  assertEquals(
    isResultMatchTarget(
      "#/unknown/foo.test.ts",
      "browser-features/chrome/common/foo.test.ts",
    ),
    false,
  );
});

Deno.test("parseOptions applies default timeout values", () => {
  const options = parseOptions([]);

  assertEquals(options.near, undefined);
  assertEquals(options.listOnly, false);
  assertEquals(options.layer, "all");
  assertEquals(options.autoStart, true);
  assertEquals(options.timeoutMs, 1_800_000);
  assertEquals(options.startupTimeoutMs, 1_800_000);
  assertEquals(options.help, false);
});

Deno.test("parseOptions accepts explicit timeout and startup values", () => {
  const options = parseOptions([
    "browser-features/chrome",
    "--layer",
    "chrome",
    "--timeout-ms",
    "450000",
    "--startup-timeout-ms=360000",
  ]);

  assertEquals(options.near, "browser-features/chrome");
  assertEquals(options.layer, "chrome");
  assertEquals(options.timeoutMs, 450_000);
  assertEquals(options.startupTimeoutMs, 360_000);
});

Deno.test("parseOptions rejects unknown options", () => {
  assertThrows(
    () => parseOptions(["--timeout", "100"]),
    Error,
    "Unknown option: --timeout",
  );
});

Deno.test("parseOptions rejects invalid timeout values", () => {
  assertThrows(
    () => parseOptions(["--timeout-ms", "abc"]),
    Error,
    "Invalid --timeout-ms value",
  );

  assertThrows(
    () => parseOptions(["--startup-timeout-ms", "0"]),
    Error,
    "Invalid --startup-timeout-ms value",
  );

  assertThrows(
    () => parseOptions(["--timeout-ms", "1800001"]),
    Error,
    "Invalid --timeout-ms value",
  );
});

Deno.test("parseOptions rejects --near with positional path", () => {
  assertThrows(
    () => parseOptions(["foo", "--near", "bar"]),
    Error,
    "Use either --near or a positional path",
  );
});

Deno.test("parseOptions supports help mode", () => {
  const options = parseOptions(["--help"]);
  assertEquals(options.help, true);
  assertEquals(options.timeoutMs, 1_800_000);
  assertEquals(options.startupTimeoutMs, 1_800_000);
});

Deno.test("browser test control is versioned, expiring, and cleared by owner", async () => {
  const root = await Deno.makeTempDir();
  try {
    const profileDir = `${root}/profile/test`;
    await Deno.mkdir(profileDir, { recursive: true });
    await Deno.writeTextFile(
      `${profileDir}/prefs.js`,
      'user_pref("floorp.unrelated", "keep");\n' +
        'user_pref("nora.tests.filter.9", "stale");\n',
    );

    const runId = "current-run";
    const expiresAtMs = 1_900_000_000_000;
    const filter = ["browser-features/chrome/test/current.test.ts"];
    writeBrowserTestControlPrefs(
      filter,
      runId,
      expiresAtMs,
      profileDir,
    );

    assertEquals(
      JSON.parse(
        await Deno.readTextFile(
          `${profileDir}/nora-tests-control.json`,
        ),
      ),
      { schemaVersion: 1, runId, expiresAtMs, filter },
    );
    const writtenPrefs = await Deno.readTextFile(`${profileDir}/prefs.js`);
    assertEquals(writtenPrefs.includes('user_pref("floorp.unrelated"'), true);
    assertEquals(writtenPrefs.includes(`user_pref("nora.tests.run_id"`), true);
    assertEquals(
      writtenPrefs.includes(`user_pref("nora.tests.filter.0"`),
      true,
    );

    clearBrowserTestControlPrefs("different-run", profileDir);
    assertEquals(
      (await Deno.stat(`${profileDir}/nora-tests-control.json`)).isFile,
      true,
    );

    clearBrowserTestControlPrefs(runId, profileDir);
    const cleanedPrefs = await Deno.readTextFile(`${profileDir}/prefs.js`);
    assertEquals(cleanedPrefs.includes('user_pref("floorp.unrelated"'), true);
    assertEquals(cleanedPrefs.includes("nora.tests."), false);
    await assertRejects(
      () => Deno.stat(`${profileDir}/nora-tests-control.json`),
      Deno.errors.NotFound,
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("host cleanup removes its control file even when prefs cleanup fails", async () => {
  const root = await Deno.makeTempDir();
  try {
    const profileDir = `${root}/profile/test`;
    await Deno.mkdir(`${profileDir}/prefs.js`, { recursive: true });
    await Deno.writeTextFile(
      `${profileDir}/nora-tests-control.json`,
      JSON.stringify({
        schemaVersion: 1,
        runId: "owned-run",
        expiresAtMs: 1_900_000_000_000,
        filter: [],
      }),
    );

    assertThrows(
      () => clearBrowserTestControlPrefs("owned-run", profileDir),
      Error,
      "cleanup was incomplete",
    );
    await assertRejects(
      () => Deno.stat(`${profileDir}/nora-tests-control.json`),
      Deno.errors.NotFound,
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

const TEST_DENO_EXE = "C:\\tools\\deno.exe";
const TEST_FLOORP_EXE = "E:\\repo\\_dist\\bin\\floorp\\floorp.exe";

function windowsProcess(
  processId: number,
  parentProcessId: number,
  executablePath: string | null,
  commandLine: string | null,
  creationDate = `created-${processId}`,
): WindowsProcessRecord {
  return {
    processId,
    parentProcessId,
    creationDate,
    executablePath,
    commandLine,
  };
}

function denoProcess(): WindowsProcessRecord {
  return windowsProcess(
    100,
    10,
    TEST_DENO_EXE,
    '"C:\\tools\\deno.exe" task feles-build test',
  );
}

Deno.test("Windows snapshot parsers accept singleton and array JSON shapes", () => {
  const singleton = JSON.stringify({
    ProcessId: 100,
    ParentProcessId: 10,
    CreationDate: "opaque-cim-date",
    ExecutablePath: TEST_DENO_EXE,
    CommandLine: "deno task feles-build test",
  });
  assertEquals(parseWindowsProcessSnapshot(singleton), [
    {
      processId: 100,
      parentProcessId: 10,
      creationDate: "opaque-cim-date",
      executablePath: TEST_DENO_EXE,
      commandLine: "deno task feles-build test",
    },
  ]);

  const listeners = JSON.stringify([
    {
      LocalAddress: "127.0.0.1",
      LocalPort: 2828,
      State: "Listen",
      OwningProcess: 200,
    },
    {
      LocalAddress: "::1",
      LocalPort: 2828,
      State: "Listen",
      OwningProcess: 200,
    },
  ]);
  assertEquals(parseWindowsListenerSnapshot(listeners).length, 2);
});

Deno.test("Windows snapshot parsers fail closed on blank, null, and missing fields", () => {
  assertThrows(
    () => parseWindowsProcessSnapshot(""),
    Error,
    "empty response",
  );
  assertThrows(
    () => parseWindowsProcessSnapshot("null"),
    Error,
    "returned null",
  );
  assertThrows(
    () =>
      parseWindowsProcessSnapshot(
        JSON.stringify({
          ProcessId: 1,
          ParentProcessId: 0,
          CreationDate: "created",
          ExecutablePath: TEST_DENO_EXE,
        }),
      ),
    Error,
    "missing CommandLine",
  );
  assertThrows(
    () => parseWindowsListenerSnapshot(" "),
    Error,
    "empty response",
  );
});

Deno.test("Windows preflight rejects the exact test executable only", () => {
  assertThrows(
    () => assertWindowsAutoStartPreflight([], TEST_FLOORP_EXE),
    Error,
    "returned no processes",
  );
  assertThrows(
    () =>
      assertWindowsAutoStartPreflight(
        [
          denoProcess(),
          windowsProcess(
            200,
            100,
            TEST_FLOORP_EXE.toUpperCase(),
            `"${TEST_FLOORP_EXE}"`,
          ),
        ],
        TEST_FLOORP_EXE,
      ),
    Error,
    "Refusing auto-start",
  );

  assertWindowsAutoStartPreflight(
    [
      denoProcess(),
      windowsProcess(
        300,
        10,
        "C:\\Program Files\\Ablaze Floorp\\floorp.exe",
        '"C:\\Program Files\\Ablaze Floorp\\floorp.exe"',
      ),
    ],
    TEST_FLOORP_EXE,
  );
});

Deno.test("Windows Deno capture requires path, opaque creation date, and task marker", () => {
  const identity = selectWindowsDenoIdentity(
    [denoProcess()],
    100,
    TEST_DENO_EXE,
  );
  assertEquals(identity.creationDate, "created-100");

  assertThrows(
    () =>
      selectWindowsDenoIdentity(
        [windowsProcess(100, 10, TEST_DENO_EXE, "deno task other")],
        100,
        TEST_DENO_EXE,
      ),
    Error,
    "expected task marker",
  );
});

Deno.test("Windows listener ownership accepts dual-stack rows for one verified root", () => {
  const deno = denoProcess();
  const intermediate = windowsProcess(
    150,
    100,
    "C:\\tools\\helper.exe",
    "helper.exe",
  );
  const root = windowsProcess(
    200,
    150,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}"`,
  );
  const identity = selectWindowsListenerRoot(
    2828,
    [
      {
        localAddress: "127.0.0.1",
        localPort: 2828,
        state: "Listen",
        owningProcess: 200,
      },
      {
        localAddress: "::1",
        localPort: 2828,
        state: "Listen",
        owningProcess: 200,
      },
    ],
    [deno, intermediate, root],
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  assertEquals(identity.processId, 200);
});

Deno.test("Windows listener ownership rejects multiple owners and content roots", () => {
  const deno = denoProcess();
  const denoIdentity = selectWindowsDenoIdentity(
    [deno],
    100,
    TEST_DENO_EXE,
  );
  const root = windowsProcess(
    200,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}"`,
  );
  const other = windowsProcess(
    201,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}"`,
  );
  assertThrows(
    () =>
      selectWindowsListenerRoot(
        2828,
        [
          {
            localAddress: "127.0.0.1",
            localPort: 2828,
            state: "Listen",
            owningProcess: 200,
          },
          {
            localAddress: "::1",
            localPort: 2828,
            state: "Listen",
            owningProcess: 201,
          },
        ],
        [deno, root, other],
        denoIdentity,
        TEST_FLOORP_EXE,
      ),
    Error,
    "exactly one owner",
  );

  const contentRoot = windowsProcess(
    200,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}" -contentproc -parentPid 199`,
  );
  assertThrows(
    () =>
      selectWindowsListenerRoot(
        2828,
        [{
          localAddress: "127.0.0.1",
          localPort: 2828,
          state: "Listen",
          owningProcess: 200,
        }],
        [deno, contentRoot],
        denoIdentity,
        TEST_FLOORP_EXE,
      ),
    Error,
    "content process",
  );

  const brokenRoot = windowsProcess(
    200,
    4,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}"`,
  );
  assertThrows(
    () =>
      selectWindowsListenerRoot(
        2828,
        [{
          localAddress: "127.0.0.1",
          localPort: 2828,
          state: "Listen",
          owningProcess: 200,
        }],
        [deno, brokenRoot],
        denoIdentity,
        TEST_FLOORP_EXE,
      ),
    Error,
    "not descended",
  );

  const missingCreationDate = { ...root, creationDate: null };
  assertThrows(
    () =>
      selectWindowsListenerRoot(
        2828,
        [{
          localAddress: "127.0.0.1",
          localPort: 2828,
          state: "Listen",
          owningProcess: 200,
        }],
        [deno, missingCreationDate],
        denoIdentity,
        TEST_FLOORP_EXE,
      ),
    Error,
    "has no CreationDate",
  );
});

Deno.test("Windows listener ownership accepts only a previously captured reparented root", () => {
  const deno = denoProcess();
  const originalRoot = windowsProcess(
    200,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}"`,
    "same-root-identity",
  );
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  reconcileWindowsFloorpOwnership(
    state,
    [deno, originalRoot],
    TEST_FLOORP_EXE,
  );
  const reparentedRoot = { ...originalRoot, parentProcessId: 4 };
  const listeners = [{
    localAddress: "127.0.0.1",
    localPort: 2828,
    state: "Listen",
    owningProcess: 200,
  }];

  const accepted = selectWindowsListenerRoot(
    2828,
    listeners,
    [deno, reparentedRoot],
    state.deno,
    TEST_FLOORP_EXE,
    new Map(state.ownedFloorp),
  );
  assertEquals(accepted.processId, 200);
  assertEquals(accepted.creationDate, "same-root-identity");

  assertThrows(
    () =>
      selectWindowsListenerRoot(
        2828,
        listeners,
        [deno, reparentedRoot],
        state.deno,
        TEST_FLOORP_EXE,
      ),
    Error,
    "no matching previously-owned identity",
  );
});

Deno.test("Windows readiness preserves the verified launcher ancestor of the listener root", async () => {
  const deno = denoProcess();
  const launcher = windowsProcess(
    200,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}" --marionette`,
  );
  const listenerRoot = windowsProcess(
    201,
    200,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}" --marionette`,
  );
  const content = windowsProcess(
    202,
    201,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}" -contentproc -parentPid 201`,
  );
  const processes = [deno, launcher, listenerRoot, content];
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  assertEquals(
    reconcileWindowsFloorpOwnership(
      state,
      [deno, launcher],
      TEST_FLOORP_EXE,
    ),
    [],
  );
  assertEquals(
    Array.from(state.ownedFloorp.keys()).sort((left, right) => left - right),
    [200],
  );

  const deps: WindowsProcessControlDeps = {
    listProcesses: () => Promise.resolve(processes),
    listListeners: () =>
      Promise.resolve([{
        localAddress: "127.0.0.1",
        localPort: 2828,
        state: "Listen",
        owningProcess: 201,
      }]),
    taskkill: () => Promise.resolve({ success: true, code: 0 }),
    isPortReachable: () => Promise.resolve(false),
    sleep: () => Promise.resolve(),
  };

  await captureWindowsReadyBrowser(state, 2828, deps);

  assertEquals(state.listenerRoot?.processId, 201);
  assertEquals(
    Array.from(state.ownedFloorp.keys()).sort((left, right) => left - right),
    [200, 201, 202],
  );
  assertEquals(state.blockedFloorpProcessIds.size, 0);
  assertEquals(state.ambiguousFloorp, []);
});

Deno.test("Windows readiness owns an exact-path sibling currently descended from verified Deno", async () => {
  const deno = denoProcess();
  const listenerRoot = windowsProcess(
    200,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}" --marionette`,
  );
  const sibling = windowsProcess(
    300,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}" --marionette`,
  );
  const processes = [deno, listenerRoot, sibling];
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  const deps: WindowsProcessControlDeps = {
    listProcesses: () => Promise.resolve(processes),
    listListeners: () =>
      Promise.resolve([{
        localAddress: "127.0.0.1",
        localPort: 2828,
        state: "Listen",
        owningProcess: 200,
      }]),
    taskkill: () => Promise.resolve({ success: true, code: 0 }),
    isPortReachable: () => Promise.resolve(false),
    sleep: () => Promise.resolve(),
  };

  await captureWindowsReadyBrowser(state, 2828, deps);

  assertEquals(
    Array.from(state.ownedFloorp.keys()).sort((left, right) => left - right),
    [200, 300],
  );
  assertEquals(state.ambiguousFloorp, []);
  assertEquals(state.treeKillSafe, true);
});

Deno.test("Windows readiness blocks a present previously-owned identity mismatch", async () => {
  const deno = denoProcess();
  const originalLauncher = windowsProcess(
    200,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}" --marionette`,
  );
  const listenerRoot = windowsProcess(
    201,
    200,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}" --marionette`,
  );
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  reconcileWindowsFloorpOwnership(
    state,
    [deno, originalLauncher, listenerRoot],
    TEST_FLOORP_EXE,
  );

  const changedLauncher = {
    ...originalLauncher,
    commandLine: `"${TEST_FLOORP_EXE}" --changed`,
  };
  const currentProcesses = [deno, changedLauncher, listenerRoot];
  const deps: WindowsProcessControlDeps = {
    listProcesses: () => Promise.resolve(currentProcesses),
    listListeners: () =>
      Promise.resolve([{
        localAddress: "127.0.0.1",
        localPort: 2828,
        state: "Listen",
        owningProcess: 201,
      }]),
    taskkill: () => Promise.resolve({ success: true, code: 0 }),
    isPortReachable: () => Promise.resolve(false),
    sleep: () => Promise.resolve(),
  };

  await assertRejects(
    () => captureWindowsReadyBrowser(state, 2828, deps),
    Error,
    "Previously owned Floorp PID 200 no longer matches",
  );
  assertEquals(state.blockedFloorpProcessIds.has(200), true);
  assertEquals(state.ownedFloorp.has(200), false);

  reconcileWindowsFloorpOwnership(
    state,
    currentProcesses,
    TEST_FLOORP_EXE,
  );
  assertEquals(state.ownedFloorp.has(200), false);
  assertEquals(
    state.ambiguousFloorp.some((message) => message.includes("PID 200")),
    true,
  );
});

Deno.test("Windows ownership expands exact-path children by ancestry and -parentPid", () => {
  const deno = denoProcess();
  const root = windowsProcess(
    200,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}"`,
  );
  const content = windowsProcess(
    201,
    4,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}" -contentproc -parentPid 200`,
  );
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  assertEquals(
    reconcileWindowsFloorpOwnership(
      state,
      [deno, root, content],
      TEST_FLOORP_EXE,
    ),
    [],
  );
  assertEquals(Array.from(state.ownedFloorp.keys()).sort(), [200, 201]);
  assertEquals(state.ambiguousFloorp, []);
});

Deno.test("Windows ownership treats a second root outside the listener tree as ambiguous", () => {
  const deno = denoProcess();
  const listenerRoot = windowsProcess(
    200,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}"`,
  );
  const secondRoot = windowsProcess(
    300,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}"`,
  );
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  const rootIdentity = selectWindowsListenerRoot(
    2828,
    [{
      localAddress: "127.0.0.1",
      localPort: 2828,
      state: "Listen",
      owningProcess: 200,
    }],
    [deno, listenerRoot, secondRoot],
    state.deno,
    TEST_FLOORP_EXE,
  );
  state.listenerRoot = rootIdentity;
  state.ownedFloorp.set(rootIdentity.processId, rootIdentity);

  reconcileWindowsFloorpOwnership(
    state,
    [deno, listenerRoot, secondRoot],
    TEST_FLOORP_EXE,
  );

  assertEquals(Array.from(state.ownedFloorp.keys()), [200]);
  assertEquals(state.ambiguousFloorp, [
    "Unlinked exact-path Floorp PID 300",
  ]);
});

Deno.test("Windows ownership never grants a new child through a stale -parentPid", () => {
  const deno = denoProcess();
  const parent = windowsProcess(
    200,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}"`,
    "parent-original",
  );
  const newChild = windowsProcess(
    201,
    4,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}" -contentproc -parentPid 200`,
  );

  const missingParentState = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  reconcileWindowsFloorpOwnership(
    missingParentState,
    [deno, parent],
    TEST_FLOORP_EXE,
  );
  missingParentState.listenerRoot = missingParentState.ownedFloorp.get(200) ??
    null;
  reconcileWindowsFloorpOwnership(
    missingParentState,
    [deno, newChild],
    TEST_FLOORP_EXE,
  );
  assertEquals(missingParentState.ownedFloorp.has(201), false);
  assertEquals(
    missingParentState.ambiguousFloorp.includes(
      "Unlinked exact-path Floorp PID 201",
    ),
    true,
  );

  const reusedParentState = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  reconcileWindowsFloorpOwnership(
    reusedParentState,
    [deno, parent],
    TEST_FLOORP_EXE,
  );
  reusedParentState.listenerRoot = reusedParentState.ownedFloorp.get(200) ??
    null;
  const reusedParent = { ...parent, creationDate: "parent-reused" };
  const reusedIssues = reconcileWindowsFloorpOwnership(
    reusedParentState,
    [deno, reusedParent, newChild],
    TEST_FLOORP_EXE,
  );
  assertEquals(
    reusedIssues.some((issue) => issue.includes("no longer matches")),
    true,
  );
  assertEquals(reusedParentState.ownedFloorp.has(201), false);

  const alreadyOwnedState = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  reconcileWindowsFloorpOwnership(
    alreadyOwnedState,
    [deno, parent, newChild],
    TEST_FLOORP_EXE,
  );
  alreadyOwnedState.listenerRoot = alreadyOwnedState.ownedFloorp.get(200) ??
    null;
  reconcileWindowsFloorpOwnership(
    alreadyOwnedState,
    [deno, newChild],
    TEST_FLOORP_EXE,
  );
  assertEquals(alreadyOwnedState.ownedFloorp.has(201), true);
});

Deno.test("Windows teardown always cleans verified survivors after diagnostic root taskkill", async () => {
  const deno = denoProcess();
  const root = windowsProcess(
    200,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}"`,
  );
  const content = windowsProcess(
    201,
    200,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}" -contentproc -parentPid 200`,
  );
  const live = new Map(
    [deno, root, content].map((process) => [process.processId, process]),
  );
  const killed: number[] = [];
  let checkedPort: number | null = null;
  const deps: WindowsProcessControlDeps = {
    listProcesses: () =>
      Promise.resolve(
        Array.from(live.values()).map((process) => ({
          ...process,
        })),
      ),
    listListeners: () => Promise.resolve([]),
    taskkill: (processId) => {
      killed.push(processId);
      live.delete(processId);
      return Promise.resolve({ success: true, code: 0 });
    },
    isPortReachable: (port) => {
      checkedPort = port;
      return Promise.resolve(false);
    },
    sleep: () => Promise.resolve(),
  };
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  reconcileWindowsFloorpOwnership(
    state,
    [deno, root, content],
    TEST_FLOORP_EXE,
  );
  state.listenerRoot = state.ownedFloorp.get(200) ?? null;
  state.port = 2828;

  await stopWindowsAutoStartedBrowser(state, deps);

  assertEquals(killed, [100, 200, 201]);
  assertEquals(checkedPort, 2828);
  assertEquals(live.size, 0);
});

Deno.test("Windows teardown records but never kills an unlinked exact-path process", async () => {
  const deno = denoProcess();
  const unrelated = windowsProcess(
    999,
    4,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}"`,
  );
  const live = new Map(
    [deno, unrelated].map((process) => [process.processId, process]),
  );
  const killed: number[] = [];
  const deps: WindowsProcessControlDeps = {
    listProcesses: () => Promise.resolve(Array.from(live.values())),
    listListeners: () => Promise.resolve([]),
    taskkill: (processId) => {
      killed.push(processId);
      live.delete(processId);
      return Promise.resolve({ success: true, code: 0 });
    },
    isPortReachable: () => Promise.resolve(false),
    sleep: () => Promise.resolve(),
  };
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );

  await assertRejects(
    () => stopWindowsAutoStartedBrowser(state, deps),
    Error,
    "Unlinked exact-path Floorp PID 999",
  );
  assertEquals(killed, [100]);
  assertEquals(live.has(999), true);
});

Deno.test("Windows teardown disables Deno tree kill when a sibling is ambiguous", async () => {
  const deno = denoProcess();
  const listenerRoot = windowsProcess(
    200,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}"`,
  );
  const sibling = windowsProcess(
    300,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}"`,
  );
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  const listenerIdentity = selectWindowsListenerRoot(
    2828,
    [{
      localAddress: "127.0.0.1",
      localPort: 2828,
      state: "Listen",
      owningProcess: 200,
    }],
    [deno, listenerRoot],
    state.deno,
    TEST_FLOORP_EXE,
  );
  state.listenerRoot = listenerIdentity;
  state.ownedFloorp.set(200, listenerIdentity);

  const live = new Map(
    [deno, listenerRoot, sibling].map((
      process,
    ) => [process.processId, process]),
  );
  const kills: Array<{ processId: number; includeTree: boolean }> = [];
  const deps: WindowsProcessControlDeps = {
    listProcesses: () => Promise.resolve(Array.from(live.values())),
    listListeners: () => Promise.resolve([]),
    taskkill: (processId, includeTree) => {
      kills.push({ processId, includeTree });
      live.delete(processId);
      if (processId === 100 && includeTree) {
        live.delete(200);
        live.delete(300);
      }
      return Promise.resolve({ success: true, code: 0 });
    },
    isPortReachable: () => Promise.resolve(false),
    sleep: () => Promise.resolve(),
  };

  await assertRejects(
    () => stopWindowsAutoStartedBrowser(state, deps),
    Error,
    "Unlinked exact-path Floorp PID 300",
  );
  assertEquals(kills, [
    { processId: 100, includeTree: false },
    { processId: 200, includeTree: false },
  ]);
  assertEquals(live.has(300), true);
  assertEquals(state.treeKillSafe, false);
});

Deno.test("Windows teardown revalidates Floorp identity immediately before taskkill", async () => {
  const deno = denoProcess();
  const root = windowsProcess(
    200,
    100,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}"`,
    "root-original",
  );
  const reusedRoot = windowsProcess(
    200,
    4,
    TEST_FLOORP_EXE,
    `"${TEST_FLOORP_EXE}"`,
    "root-reused",
  );
  let listCall = 0;
  let denoKilled = false;
  const killed: number[] = [];
  const deps: WindowsProcessControlDeps = {
    listProcesses: () => {
      listCall++;
      if (listCall <= 2) {
        return Promise.resolve([deno, root]);
      }
      if (!denoKilled && listCall === 3) {
        return Promise.resolve([deno, root]);
      }
      if (listCall === 3) {
        return Promise.resolve([root]);
      }
      return Promise.resolve([reusedRoot]);
    },
    listListeners: () => Promise.resolve([]),
    taskkill: (processId) => {
      killed.push(processId);
      if (processId === 100) {
        denoKilled = true;
      }
      return Promise.resolve({ success: true, code: 0 });
    },
    isPortReachable: () => Promise.resolve(false),
    sleep: () => Promise.resolve(),
  };
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  reconcileWindowsFloorpOwnership(
    state,
    [deno, root],
    TEST_FLOORP_EXE,
  );

  await assertRejects(
    () => stopWindowsAutoStartedBrowser(state, deps),
    Error,
    "identity changed before taskkill",
  );
  assertEquals(killed, [100]);
});

Deno.test("Windows teardown revalidates the captured Deno command line before taskkill", async () => {
  const deno = denoProcess();
  const changedDeno = windowsProcess(
    100,
    10,
    TEST_DENO_EXE,
    '"C:\\tools\\deno.exe" task feles-build stage',
  );
  let listCall = 0;
  const killed: number[] = [];
  const deps: WindowsProcessControlDeps = {
    listProcesses: () => {
      listCall++;
      return Promise.resolve(listCall === 1 ? [deno] : [changedDeno]);
    },
    listListeners: () => Promise.resolve([]),
    taskkill: (processId) => {
      killed.push(processId);
      return Promise.resolve({ success: true, code: 0 });
    },
    isPortReachable: () => Promise.resolve(false),
    sleep: () => Promise.resolve(),
  };
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );

  await assertRejects(
    () => stopWindowsAutoStartedBrowser(state, deps),
    Error,
    "captured identity no longer matches",
  );
  assertEquals(killed, []);
});

Deno.test("Windows teardown fails when the captured port remains reachable", async () => {
  const deno = denoProcess();
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  state.port = 2828;
  const deps: WindowsProcessControlDeps = {
    listProcesses: () => Promise.resolve([]),
    listListeners: () => Promise.resolve([]),
    taskkill: () => Promise.resolve({ success: true, code: 0 }),
    isPortReachable: () => Promise.resolve(true),
    sleep: () => Promise.resolve(),
  };

  await assertRejects(
    () => stopWindowsAutoStartedBrowser(state, deps),
    Error,
    "Captured Marionette port 2828 remained reachable",
  );
});

Deno.test("Windows teardown detects an IPv6 listener even when IPv4 connect fails", async () => {
  const deno = denoProcess();
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  state.port = 2828;
  const deps: WindowsProcessControlDeps = {
    listProcesses: () => Promise.resolve([]),
    listListeners: () =>
      Promise.resolve([{
        localAddress: "::1",
        localPort: 2828,
        state: "Listen",
        owningProcess: 999,
      }]),
    taskkill: () => Promise.resolve({ success: true, code: 0 }),
    isPortReachable: () => Promise.resolve(false),
    sleep: () => Promise.resolve(),
  };

  await assertRejects(
    () => stopWindowsAutoStartedBrowser(state, deps),
    Error,
    "still has 1 listener row",
  );
});

Deno.test("Windows teardown fails closed when the final listener query fails", async () => {
  const deno = denoProcess();
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  state.port = 2828;
  const deps: WindowsProcessControlDeps = {
    listProcesses: () => Promise.resolve([]),
    listListeners: () => Promise.reject(new Error("listener query failed")),
    taskkill: () => Promise.resolve({ success: true, code: 0 }),
    isPortReachable: () => Promise.resolve(false),
    sleep: () => Promise.resolve(),
  };

  await assertRejects(
    () => stopWindowsAutoStartedBrowser(state, deps),
    Error,
    "Could not query listeners",
  );
});

Deno.test("Windows root-only fallback uses the spawned child control", async () => {
  let killCount = 0;
  await stopWindowsSpawnedChildRootOnly(
    {
      processId: 100,
      killRoot: () => {
        killCount++;
      },
      status: Promise.resolve({ code: 1 }),
    },
    () => Promise.resolve(),
  );
  assertEquals(killCount, 1);
});

Deno.test("Windows verified teardown failure invokes root-only child fallback and stays failed", async () => {
  const deno = denoProcess();
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  const deps: WindowsProcessControlDeps = {
    listProcesses: () => Promise.reject(new Error("process query failed")),
    listListeners: () => Promise.resolve([]),
    taskkill: () => Promise.resolve({ success: true, code: 0 }),
    isPortReachable: () => Promise.resolve(false),
    sleep: () => Promise.resolve(),
  };
  let killCount = 0;

  await assertRejects(
    () =>
      stopWindowsAutoStartedBrowserWithRootFallback(
        state,
        deps,
        {
          processId: 100,
          killRoot: () => {
            killCount++;
          },
          status: Promise.resolve({ code: 1 }),
        },
        undefined,
        () => Promise.resolve(),
      ),
    Error,
    "process query failed; spawned Deno root-only fallback completed",
  );
  assertEquals(killCount, 1);
});

Deno.test("Windows verified teardown combines root-only fallback failure", async () => {
  const deno = denoProcess();
  const state = createWindowsAutoStartState(
    selectWindowsDenoIdentity([deno], 100, TEST_DENO_EXE),
    TEST_FLOORP_EXE,
  );
  const deps: WindowsProcessControlDeps = {
    listProcesses: () => Promise.reject(new Error("process query failed")),
    listListeners: () => Promise.resolve([]),
    taskkill: () => Promise.resolve({ success: true, code: 0 }),
    isPortReachable: () => Promise.resolve(false),
    sleep: () => Promise.resolve(),
  };
  let killCount = 0;

  await assertRejects(
    () =>
      stopWindowsAutoStartedBrowserWithRootFallback(
        state,
        deps,
        {
          processId: 100,
          killRoot: () => {
            killCount++;
          },
          status: new Promise<never>(() => {}),
        },
        undefined,
        () => Promise.resolve(),
      ),
    Error,
    "process query failed; spawned Deno root-only fallback failed",
  );
  assertEquals(killCount, 1);
});
