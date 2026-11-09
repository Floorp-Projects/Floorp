// SPDX-License-Identifier: MPL-2.0

/**
 * Host tests for `setup.ts` and the relative `noraneko-devdir` mount links.
 * All filesystem work happens in temporary directories; no runtime registry,
 * network, browser or `deno install` is required (deps install is DI-faked).
 */

import {
  assert,
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
  recreateLink,
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
    // Windows cannot infer the type of a relative target from the cwd.
    Deno.symlinkSync("target.txt", path.join(source, "link.txt"), {
      type: "file",
    });
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

Deno.test("runtime copy keeps directory and dangling links as dir links", () =>
  withTempDir(async (root) => {
    const source = path.join(root, "registry", "bin");
    writeRuntimeTree(source);
    Deno.mkdirSync(path.join(source, "realdir"));
    Deno.writeTextFileSync(path.join(source, "realdir", "f.txt"), "f");
    Deno.symlinkSync("realdir", path.join(source, "dirlink"), { type: "dir" });
    Deno.symlinkSync("missing-dir", path.join(source, "dangling"), {
      type: "dir",
    });
    const binRootDir = path.join(root, "target", "_dist", "bin");

    assertEquals(await copyRuntimeTree(source, binRootDir), "symlink-fallback");
    // The copied directory link resolves inside the new tree.
    assertEquals(
      Deno.readTextFileSync(path.join(binRootDir, "dirlink", "f.txt")),
      "f",
    );
    // A dangling link is copied as a link, not dropped or followed.
    assert(Deno.lstatSync(path.join(binRootDir, "dangling")).isSymlink);
  }));

Deno.test("recreateLink falls back to a junction for dir links on Windows only", () => {
  const calls: Array<{ target: string; type: string | undefined }> = [];
  const failSymlink: typeof Deno.symlinkSync = (target, _link, options) => {
    calls.push({ target: String(target), type: options?.type });
    if (options?.type !== "junction") {
      throw new Error("A required privilege is not held (os error 1314)");
    }
  };
  const destination = path.join("C:\\", "rt", "bin", "mount");

  assertEquals(
    recreateLink("..\\src", destination, "dir", failSymlink, "windows"),
    "junction",
  );
  // The junction gets an absolute target resolved from the new link's parent.
  assertEquals(calls[1].type, "junction");
  assert(path.isAbsolute(calls[1].target));

  // An extended-length `\\?\` target is used without the prefix.
  calls.length = 0;
  recreateLink(
    "\\\\?\\D:\\real\\dir",
    destination,
    "dir",
    failSymlink,
    "windows",
  );
  assertFalse(calls[1].target.includes("?"), calls[1].target);
  assertStringIncludes(calls[1].target, "real");

  // File links and non-Windows hosts keep the original error.
  assertThrows(
    () => recreateLink("a.txt", destination, "file", failSymlink, "windows"),
    Error,
    "1314",
  );
  assertThrows(
    () => recreateLink("../src", destination, "dir", failSymlink, "linux"),
    Error,
    "1314",
  );
});

Deno.test("prepareSetup uses a precomputed deps result without re-running deps", () =>
  withTempDir(async (root) => {
    let installs = 0;
    const depsResult = {
      status: "prepared" as const,
      nodeModulesDir: path.join(root, "node_modules"),
      installed: true,
      isCI: false,
    };
    const record = await prepareSetup({
      projectRoot: root,
      depsResult,
      deps: {
        runInstall: () => {
          installs += 1;
          return 0;
        },
      },
      vite: { devRunning: false },
      now: () => "2026-11-09T00:00:00.000Z",
    });
    assertEquals(installs, 0);
    assertEquals(record.deps, depsResult);
  }));

Deno.test("setup_deps.ts never reaches an npm package (W-05)", async () => {
  // fdh.ts imports setup_deps.ts statically; an npm import anywhere in its
  // local graph would break every fdh command before `deno install`.
  const config = JSON.parse(
    Deno.readTextFileSync(path.join(PROJECT_ROOT, "deno.json")),
  ) as { imports?: Record<string, string> };
  const imports = config.imports ?? {};
  const seen = new Set<string>();
  const visit = async (file: string): Promise<void> => {
    if (seen.has(file)) return;
    seen.add(file);
    const text = await Deno.readTextFile(file);
    // `import|export ... from "x"` (may span lines) or a bare `import "x"`.
    const importPattern =
      /^\s*(?:import|export)\b[^;]*?\bfrom\s*["']([^"']+)["']|^\s*import\s*["']([^"']+)["']/gm;
    for (const match of text.matchAll(importPattern)) {
      const specifier = match[1] ?? match[2];
      if (specifier.startsWith(".")) {
        await visit(path.resolve(path.dirname(file), specifier));
        continue;
      }
      assertFalse(
        specifier.startsWith("npm:"),
        `${file} imports ${specifier}`,
      );
      const mapped = Object.entries(imports).find(([key]) =>
        specifier === key || (key.endsWith("/") && specifier.startsWith(key))
      );
      assertFalse(
        mapped?.[1].startsWith("npm:") ?? false,
        `${file} imports ${specifier} -> ${mapped?.[1]}`,
      );
    }
  };
  await visit(path.join(PROJECT_ROOT, "tools", "src", "setup_deps.ts"));
  assert(seen.size >= 3, "setup_deps.ts graph was not traversed");
});

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

/**
 * The injector stores relative mount links on POSIX (a copied `_dist/bin`
 * resolves inside its own worktree) and absolute targets on Windows (its
 * junction fallback can only store an absolute path; see injector.ts).
 */
const RELATIVE_MOUNT_LINKS = Deno.build.os !== "windows";

/** Strip the Win32 extended-length prefix a junction target may carry. */
function stripExtendedPrefix(target: string): string {
  return target.startsWith("\\\\?\\") ? target.slice(4) : target;
}

async function assertMountLinkForm(link: string, label: string) {
  const stored = stripExtendedPrefix(await Deno.readLink(link));
  if (RELATIVE_MOUNT_LINKS) {
    assertFalse(path.isAbsolute(stored), `${label} link must be relative`);
  } else {
    assert(path.isAbsolute(stored), `${label} link must be absolute`);
  }
  return stored;
}

Deno.test("noraneko-devdir mount links use the per-OS form and are stable", () =>
  withTempDir(async (root) => {
    const binRootDir = path.join(root, "_dist", "bin");
    Deno.mkdirSync(binRootDir, { recursive: true });
    const options = { binRootDir, projectRoot: PROJECT_ROOT };
    runInjector("dev", "noraneko-devdir", options);

    const link = path.join(binRootDir, "noraneko-devdir", "content");
    const stored = await assertMountLinkForm(link, "content");
    // Windows paths are case-insensitive; POSIX comparisons stay exact.
    const comparable = (value: string) =>
      Deno.build.os === "windows" ? value.toLowerCase() : value;
    assertEquals(
      comparable(path.resolve(path.dirname(link), stored)),
      comparable(path.join(PROJECT_ROOT, "bridge", "loader-features", "_dist")),
    );

    for (const mount of ["startup", "skin", "resource", "pages-settings"]) {
      await assertMountLinkForm(
        path.join(binRootDir, "noraneko-devdir", mount),
        mount,
      );
    }

    // Re-running keeps the same link form (and the manifest entry once).
    runInjector("dev", "noraneko-devdir", options);
    await assertMountLinkForm(link, "content (re-run)");
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

Deno.test("mount links resolve to the target inside the same root", () => {
  // The temp bin root must sit under the project root, otherwise the extra
  // `..` from a path.relative(linkPath, target) bug clamps at `/` and passes.
  const root = Deno.makeTempDirSync({
    dir: PROJECT_ROOT,
    prefix: "fdh-link-",
  });
  try {
    const binRootDir = path.join(root, "_dist", "bin");
    const target = path.join(root, "bridge", "loader-features", "_dist");
    Deno.mkdirSync(binRootDir, { recursive: true });
    Deno.mkdirSync(target, { recursive: true });
    runInjector("dev", "noraneko-devdir", {
      binRootDir,
      projectRoot: root,
    });

    const link = path.join(binRootDir, "noraneko-devdir", "content");
    assertEquals(Deno.realPathSync(link), target);
  } finally {
    Deno.removeSync(root, { recursive: true });
  }
});
