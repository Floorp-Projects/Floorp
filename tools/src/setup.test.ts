// SPDX-License-Identifier: MPL-2.0

/**
 * Host tests for `setup.ts` and the relative `noraneko-devdir` mount links.
 * All filesystem work happens in temporary directories; no runtime registry,
 * network, browser or `deno install` is required (deps install is DI-faked).
 */

import {
  assertEquals,
  assertFalse,
  assertRejects,
  assertStringIncludes,
  assertThrows,
} from "@std/assert";
import * as path from "@std/path";
import { PROJECT_ROOT } from "./defines.ts";
import { run as runInjector } from "./injector.ts";
import { exists } from "./utils.ts";
import {
  assertRuntimeMarker,
  buildSetupRecord,
  copyRuntimeTree,
  DEBUG_RUNTIME_MARKER_FILE,
  expectedRuntimeTarget,
  fflatePresent,
  mergeSetupRecord,
  prepareDeps,
  prepareProfile,
  prepareRuntime,
  prepareSetup,
  prepareVite,
  readRuntimeMarker,
  recordSetup,
  type RuntimeMarker,
  type SetupRecord,
} from "./setup.ts";

function withTempDir(
  fn: (dir: string) => void | Promise<void>,
): Promise<void> {
  const dir = Deno.makeTempDirSync({ prefix: "fdh-setup-" });
  return (async () => {
    try {
      await fn(dir);
    } finally {
      Deno.removeSync(dir, { recursive: true });
    }
  })();
}

function writeFflate(nodeModulesDir: string): void {
  const packageDir = path.join(nodeModulesDir, "fflate");
  Deno.mkdirSync(packageDir, { recursive: true });
  Deno.writeTextFileSync(path.join(packageDir, "package.json"), "{}");
}

function runtimeMarker(overrides: Partial<RuntimeMarker> = {}): RuntimeMarker {
  const target = expectedRuntimeTarget();
  return {
    schemaVersion: 1,
    distribution: "debug",
    platform: target.platform,
    architecture: target.architecture,
    artifactName: "mac-universal-artifacts.zip",
    artifactSize: 246065037,
    artifactSha256:
      "10d7564ba8aacfe5b6745fdfea06248f0ff2a9e0af62497b93d3d22f85c60645",
    version: "158.0.1",
    buildId: "20261019220659",
    ...overrides,
  };
}

function writeRuntimeTree(
  binRootDir: string,
  marker: RuntimeMarker = runtimeMarker(),
): void {
  Deno.mkdirSync(binRootDir, { recursive: true });
  Deno.writeTextFileSync(
    path.join(binRootDir, DEBUG_RUNTIME_MARKER_FILE),
    JSON.stringify(marker),
  );
  Deno.writeTextFileSync(path.join(binRootDir, "runtime.txt"), "runtime");
}

Deno.test("prepareDeps detects fflate and does not install twice", () =>
  withTempDir((root) => {
    const nodeModulesDir = path.join(root, "node_modules");
    writeFflate(nodeModulesDir);
    let installs = 0;
    const runInstall = () => {
      installs++;
      return 0;
    };
    assertEquals(fflatePresent(nodeModulesDir), true);

    const first = prepareDeps({ projectRoot: root, runInstall });
    assertEquals(first.status, "already");
    assertEquals(first.installed, false);
    const second = prepareDeps({ projectRoot: root, runInstall });
    assertEquals(second.status, "already");
    assertEquals(installs, 0);
  }));

Deno.test("prepareDeps installs once when fflate is missing", () =>
  withTempDir((root) => {
    let installs = 0;
    const runInstall = () => {
      installs++;
      writeFflate(path.join(root, "node_modules"));
      return 0;
    };
    const first = prepareDeps({ projectRoot: root, runInstall });
    assertEquals(first.status, "prepared");
    assertEquals(first.installed, true);
    assertEquals(installs, 1);

    const second = prepareDeps({ projectRoot: root, runInstall });
    assertEquals(second.status, "already");
    assertEquals(second.installed, false);
    assertEquals(installs, 1);
  }));

Deno.test("prepareDeps is a no-op in CI", () =>
  withTempDir((root) => {
    let installs = 0;
    const runInstall = () => {
      installs++;
      return 0;
    };
    const result = prepareDeps({ projectRoot: root, isCI: true, runInstall });
    assertEquals(result.status, "ci-skip");
    assertEquals(result.isCI, true);
    assertEquals(installs, 0);
  }));

Deno.test("prepareRuntime copies a marker tree and relinks idempotently", () =>
  withTempDir(async (root) => {
    const source = path.join(root, "registry", "bin");
    writeRuntimeTree(source);
    const binRootDir = path.join(root, "target", "_dist", "bin");
    let relinks = 0;
    const relink = () => {
      relinks++;
    };

    const first = await prepareRuntime({
      sourcePath: source,
      binRootDir,
      projectRoot: root,
      relink,
    });
    assertEquals(first.status, "prepared");
    assertEquals(first.copyMode, "safe-copy");
    assertEquals(first.relinked, true);
    assertEquals(relinks, 1);
    assertEquals(readRuntimeMarker(binRootDir)?.buildId, "20261019220659");
    assertEquals(exists(path.join(binRootDir, "runtime.txt")), true);

    const second = await prepareRuntime({
      sourcePath: source,
      binRootDir,
      projectRoot: root,
      relink,
    });
    assertEquals(second.status, "already");
    assertEquals(second.relinked, true);
    assertEquals(relinks, 2);
  }));

Deno.test("prepareRuntime falls back to symlink-preserving copy", () =>
  withTempDir(async (root) => {
    const source = path.join(root, "registry", "bin");
    writeRuntimeTree(source);
    Deno.writeTextFileSync(path.join(source, "target.txt"), "target");
    Deno.symlinkSync("target.txt", path.join(source, "link.txt"));
    const binRootDir = path.join(root, "target", "_dist", "bin");

    const mode = await copyRuntimeTree(source, binRootDir);
    assertEquals(mode, "symlink-fallback");
    assertEquals(
      await Deno.readLink(path.join(binRootDir, "link.txt")),
      "target.txt",
    );
    assertEquals(
      Deno.readTextFileSync(path.join(binRootDir, "target.txt")),
      "target",
    );
  }));

Deno.test("prepareRuntime reports a missing registry source", () =>
  withTempDir(async (root) => {
    const binRootDir = path.join(root, "target", "_dist", "bin");
    const result = await prepareRuntime({ binRootDir, projectRoot: root });
    assertEquals(result.status, "missing");
    assertEquals(result.relinked, false);
  }));

Deno.test("prepareRuntime rejects a marker for another platform", () =>
  withTempDir(async (root) => {
    const source = path.join(root, "registry", "bin");
    const target = expectedRuntimeTarget();
    const wrongPlatform: RuntimeMarker["platform"] = target.platform === "macos"
      ? "linux"
      : "macos";
    writeRuntimeTree(source, runtimeMarker({ platform: wrongPlatform }));
    const binRootDir = path.join(root, "target", "_dist", "bin");
    await assertRejects(
      () =>
        prepareRuntime({ sourcePath: source, binRootDir, projectRoot: root }),
      Error,
      "does not match",
    );
  }));

Deno.test("assertRuntimeMarker validates platform and architecture", () => {
  const target = expectedRuntimeTarget();
  assertEquals(
    assertRuntimeMarker(runtimeMarker(), target).platform,
    target.platform,
  );
  assertThrows(
    () =>
      assertRuntimeMarker(
        runtimeMarker({
          architecture: target.architecture === "universal"
            ? "x86_64"
            : "universal",
        }),
        target,
      ),
    Error,
    "architecture",
  );
});

Deno.test("prepareProfile wipes sessionstore but keeps user.js", () =>
  withTempDir((root) => {
    const profileDir = path.join(root, "profile", "test");
    const backups = path.join(profileDir, "sessionstore-backups");
    Deno.mkdirSync(backups, { recursive: true });
    Deno.writeTextFileSync(path.join(backups, "recovery.jsonlz4"), "x");
    Deno.writeTextFileSync(path.join(profileDir, "sessionstore.jsonlz4"), "x");
    Deno.writeTextFileSync(path.join(profileDir, "user.js"), "pref");

    const first = prepareProfile({ profileDir });
    assertEquals(first.status, "removed");
    assertEquals(first.removed.length, 2);
    assertEquals(exists(backups), false);
    assertEquals(exists(path.join(profileDir, "sessionstore.jsonlz4")), false);
    assertEquals(exists(path.join(profileDir, "user.js")), true);

    const second = prepareProfile({ profileDir });
    assertEquals(second.status, "already");
    assertEquals(second.removed.length, 0);
  }));

Deno.test("prepareVite clears the cache idempotently", () =>
  withTempDir((root) => {
    const cacheDir = path.join(root, "node_modules", ".vite");
    Deno.mkdirSync(path.join(cacheDir, "pages-settings"), { recursive: true });

    const first = prepareVite({ projectRoot: root, devRunning: false });
    assertEquals(first.status, "removed");
    assertEquals(first.removed, true);
    assertEquals(exists(cacheDir), false);

    const second = prepareVite({ projectRoot: root, devRunning: false });
    assertEquals(second.status, "already");
    assertEquals(second.removed, false);
  }));

Deno.test("prepareVite refuses while a dev server owns the cache", () =>
  withTempDir((root) => {
    const cacheDir = path.join(root, "node_modules", ".vite");
    Deno.mkdirSync(cacheDir, { recursive: true });
    assertThrows(
      () => prepareVite({ projectRoot: root, devRunning: true }),
      Error,
      "stopped",
    );
    assertEquals(exists(cacheDir), true);
  }));

Deno.test("noraneko-devdir mount links are relative and stable", () =>
  withTempDir(async (root) => {
    const binRootDir = path.join(root, "_dist", "bin");
    Deno.mkdirSync(binRootDir, { recursive: true });
    const options = { binRootDir, projectRoot: PROJECT_ROOT };
    runInjector("dev", "noraneko-devdir", options);

    const link = path.join(binRootDir, "noraneko-devdir", "content");
    const stored = await Deno.readLink(link);
    assertFalse(path.isAbsolute(stored), "content link must be relative");
    assertEquals(
      path.resolve(path.dirname(link), stored),
      path.join(PROJECT_ROOT, "bridge", "loader-features", "_dist"),
    );

    for (const mount of ["startup", "skin", "resource", "pages-settings"]) {
      const mountLink = path.join(binRootDir, "noraneko-devdir", mount);
      assertFalse(
        path.isAbsolute(await Deno.readLink(mountLink)),
        `${mount} link must be relative`,
      );
    }

    // Re-running keeps the links relative (and the manifest entry once).
    runInjector("dev", "noraneko-devdir", options);
    assertFalse(path.isAbsolute(await Deno.readLink(link)));
    const manifest = Deno.readTextFileSync(
      path.join(binRootDir, "chrome.manifest"),
    );
    assertStringIncludes(
      manifest,
      "manifest noraneko-devdir/noraneko.manifest",
    );
    assertEquals(
      manifest.split("manifest noraneko-devdir/noraneko.manifest").length,
      2,
    );
  }));

Deno.test("setup record merge keeps the first preparation time and source", () => {
  const base = {
    deps: {
      status: "prepared" as const,
      nodeModulesDir: "/n",
      installed: true,
      isCI: false,
    },
    runtime: {
      status: "prepared" as const,
      binRootDir: "/b",
      sourcePath: "/registry/bin",
      marker: runtimeMarker(),
      copyMode: "safe-copy" as const,
      relinked: true,
    },
    profile: {
      status: "removed" as const,
      profileDir: "/p",
      removed: ["/p/sessionstore.jsonlz4"],
    },
    vite: { status: "removed" as const, cacheDir: "/v", removed: true },
  };
  const first = buildSetupRecord(base, () => "2026-01-01T00:00:00.000Z");
  assertEquals(first.schemaVersion, 1);
  assertEquals(first.guardEnv.FLOORP_SKIP_PATCHER, "1");

  const second = buildSetupRecord(
    {
      deps: { ...base.deps, status: "already", installed: false },
      runtime: { status: "already", binRootDir: "/b", relinked: false },
      profile: { status: "already", profileDir: "/p", removed: [] },
      vite: { status: "already", cacheDir: "/v", removed: false },
    },
    () => "2026-02-02T00:00:00.000Z",
  );
  const merged = mergeSetupRecord(first, second);
  assertEquals(merged.preparedAt, "2026-01-01T00:00:00.000Z");
  assertEquals(merged.runtime.sourcePath, "/registry/bin");
  assertEquals(merged.runtime.marker?.buildId, "20261019220659");

  const state: { setup?: SetupRecord } = { setup: first };
  recordSetup(state, second);
  assertEquals(state.setup?.preparedAt, "2026-01-01T00:00:00.000Z");
  assertEquals(state.setup?.runtime.sourcePath, "/registry/bin");
});

Deno.test("prepareSetup composes all steps and records the guard env", () =>
  withTempDir(async (root) => {
    writeFflate(path.join(root, "node_modules"));
    const source = path.join(root, "registry", "bin");
    writeRuntimeTree(source);
    const profileDir = path.join(root, "profile", "test");
    Deno.mkdirSync(path.join(profileDir, "sessionstore-backups"), {
      recursive: true,
    });
    Deno.writeTextFileSync(
      path.join(profileDir, "sessionstore-backups", "recovery.jsonlz4"),
      "x",
    );
    Deno.mkdirSync(path.join(root, "node_modules", ".vite", "pages-settings"), {
      recursive: true,
    });
    let relinks = 0;

    const record = await prepareSetup({
      projectRoot: root,
      deps: {
        runInstall: () => {
          throw new Error("install must not run when fflate is present");
        },
      },
      runtime: {
        sourcePath: source,
        relink: () => {
          relinks++;
        },
      },
      profile: { profileDir },
      vite: { devRunning: false },
    });
    assertEquals(record.deps.status, "already");
    assertEquals(record.runtime.status, "prepared");
    assertEquals(record.profile.status, "removed");
    assertEquals(record.vite.status, "removed");
    assertEquals(record.guardEnv.FLOORP_SKIP_PATCHER, "1");
    assertEquals(relinks, 1);
  }));

Deno.test("relative mount links resolve from the link parent in the same root", () => {
  // The temp bin root must sit under the project root, otherwise the extra
  // `..` from a path.relative(linkPath, target) bug clamps at `/` and passes.
  const root = Deno.makeTempDirSync({
    dir: PROJECT_ROOT,
    prefix: "fdh-link-",
  });
  try {
    const binRootDir = path.join(root, "_dist", "bin");
    Deno.mkdirSync(binRootDir, { recursive: true });
    runInjector("dev", "noraneko-devdir", {
      binRootDir,
      projectRoot: PROJECT_ROOT,
    });

    const link = path.join(binRootDir, "noraneko-devdir", "content");
    assertEquals(
      Deno.realPathSync(link),
      path.join(PROJECT_ROOT, "bridge", "loader-features", "_dist"),
    );
  } finally {
    Deno.removeSync(root, { recursive: true });
  }
});
