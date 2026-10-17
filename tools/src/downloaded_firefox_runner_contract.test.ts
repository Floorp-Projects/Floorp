// SPDX-License-Identifier: MPL-2.0

import {
  assert,
  assertEquals,
  assertRejects,
  assertStringIncludes,
} from "@std/assert";
import {
  consumeRequestedTestControl,
  default as runBrowserTests,
  type LazyModule,
  runSingleTest,
} from "../../bridge/loader-features/loader/test/index.ts";

declare global {
  interface ImportMeta {
    glob(
      pattern: string | string[],
      options: { eager: true },
    ): Record<string, Record<string, unknown>>;
    glob(
      pattern: string | string[],
      options?: unknown,
    ): Record<string, () => Promise<unknown>>;
  }
}

type PrefValue = boolean | number | string;

class FakePrefs {
  readonly values = new Map<string, PrefValue>();
  saveCount = 0;

  prefHasUserValue(name: string): boolean {
    return this.values.has(name);
  }

  getPrefType(name: string): number {
    const value = this.values.get(name);
    if (typeof value === "boolean") return 128;
    if (typeof value === "number") return 64;
    if (typeof value === "string") return 32;
    return 0;
  }

  getBoolPref(name: string): boolean {
    const value = this.values.get(name);
    if (typeof value !== "boolean") throw new Error(`${name} is not bool`);
    return value;
  }

  getIntPref(name: string): number {
    const value = this.values.get(name);
    if (typeof value !== "number") throw new Error(`${name} is not int`);
    return value;
  }

  getStringPref(name: string, fallback?: string): string {
    const value = this.values.get(name);
    if (typeof value !== "string") {
      if (fallback !== undefined) return fallback;
      throw new Error(`${name} is not string`);
    }
    return value;
  }

  getChildList(prefix: string): string[] {
    return [...this.values.keys()].filter((name) => name.startsWith(prefix));
  }

  setBoolPref(name: string, value: boolean): void {
    this.values.set(name, value);
  }

  setIntPref(name: string, value: number): void {
    this.values.set(name, value);
  }

  setStringPref(name: string, value: string): void {
    this.values.set(name, value);
  }

  clearUserPref(name: string): void {
    this.values.delete(name);
  }

  savePrefFile(_prefFile: string | null): void {
    this.saveCount += 1;
  }
}

async function withFakePrefs(
  prefs: FakePrefs,
  fn: () => Promise<void>,
): Promise<void> {
  const globals = globalThis as Record<string, unknown>;
  const existed = Object.hasOwn(globals, "Services");
  const previous = globals.Services;
  globals.Services = { prefs };
  try {
    await fn();
  } finally {
    if (existed) {
      globals.Services = previous;
    } else {
      delete globals.Services;
    }
  }
}

async function withFakeTestControlEnvironment(
  prefs: FakePrefs,
  profileDir: string,
  fn: () => Promise<void>,
  overrides: {
    remove?: (
      filePath: string,
      options?: { ignoreAbsent?: boolean },
    ) => Promise<void>;
    dirsvc?: unknown;
  } = {},
): Promise<void> {
  const globals = globalThis as Record<string, unknown>;
  const saved = new Map<string, { existed: boolean; value: unknown }>();
  for (const name of ["Services", "PathUtils", "IOUtils"]) {
    saved.set(name, {
      existed: Object.hasOwn(globals, name),
      value: globals[name],
    });
  }
  const services: Record<string, unknown> = { prefs };
  if (Object.hasOwn(overrides, "dirsvc")) {
    services.dirsvc = overrides.dirsvc;
  }
  globals.Services = services;
  globals.PathUtils = {
    profileDir,
    join: (...parts: string[]) => parts.join("/"),
  };
  globals.IOUtils = {
    readUTF8: (filePath: string) => Deno.readTextFile(filePath),
    remove: overrides.remove ??
      ((filePath: string, options?: { ignoreAbsent?: boolean }) =>
        Deno.remove(filePath).catch((error) => {
          if (options?.ignoreAbsent && error instanceof Deno.errors.NotFound) {
            return;
          }
          throw error;
        })),
  };
  try {
    await fn();
  } finally {
    for (const [name, state] of saved) {
      if (state.existed) globals[name] = state.value;
      else delete globals[name];
    }
  }
}

function seedControlPrefs(
  prefs: FakePrefs,
  runId: string,
  filter: string[],
): void {
  prefs.values.set("nora.tests.run_id", runId);
  prefs.values.set("nora.tests.filter", JSON.stringify(filter));
  prefs.values.set("nora.tests.filter.count", String(filter.length));
  filter.forEach((entry, index) => {
    prefs.values.set(`nora.tests.filter.${index}`, entry);
  });
}

Deno.test("current browser test control is consumed exactly once", async () => {
  const root = await Deno.makeTempDir();
  try {
    const prefs = new FakePrefs();
    const runId = "current-run";
    const filter = ["browser-features/chrome/test/current.test.ts"];
    seedControlPrefs(prefs, runId, filter);
    prefs.values.set("floorp.unrelated", "keep");
    await Deno.writeTextFile(
      `${root}/nora-tests-control.json`,
      JSON.stringify({
        schemaVersion: 1,
        runId,
        expiresAtMs: 2_000,
        filter,
      }),
    );

    await withFakeTestControlEnvironment(prefs, root, async () => {
      const consumed = await consumeRequestedTestControl(1_000);
      assertEquals(consumed.runId, runId);
      assertEquals([...consumed.requestedTestFiles], filter);
      assertEquals(prefs.values.get("floorp.unrelated"), "keep");
      assertEquals(
        [...prefs.values.keys()].some((name) => name.startsWith("nora.tests.")),
        false,
      );
      assertEquals(prefs.saveCount, 1);
      await assertRejects(
        () => Deno.stat(`${root}/nora-tests-control.json`),
        Deno.errors.NotFound,
      );

      const second = await consumeRequestedTestControl(1_000);
      assertEquals(second.runId, undefined);
      assertEquals(second.requestedTestFiles.size, 0);
    });
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("test control uses PathUtils.profileDir when dirsvc is absent or throws", async () => {
  for (const variant of ["absent", "throwing"] as const) {
    const root = await Deno.makeTempDir();
    try {
      const prefs = new FakePrefs();
      const runId = `${variant}-dirsvc-run`;
      const filter = [
        `browser-features/chrome/test/${variant}-dirsvc.test.ts`,
      ];
      seedControlPrefs(prefs, runId, filter);
      await Deno.writeTextFile(
        `${root}/nora-tests-control.json`,
        JSON.stringify({
          schemaVersion: 1,
          runId,
          expiresAtMs: 2_000,
          filter,
        }),
      );

      const overrides = variant === "throwing"
        ? {
          dirsvc: {
            get: () => {
              throw new Error("Services.dirsvc must not be used");
            },
          },
        }
        : {};
      await withFakeTestControlEnvironment(
        prefs,
        root,
        async () => {
          const consumed = await consumeRequestedTestControl(1_000);
          assertEquals(consumed.runId, runId, variant);
          assertEquals([...consumed.requestedTestFiles], filter, variant);
        },
        overrides,
      );
    } finally {
      await Deno.remove(root, { recursive: true });
    }
  }
});

Deno.test("stale or mismatched browser test control becomes a full run", async () => {
  for (const variant of ["mismatch", "expired", "malformed"] as const) {
    const root = await Deno.makeTempDir();
    try {
      const prefs = new FakePrefs();
      const filter = ["browser-features/chrome/test/stale.test.ts"];
      seedControlPrefs(prefs, "prefs-run", filter);
      const body = variant === "malformed" ? "{not-json" : JSON.stringify({
        schemaVersion: 1,
        runId: variant === "mismatch" ? "file-run" : "prefs-run",
        expiresAtMs: variant === "expired" ? 999 : 2_000,
        filter,
      });
      await Deno.writeTextFile(`${root}/nora-tests-control.json`, body);

      await withFakeTestControlEnvironment(prefs, root, async () => {
        const consumed = await consumeRequestedTestControl(1_000);
        assertEquals(consumed.runId, undefined, variant);
        assertEquals(consumed.requestedTestFiles.size, 0, variant);
        assertEquals(
          [...prefs.values.keys()].some((name) =>
            name.startsWith("nora.tests.")
          ),
          false,
          variant,
        );
        await assertRejects(
          () => Deno.stat(`${root}/nora-tests-control.json`),
          Deno.errors.NotFound,
        );
      });
    } finally {
      await Deno.remove(root, { recursive: true });
    }
  }
});

Deno.test("browser control cleanup attempts both invalidation channels", async () => {
  const filter = ["browser-features/chrome/test/current.test.ts"];

  const clearFailureRoot = await Deno.makeTempDir();
  try {
    class FailingClearPrefs extends FakePrefs {
      override clearUserPref(_name: string): void {
        throw new Error("injected pref clear failure");
      }
    }
    const prefs = new FailingClearPrefs();
    seedControlPrefs(prefs, "clear-failure", filter);
    await Deno.writeTextFile(
      `${clearFailureRoot}/nora-tests-control.json`,
      JSON.stringify({
        schemaVersion: 1,
        runId: "clear-failure",
        expiresAtMs: 2_000,
        filter,
      }),
    );
    await withFakeTestControlEnvironment(
      prefs,
      clearFailureRoot,
      async () => {
        await assertRejects(
          () => consumeRequestedTestControl(1_000),
          Error,
          "refusing to apply a scoped run",
        );
      },
    );
    await assertRejects(
      () => Deno.stat(`${clearFailureRoot}/nora-tests-control.json`),
      Deno.errors.NotFound,
    );
  } finally {
    await Deno.remove(clearFailureRoot, { recursive: true });
  }

  const removeFailureRoot = await Deno.makeTempDir();
  try {
    const prefs = new FakePrefs();
    seedControlPrefs(prefs, "remove-failure", filter);
    await Deno.writeTextFile(
      `${removeFailureRoot}/nora-tests-control.json`,
      JSON.stringify({
        schemaVersion: 1,
        runId: "remove-failure",
        expiresAtMs: 2_000,
        filter,
      }),
    );
    await withFakeTestControlEnvironment(
      prefs,
      removeFailureRoot,
      async () => {
        await assertRejects(
          () => consumeRequestedTestControl(1_000),
          Error,
          "refusing to apply a scoped run",
        );
      },
      {
        remove: () => Promise.reject(new Error("injected remove failure")),
      },
    );
    assertEquals(
      [...prefs.values.keys()].some((name) => name.startsWith("nora.tests.")),
      false,
    );
  } finally {
    await Deno.remove(removeFailureRoot, { recursive: true });
  }
});

Deno.test("runBrowserTests propagates test control consumption failures", async () => {
  class FailingClearPrefs extends FakePrefs {
    override clearUserPref(_name: string): void {
      throw new Error("injected pref clear failure");
    }
  }

  const root = await Deno.makeTempDir();
  try {
    const prefs = new FailingClearPrefs();
    const runId = "runner-consume-failure";
    const filter = ["browser-features/chrome/test/never-discovered.test.ts"];
    seedControlPrefs(prefs, runId, filter);
    await Deno.writeTextFile(
      `${root}/nora-tests-control.json`,
      JSON.stringify({
        schemaVersion: 1,
        runId,
        expiresAtMs: Date.now() + 60_000,
        filter,
      }),
    );

    await withFakeTestControlEnvironment(prefs, root, async () => {
      await assertRejects(
        () => runBrowserTests(),
        Error,
        "refusing to apply a scoped run",
      );
    });
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

const isolationTarget =
  "tools/workspace-restart-development/no-autodiscovered-suites.test.ts";

const sourceFixtureContext = {
  schemaVersion: 1,
  runId: "qa3-restart-host-fixture",
  manifestSha256: "b".repeat(64),
} as const;

async function withIsolationControl(
  changes: Record<string, unknown>,
  callback: (prefs: FakePrefs, root: string) => Promise<void>,
  mismatchedPrefs = false,
): Promise<void> {
  const root = await Deno.makeTempDir();
  const prefs = new FakePrefs();
  const control = {
    schemaVersion: 1,
    purpose: "restart-bootstrap-isolation",
    runId: "qa3-restart-host-fixture",
    expiresAtMs: Date.now() + 60_000,
    filter: [isolationTarget],
    sourceManifestSha256: undefined as string | undefined,
    ...changes,
  };
  if (control.purpose === undefined) {
    control.sourceManifestSha256 = sourceFixtureContext.manifestSha256;
    prefs.setStringPref(
      "nora.tests.source_manifest_sha256",
      control.sourceManifestSha256,
    );
  }
  seedControlPrefs(
    prefs,
    mismatchedPrefs ? "foreign" : control.runId,
    control.filter,
  );
  try {
    await Deno.writeTextFile(
      `${root}/nora-tests-control.json`,
      JSON.stringify(control),
    );
    await withFakeTestControlEnvironment(
      prefs,
      root,
      () => callback(prefs, root),
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
}

Deno.test("actual loader preserves explicit one-shot restart isolation as NOT_EXECUTED QA", async () => {
  await withIsolationControl({}, async (prefs) => {
    let executions = 0;
    let retainCalls = 0;
    await runBrowserTests({
      chrome: {
        "#features-chrome/test/unused.test.ts": () => {
          executions++;
          return Promise.resolve({});
        },
      },
      esm: {},
      retainForCollection: () => {
        retainCalls++;
      },
    });
    const state = JSON.parse(prefs.getStringPref("nora.tests.state", ""));
    assertEquals(state.status, "done");
    assertEquals(state.runId, "qa3-restart-host-fixture");
    assertEquals(state.results, []);
    assertEquals(state.discoveredFiles, []);
    assertEquals(state.aborted, false);
    assertEquals(state.executionPurpose, "restart-bootstrap-isolation");
    assertEquals(state.qaVerdict, "NOT_EXECUTED");
    assertEquals(executions, 0);
    assertEquals(retainCalls, 1);
  });
});

Deno.test("actual loader still rejects an ordinary empty execution using the same sentinel", async () => {
  await withIsolationControl({ purpose: undefined }, async (prefs) => {
    await runBrowserTests({
      sourceBinding: sourceFixtureContext,
      chrome: {},
      esm: {},
      retainForCollection: () => {
        throw new Error("Unexpected success retention");
      },
    });
    const state = JSON.parse(prefs.getStringPref("nora.tests.state", ""));
    assertEquals(state.status, "error");
    assertStringIncludes(state.error, "selected no tests");
    assertEquals(state.qaVerdict, undefined);
  });
});

Deno.test("actual loader normalizes Vite filesystem keys before inventory checks and result publication", async () => {
  const target =
    "browser-features/chrome/common/workspaces/test/nativeLastTabWorkspacesEnabled.test.ts";
  for (
    const file of [
      "../../" + target,
      "/home/runner/work/Floorp/Floorp/" + target,
      "C:/ci/Floorp/" + target,
      "#features-chrome/" + target.slice("browser-features/chrome/".length),
      "/link-features-chrome/" +
      target.slice("browser-features/chrome/".length),
    ]
  ) {
    await withIsolationControl(
      { purpose: undefined, filter: [target] },
      async (prefs) => {
        let imports = 0;
        await runBrowserTests({
          sourceBinding: sourceFixtureContext,
          chrome: {
            [file]: () => {
              imports++;
              return Promise.resolve({
                __NORA_TEST_SOURCE_PROOF__: {
                  ...sourceFixtureContext,
                  id: target,
                  sourceSha256: "a".repeat(64),
                },
              });
            },
          },
          esm: {},
          retainForCollection: () => {},
        });
        const state = JSON.parse(prefs.getStringPref("nora.tests.state", ""));
        assertEquals(state.status, "done");
        assertEquals(state.discoveredFiles, [target]);
        assertEquals(state.results.length, 1);
        assertEquals(state.results[0].file, target);
        assertEquals(state.results[0].origin, "floorp");
        assertEquals(state.results[0].executionKind, "development-browser");
        assertEquals(state.results[0].ok, true);
        assertEquals(imports, 1);
      },
    );
  }
});

Deno.test("restart isolation rejects stale, broad, foreign, mismatched and oversized controls without fallback", async () => {
  for (
    const changes of [
      { expiresAtMs: Date.now() - 1 },
      { expiresAtMs: Date.now() + 600_000 },
      { filter: [] },
      {
        filter: [isolationTarget, "browser-features/chrome/test/other.test.ts"],
      },
      { filter: ["browser-features/chrome/test/other.test.ts"] },
      { runId: "foreign" },
      { purpose: "quality-pass" },
    ]
  ) {
    await withIsolationControl(changes, async (_prefs, root) => {
      await assertRejects(
        () =>
          runBrowserTests({
            chrome: {},
            esm: {},
            retainForCollection: () => {},
          }),
        Error,
        "refusing to apply a scoped run",
      );
      await assertRejects(
        () => Deno.stat(`${root}/nora-tests-control.json`),
        Deno.errors.NotFound,
      );
    });
  }
  await withIsolationControl({}, async () => {
    await assertRejects(
      () =>
        runBrowserTests({ chrome: {}, esm: {}, retainForCollection: () => {} }),
      Error,
      "prefs differ",
    );
  }, true);
});

Deno.test("ordinary source-bound loader rejects missing or foreign driver before eager pages", async () => {
  for (
    const driver of [undefined, { ...sourceFixtureContext, runId: "foreign" }, {
      ...sourceFixtureContext,
      manifestSha256: "c".repeat(64),
    }]
  ) {
    await withIsolationControl(
      { purpose: undefined, filter: [] },
      async (prefs) => {
        let pageImports = 0;
        await runBrowserTests({
          sourceBinding: driver,
          chrome: {},
          esm: {},
          pages: () => {
            pageImports++;
            return Promise.resolve({});
          },
          retainForCollection: () => {
            throw new Error("Unverified retention");
          },
        });
        const state = JSON.parse(prefs.getStringPref("nora.tests.state", ""));
        assertEquals(state.status, "error");
        assertStringIncludes(state.error, "source");
        assertEquals(pageImports, 0);
      },
    );
  }
});

Deno.test("source proof is required before explicit test tasks run", async () => {
  let calls = 0;
  const result = await runSingleTest(
    "browser-features/chrome/common/workspaces/test/nativeLastTabWorkspacesEnabled.test.ts",
    () =>
      Promise.resolve({
        runAllTests: () => {
          calls++;
        },
      }),
    sourceFixtureContext,
  );
  assertEquals(result.ok, false);
  assertStringIncludes(result.error ?? "", "source");
  assertEquals(calls, 0);
});

Deno.test("prefs without the original control file cannot grant restart isolation", async () => {
  await withIsolationControl({}, async (prefs, root) => {
    await Deno.remove(`${root}/nora-tests-control.json`);
    await runBrowserTests({
      chrome: {},
      esm: {},
      pages: () => Promise.resolve({}),
      retainForCollection: () => {
        throw new Error("Unexpected retention");
      },
    });
    const state = JSON.parse(prefs.getStringPref("nora.tests.state", ""));
    assertEquals(state.status, "error");
    assertEquals(state.executionPurpose, undefined);
  });
});

Deno.test("restart lease expiry during actual discovery cannot produce a done state", async () => {
  const saved = Date.now;
  let now = 1000;
  Date.now = () => now;
  try {
    await withIsolationControl({}, async (prefs) => {
      await runBrowserTests({
        get chrome() {
          now += 70_000;
          return {};
        },
        esm: {},
        retainForCollection: () => {
          throw new Error("Unexpected retention");
        },
      });
      const state = JSON.parse(prefs.getStringPref("nora.tests.state", ""));
      assertEquals(state.status, "error");
      assertStringIncludes(state.error, "lease differs");
    });
  } finally {
    Date.now = saved;
  }
});

function downloadedLoader(
  prefs: FakePrefs,
  expectedTaskCount: number,
  registerTasks: () => void,
): LazyModule {
  return () =>
    Promise.resolve({
      __NORA_DOWNLOADED_FIREFOX_TEST__: {
        schemaVersion: 1,
        upstreamPath: "browser/example/browser_locked.js",
        manifestPath: "browser/example/browser.toml",
        expectedTaskCount,
        prefs: [
          { name: "floorp.test.locked.bool", kind: "bool", value: true },
          { name: "floorp.test.locked.int", kind: "int", value: 42 },
          { name: "floorp.test.locked.string", kind: "string", value: "new" },
        ],
        headPolicy: "harness-replaced",
        supportPolicy: "locked-not-loaded",
        load: () => {
          registerTasks();
          return Promise.resolve({});
        },
      },
      prefs,
    });
}

Deno.test("unmarked import-only browser modules keep existing success semantics", async () => {
  const result = await runSingleTest(
    "browser-features/chrome/test/import-only.test.ts",
    () => Promise.resolve({}),
  );

  assert(result.ok);
  assertEquals(result.mode, "import");
  assertEquals(result.source, undefined);
  assertEquals(result.tasks, undefined);
});

Deno.test("downloaded marker applies prefs lazily, records tasks, and restores prefs", async () => {
  const prefs = new FakePrefs();
  prefs.values.set("floorp.test.locked.int", 7);
  prefs.values.set("floorp.test.locked.string", "old");

  await withFakePrefs(prefs, async () => {
    const loader = downloadedLoader(prefs, 1, () => {
      const addTask = (globalThis as Record<string, unknown>).add_task;
      assert(typeof addTask === "function");
      addTask(function lockedTask() {
        assertEquals(prefs.getBoolPref("floorp.test.locked.bool"), true);
        assertEquals(prefs.getIntPref("floorp.test.locked.int"), 42);
        assertEquals(prefs.getStringPref("floorp.test.locked.string"), "new");
      });
    });

    const result = await runSingleTest(
      "browser-features/chrome/test/firefox-downloaded/generated/locked.test.ts",
      loader,
    );

    assert(result.ok, result.error);
    assertEquals(result.mode, "mozillaTasks");
    assertEquals(result.source, "downloaded-firefox");
    assertEquals(result.upstreamPath, "browser/example/browser_locked.js");
    assertEquals(result.tasks?.length, 1);
    assertEquals(result.tasks?.[0]?.name, "lockedTask");
    assertEquals(result.tasks?.[0]?.ok, true);
    assertEquals(
      prefs.prefHasUserValue("floorp.test.locked.bool"),
      false,
      "a previously absent user pref must be cleared",
    );
    assertEquals(
      prefs.getIntPref("floorp.test.locked.int"),
      7,
      "a previous integer user value must be restored",
    );
    assertEquals(
      prefs.getStringPref("floorp.test.locked.string"),
      "old",
      "a previous typed user value must be restored",
    );
  });
});

Deno.test("downloaded marker fails closed on zero or mismatched tasks and restores prefs", async () => {
  const prefs = new FakePrefs();

  await withFakePrefs(prefs, async () => {
    const result = await runSingleTest(
      "browser-features/chrome/test/firefox-downloaded/generated/empty.test.ts",
      downloadedLoader(prefs, 1, () => {}),
    );

    assertEquals(result.ok, false);
    assertStringIncludes(result.error ?? "", "registered 0 Mozilla task(s)");
    assertEquals(result.tasks, undefined);
    assertEquals(prefs.values.size, 0);
  });

  await withFakePrefs(prefs, async () => {
    const result = await runSingleTest(
      "browser-features/chrome/test/firefox-downloaded/generated/mismatch.test.ts",
      downloadedLoader(prefs, 2, () => {
        const addTask = (globalThis as Record<string, unknown>).add_task;
        assert(typeof addTask === "function");
        addTask(function onlyTask() {});
      }),
    );

    assertEquals(result.ok, false);
    assertStringIncludes(result.error ?? "", "registered 1 Mozilla task(s)");
    assertEquals(prefs.values.size, 0);
  });
});

Deno.test("downloaded task failures retain per-task evidence", async () => {
  const prefs = new FakePrefs();

  await withFakePrefs(prefs, async () => {
    const result = await runSingleTest(
      "browser-features/chrome/test/firefox-downloaded/generated/failing.test.ts",
      downloadedLoader(prefs, 1, () => {
        const addTask = (globalThis as Record<string, unknown>).add_task;
        assert(typeof addTask === "function");
        addTask(function failingLockedTask() {
          throw new Error("locked task failed");
        });
      }),
    );

    assertEquals(result.ok, false);
    assertStringIncludes(result.error ?? "", "locked task failed");
    assertEquals(result.tasks?.length, 1);
    assertEquals(result.tasks?.[0]?.name, "failingLockedTask");
    assertEquals(result.tasks?.[0]?.ok, false);
    assertStringIncludes(result.tasks?.[0]?.error ?? "", "locked task failed");
    assertEquals(prefs.values.size, 0);
  });
});
