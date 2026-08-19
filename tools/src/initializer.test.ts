// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert";
import * as path from "@std/path";
import {
  debugRuntimeArchiveName,
  filterRuntimeEntries,
  type InitializerRunDependencies,
  installLockedRuntime,
  type LockedReleaseMetadata,
  lockedReleasePublicDownloadUrl,
  type LockedRuntimeOperations,
  pickRuntimeEntry,
  resolveNativeRuntimeTarget,
  run,
  runtimeLayoutFor,
  scoreRuntimeEntry,
  selectDebugRuntimeEntry,
  validateLockedRuntimeArtifact,
  validateLockedRuntimeReleaseMetadata,
} from "./initializer.ts";
import type { RuntimeArtifact, RuntimeLock } from "./runtime_lock.ts";

// --- filterRuntimeEntries ---

Deno.test(
  "filterRuntimeEntries returns empty array for null/undefined data",
  () => {
    assertEquals(filterRuntimeEntries({}), []);
    assertEquals(filterRuntimeEntries({ data: undefined }), []);
    assertEquals(
      filterRuntimeEntries({ data: null as unknown as undefined }),
      [],
    );
  },
);

Deno.test("filterRuntimeEntries filters out non-file entries", () => {
  const index = {
    data: [
      { type: "file", name: "artifact.zip" },
      { type: "directory", name: "somedir" },
      { type: "file", name: "" },
      { type: "file", name: "another.tar.xz" },
    ],
  };
  const result = filterRuntimeEntries(index);
  assertEquals(result.length, 2);
  assertEquals(result[0].name, "artifact.zip");
  assertEquals(result[1].name, "another.tar.xz");
});

Deno.test("filterRuntimeEntries handles entries with missing name", () => {
  const index = {
    data: [
      { type: "file" } as unknown as { type: string; name: string },
      { type: "file", name: "valid.zip" },
    ],
  };
  const result = filterRuntimeEntries(index);
  assertEquals(result.length, 1);
  assertEquals(result[0].name, "valid.zip");
});

// --- scoreRuntimeEntry ---

Deno.test("scoreRuntimeEntry gives platform points for windows keyword", () => {
  const score = scoreRuntimeEntry("floorp-windows-x86_64-artifact.zip", {
    platform: "windows",
    architecture: "x86_64",
    filename: "test.zip",
    format: "zip",
  });
  // "windows" matches both "windows" and "win" keywords (50+50=100),
  // "x86_64" matches "x86_64" keyword (40), artifact bonus (5), .zip bonus (5)
  assertEquals(score, 150);
});

Deno.test("scoreRuntimeEntry gives platform points for darwin keyword", () => {
  const score = scoreRuntimeEntry("floorp-macos-universal.dmg", {
    platform: "darwin",
    architecture: "universal",
    filename: "test.dmg",
    format: "dmg",
  });
  // "macos" matches both "macos" and "mac" keywords (50+50=100),
  // "universal" matches "universal" keyword (40) = 140
  assertEquals(score, 140);
});

Deno.test("scoreRuntimeEntry gives zero for unmatched entry", () => {
  const score = scoreRuntimeEntry("readme.txt", {
    platform: "linux",
    architecture: "x86_64",
    filename: "test.tar.xz",
    format: "tar.xz",
  });
  assertEquals(score, 0);
});

Deno.test("scoreRuntimeEntry adds bonus for .zip extension", () => {
  const withZip = scoreRuntimeEntry("windows-x86_64.zip", {
    platform: "windows",
    architecture: "x86_64",
    filename: "test.zip",
    format: "zip",
  });
  const withoutZip = scoreRuntimeEntry("windows-x86_64.tar", {
    platform: "windows",
    architecture: "x86_64",
    filename: "test.zip",
    format: "zip",
  });
  assert(withZip > withoutZip, "zip extension should add bonus points");
});

Deno.test("scoreRuntimeEntry adds bonus for artifact keyword", () => {
  const withArtifact = scoreRuntimeEntry("linux-artifact.tar", {
    platform: "linux",
    architecture: "x86_64",
    filename: "test.tar.xz",
    format: "tar.xz",
  });
  const withoutArtifact = scoreRuntimeEntry("linux-build.tar", {
    platform: "linux",
    architecture: "x86_64",
    filename: "test.tar.xz",
    format: "tar.xz",
  });
  assert(withArtifact > withoutArtifact, "artifact keyword should add bonus");
});

// --- pickRuntimeEntry ---

Deno.test("pickRuntimeEntry picks the highest-scored entry", () => {
  const entries = [
    { type: "file", name: "readme.txt" },
    { type: "file", name: "floorp-windows-x86_64-artifact.zip" },
    { type: "file", name: "floorp-linux-x86_64.tar.xz" },
  ];
  const picked = pickRuntimeEntry(entries, {
    platform: "windows",
    architecture: "x86_64",
    filename: "test.zip",
    format: "zip",
  });
  assertEquals(picked.name, "floorp-windows-x86_64-artifact.zip");
});

Deno.test("pickRuntimeEntry throws when entries array is empty", () => {
  assertThrows(
    () =>
      pickRuntimeEntry([], {
        platform: "windows",
        architecture: "x86_64",
        filename: "test.zip",
        format: "zip",
      }),
    Error,
    "No runtime artifacts available",
  );
});

Deno.test("pickRuntimeEntry returns first entry when no strong match", () => {
  const entries = [
    { type: "file", name: "unknown-file.dat" },
    { type: "file", name: "other-file.dat" },
  ];
  const picked = pickRuntimeEntry(entries, {
    platform: "linux",
    architecture: "aarch64",
    filename: "test.tar.xz",
    format: "tar.xz",
  });
  assertEquals(picked.name, "unknown-file.dat");
});

Deno.test("Debug Runtime selection requires the exact FTP platform bundle", () => {
  const entries = [
    {
      type: "file",
      name: "linux-x86_64-artifacts.zip",
      size: 10,
      hash: "a".repeat(64),
    },
    {
      type: "file",
      name: "windows-x86_64-artifacts.zip",
      size: 11,
      hash: "b".repeat(64),
    },
  ];
  const archive = {
    filename: "floorp-linux-x86_64-moz-artifact.tar.xz",
    format: "tar.xz" as const,
    platform: "linux" as const,
    architecture: "x86_64" as const,
  };
  assertEquals(debugRuntimeArchiveName(archive), "linux-x86_64-artifacts.zip");
  assertEquals(selectDebugRuntimeEntry(entries, archive), entries[0]);
  assertThrows(
    () =>
      selectDebugRuntimeEntry(
        entries.filter((entry) => entry.name !== "linux-x86_64-artifacts.zip"),
        archive,
      ),
    Error,
    "Expected exactly one Debug Runtime FTP asset",
  );
});

const MAIN_SHA256 = "1".repeat(64);
const COMPANION_SHA256 = "2".repeat(64);

function lockedArtifact(): RuntimeArtifact {
  return {
    platform: "windows",
    architecture: "x86_64",
    format: "zip",
    extractionPolicy: "zip-direct-floorp",
    asset: {
      id: 101,
      name: "floorp-windows-x86_64-moz-artifact.zip",
      size: 4,
      sha256: MAIN_SHA256,
    },
    applicationIniAsset: {
      id: 102,
      name: "windows-x86_64-application-ini.zip",
      size: 3,
      sha256: COMPANION_SHA256,
    },
    version: "152.0.7",
    buildId: "20260718112318",
  };
}

function lockedMacArtifact(): RuntimeArtifact {
  return {
    platform: "macos",
    architecture: "universal",
    format: "dmg",
    extractionPolicy: "dmg-floorp-app",
    asset: {
      id: 201,
      name: "floorp-macos-universal.dmg",
      size: 4,
      sha256: MAIN_SHA256,
    },
    applicationIniAsset: {
      id: 202,
      name: "macos-universal-application-ini.zip",
      size: 3,
      sha256: COMPANION_SHA256,
    },
    version: "152.0.7",
    buildId: "20260718112318",
  };
}

function lockedRuntime(artifact = lockedArtifact()): RuntimeLock {
  return {
    schemaVersion: 1,
    source: {
      repository: "Floorp-Projects/Floorp-Runtime",
      trackingRef: "nora-test",
      ref: "daily-test",
      commit: "a".repeat(40),
      tree: "b".repeat(40),
      release: { id: 100, immutable: false },
      materials: { count: 0, totalBytes: 0, entries: [] },
      tests: {
        count: 0,
        totalTasks: 0,
        supportDependencyEdges: 0,
        entries: [],
        manifests: [],
      },
    },
    artifacts: [artifact],
  };
}

Deno.test("locked Runtime public download URLs encode locked path segments", () => {
  assertEquals(
    lockedReleasePublicDownloadUrl(
      "Floorp-Projects/Floorp-Runtime",
      "daily/test #1",
      "floorp build+#.zip",
    ),
    "https://github.com/Floorp-Projects/Floorp-Runtime/releases/download/daily%2Ftest%20%231/floorp%20build%2B%23.zip",
  );
});

Deno.test("locked Runtime public download URLs reject unsafe fields", () => {
  assertThrows(
    () => lockedReleasePublicDownloadUrl("invalid", "daily-test", "a.zip"),
    Error,
    "Invalid locked Runtime repository",
  );
  assertThrows(
    () =>
      lockedReleasePublicDownloadUrl(
        "Floorp-Projects/Floorp-Runtime",
        "..",
        "a.zip",
      ),
    Error,
    "release ref",
  );
  for (const assetName of ["", "..", "../a.zip", "dir\\a.zip"]) {
    assertThrows(
      () =>
        lockedReleasePublicDownloadUrl(
          "Floorp-Projects/Floorp-Runtime",
          "daily-test",
          assetName,
        ),
      Error,
      "plain file names",
    );
  }
});

function releaseMetadata(artifact: RuntimeArtifact): LockedReleaseMetadata {
  return {
    id: 100,
    tagName: "daily-test",
    immutable: false,
    assets: [
      {
        id: artifact.asset.id,
        name: artifact.asset.name,
        size: artifact.asset.size,
        digest: `sha256:${artifact.asset.sha256}`,
        browserDownloadUrl: "https://malicious.invalid/main",
      },
      {
        id: artifact.applicationIniAsset.id,
        name: artifact.applicationIniAsset.name,
        size: artifact.applicationIniAsset.size,
        digest: `sha256:${artifact.applicationIniAsset.sha256}`,
        browserDownloadUrl: "https://malicious.invalid/companion",
      },
    ],
  };
}

function releaseMetadataPayload(artifact: RuntimeArtifact): unknown {
  return {
    id: 100,
    tag_name: "daily-test",
    immutable: false,
    assets: [
      {
        id: artifact.asset.id,
        name: artifact.asset.name,
        size: artifact.asset.size,
        digest: `sha256:${artifact.asset.sha256}`,
        browser_download_url: "https://malicious.invalid/main",
      },
      {
        id: artifact.applicationIniAsset.id,
        name: artifact.applicationIniAsset.name,
        size: artifact.applicationIniAsset.size,
        digest: `sha256:${artifact.applicationIniAsset.sha256}`,
        browser_download_url: "https://malicious.invalid/companion",
      },
    ],
  };
}

interface FakeOperationOptions {
  nonce: string;
  digestMismatch?: boolean;
  buildIdMismatch?: boolean;
  extractionFailure?: boolean;
  missingExecutable?: boolean;
  downloadUrls?: string[];
  rename?: LockedRuntimeOperations["rename"];
  remove?: LockedRuntimeOperations["remove"];
  malformedMacInfoPlist?: boolean;
}

function fakeLockedOperations(
  artifact: RuntimeArtifact,
  options: FakeOperationOptions,
): LockedRuntimeOperations {
  return {
    fetchRelease: () => Promise.resolve(releaseMetadata(artifact)),
    download: async (url, destination) => {
      options.downloadUrls?.push(url);
      const size = url.endsWith(`/${encodeURIComponent(artifact.asset.name)}`)
        ? artifact.asset.size
        : artifact.applicationIniAsset.size;
      await Deno.writeFile(destination, new Uint8Array(size));
    },
    sha256: (filePath) => {
      if (filePath.endsWith(artifact.asset.name)) {
        return Promise.resolve(
          options.digestMismatch ? "0".repeat(64) : artifact.asset.sha256,
        );
      }
      return Promise.resolve(artifact.applicationIniAsset.sha256);
    },
    extractMain: async (_artifact, _archive, destination) => {
      if (options.extractionFailure) {
        throw new Error("injected extraction failure");
      }
      const layout = runtimeLayoutFor(destination, artifact);
      await Deno.mkdir(path.dirname(layout.applicationIni), {
        recursive: true,
      });
      await Deno.writeTextFile(
        layout.applicationIni,
        [
          "[App]",
          "Name=Floorp",
          `Version=${artifact.version}`,
          `BuildID=${
            options.buildIdMismatch ? "19990101000000" : artifact.buildId
          }`,
          "",
        ].join("\n"),
      );
      await Deno.mkdir(path.dirname(layout.executable), {
        recursive: true,
      });
      if (!options.missingExecutable) {
        await Deno.writeFile(layout.executable, new Uint8Array([1]));
        if (Deno.build.os !== "windows" && artifact.platform !== "windows") {
          await Deno.chmod(layout.executable, 0o755);
        }
      }
      if (artifact.platform === "macos") {
        const contentsRoot = path.dirname(path.dirname(layout.applicationIni));
        const stagingDeveloperPath = path.join(destination, "floorp");
        const infoPlist = options.malformedMacInfoPlist
          ? [
            '<?xml version="1.0" encoding="UTF-8"?>',
            "<plist><dict>",
            "<key>MozillaDeveloperRepoPath</key>",
            `<string>${stagingDeveloperPath}</string>`,
          ].join("\n")
          : [
            '<?xml version="1.0" encoding="UTF-8"?>',
            "<plist><dict>",
            "<key>MozillaDeveloperRepoPath</key>",
            `<string>${stagingDeveloperPath}</string>`,
            "<key>MozillaDeveloperObjPath</key>",
            `<string>${stagingDeveloperPath}</string>`,
            "</dict></plist>",
          ].join("\n");
        await Deno.writeTextFile(
          path.join(contentsRoot, "Info.plist"),
          infoPlist,
        );
      }
    },
    extractCompanion: async (_archive, destination) => {
      await Deno.writeTextFile(
        `${destination}/application.ini`,
        [
          "[App]",
          "Name=Floorp",
          `Version=${artifact.version}`,
          `BuildID=${artifact.buildId}`,
          "",
        ].join("\n"),
      );
    },
    rename: options.rename ?? ((from, to) => Deno.rename(from, to)),
    remove: options.remove ?? ((filePath) => Deno.remove(filePath)),
    nonce: () => options.nonce,
  };
}

function fakeLockedOperationsWithoutNetwork(
  artifact: RuntimeArtifact,
  options: FakeOperationOptions,
): Partial<LockedRuntimeOperations> {
  const operations = fakeLockedOperations(artifact, options);
  const {
    fetchRelease: _fetchRelease,
    download: _download,
    ...withoutNetwork
  } = operations;
  return withoutNetwork;
}

async function writeOldRuntime(binRoot: string): Promise<void> {
  await Deno.mkdir(`${binRoot}/floorp/.git`, { recursive: true });
  await Deno.mkdir(`${binRoot}/applied_patches`, { recursive: true });
  await Deno.writeTextFile(`${binRoot}/floorp/old.txt`, "old-runtime");
  await Deno.writeTextFile(`${binRoot}/floorp/.git/config`, "old-git");
  await Deno.writeTextFile(`${binRoot}/applied_patches/marker`, "old-patch");
}

interface RunProbe {
  dependencies: InitializerRunDependencies;
  calls: {
    loadLock: number;
    installLock: number;
    installDebug: number;
    savePrefs: number;
  };
}

function runProbe(): RunProbe {
  const calls = { loadLock: 0, installLock: 0, installDebug: 0, savePrefs: 0 };
  return {
    calls,
    dependencies: {
      loadLock: () => {
        calls.loadLock += 1;
        return Promise.resolve(lockedRuntime());
      },
      installLock: () => {
        calls.installLock += 1;
        return Promise.resolve();
      },
      installDebug: () => {
        calls.installDebug += 1;
        return Promise.resolve();
      },
      savePrefs: () => {
        calls.savePrefs += 1;
      },
    },
  };
}

Deno.test("run uses locked initialization by default", async () => {
  const probe = runProbe();
  await run(probe.dependencies);
  assertEquals(probe.calls, {
    loadLock: 1,
    installLock: 1,
    installDebug: 0,
    savePrefs: 1,
  });
});

Deno.test("run selects the Debug FTP initializer without loading the Release lock", async () => {
  const probe = runProbe();
  await run({ ...probe.dependencies, distribution: "debug" });
  assertEquals(probe.calls, {
    loadLock: 0,
    installLock: 0,
    installDebug: 1,
    savePrefs: 1,
  });
});

Deno.test("run ignores legacy environment and initializer hooks", async () => {
  const environmentReaders = [
    () => undefined,
    () => "1",
    () => {
      throw new Deno.errors.PermissionDenied("injected permission denial");
    },
    () => {
      throw new Deno.errors.NotCapable("injected env capability denial");
    },
  ];
  for (const readEnvironment of environmentReaders) {
    const probe = runProbe();
    let environmentReads = 0;
    let legacyRuns = 0;
    const legacyDependencies = {
      ...probe.dependencies,
      getEnv: () => {
        environmentReads += 1;
        return readEnvironment();
      },
      runLegacy: () => {
        legacyRuns += 1;
        return Promise.resolve();
      },
    };
    await run(legacyDependencies);
    assertEquals(environmentReads, 0);
    assertEquals(legacyRuns, 0);
    assertEquals(probe.calls, {
      loadLock: 1,
      installLock: 1,
      installDebug: 0,
      savePrefs: 1,
    });
  }
});

Deno.test("run does not save preferences when locked installation fails", async () => {
  const probe = runProbe();
  probe.dependencies.installLock = () => {
    probe.calls.installLock += 1;
    return Promise.reject(new Error("injected locked installation failure"));
  };
  await assertRejects(
    () => run(probe.dependencies),
    Error,
    "injected locked installation failure",
  );
  assertEquals(probe.calls, {
    loadLock: 1,
    installLock: 1,
    installDebug: 0,
    savePrefs: 0,
  });
});

Deno.test("run does not install or save when the Runtime lock fails to load", async () => {
  const probe = runProbe();
  probe.dependencies.loadLock = () => {
    probe.calls.loadLock += 1;
    return Promise.reject(new Error("injected Runtime lock load failure"));
  };
  await assertRejects(
    () => run(probe.dependencies),
    Error,
    "injected Runtime lock load failure",
  );
  assertEquals(probe.calls, {
    loadLock: 1,
    installLock: 0,
    installDebug: 0,
    savePrefs: 0,
  });
});

Deno.test("combined browser workflow does not require a Runtime opt-in", async () => {
  const workflow = await Deno.readTextFile(
    new URL(
      "../../.github/workflows/colocated_runner_test.yml",
      import.meta.url,
    ),
  );
  assertEquals(workflow.includes("FLOORP_RUNTIME_LOCKED"), false);
});

Deno.test("locked Runtime native target mapping fails closed", () => {
  assertEquals(resolveNativeRuntimeTarget("windows", "x86_64"), {
    platform: "windows",
    architecture: "x86_64",
  });
  assertEquals(resolveNativeRuntimeTarget("linux", "aarch64"), {
    platform: "linux",
    architecture: "aarch64",
  });
  assertEquals(resolveNativeRuntimeTarget("darwin", "aarch64"), {
    platform: "macos",
    architecture: "universal",
  });
  assertThrows(() => resolveNativeRuntimeTarget("windows", "aarch64"));
  assertThrows(() => resolveNativeRuntimeTarget("freebsd", "x86_64"));
});

Deno.test("locked Runtime install validates then swaps and retains recovery state", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedArtifact();
    const binRoot = `${root}/bin`;
    const profile = `${root}/profile/test`;
    await writeOldRuntime(binRoot);
    await Deno.mkdir(profile, { recursive: true });
    await Deno.writeTextFile(`${root}/marionette-port.txt`, "2828");
    await Deno.writeTextFile(`${profile}/nora-tests-control.json`, "stale");
    await Deno.writeTextFile(`${profile}/prefs.js`, "current-prefs");

    const result = await installLockedRuntime({
      lock: lockedRuntime(artifact),
      target: { platform: "windows", architecture: "x86_64" },
      binRootDir: binRoot,
      profileDir: profile,
      operations: fakeLockedOperations(artifact, { nonce: "success" }),
    });

    assertEquals(result.reused, false);
    assert(result.backupPath);
    assert(result.controlBackupPath);
    assertEquals(
      await Deno.readTextFile(`${result.backupPath}/floorp/old.txt`),
      "old-runtime",
    );
    assert(await Deno.stat(`${result.backupPath}/applied_patches/marker`));
    assert(await Deno.stat(`${result.backupPath}/floorp/.git/config`));
    const layout = runtimeLayoutFor(binRoot, artifact);
    assertEquals((await Deno.stat(layout.executable)).isFile, true);
    assertEquals(await Deno.readTextFile(layout.legacyVersionFile), "002");
    assertEquals(
      (await Deno.readTextFile(`${binRoot}/.floorp-runtime-lock.json`))
        .includes(
          artifact.buildId,
        ),
      true,
    );
    await assertRejects(() => Deno.stat(`${binRoot}/applied_patches`));
    await assertRejects(() => Deno.stat(`${binRoot}/floorp/.git`));
    assertEquals(
      await Deno.readTextFile(
        `${result.controlBackupPath}/marionette-port.txt`,
      ),
      "2828",
    );
    await assertRejects(() => Deno.stat(`${root}/marionette-port.txt`));
    assertEquals(
      await Deno.readTextFile(`${profile}/nora-tests-control.json`),
      "stale",
    );
    assertEquals(
      await Deno.readTextFile(`${profile}/prefs.js`),
      "current-prefs",
    );
    await assertRejects(() =>
      Deno.stat(
        `${result.controlBackupPath}/profile/test/nora-tests-control.json`,
      )
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("locked macOS Runtime commits only final developer paths", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedMacArtifact();
    const binRoot = path.resolve(`${root}/bin`);
    const nonce = "mac-final-path";

    await installLockedRuntime({
      lock: lockedRuntime(artifact),
      target: { platform: "macos", architecture: "universal" },
      binRootDir: binRoot,
      profileDir: `${root}/profile/test`,
      operations: fakeLockedOperations(artifact, { nonce }),
    });

    const layout = runtimeLayoutFor(binRoot, artifact);
    const infoPlistPath = path.join(
      path.dirname(path.dirname(layout.applicationIni)),
      "Info.plist",
    );
    const content = await Deno.readTextFile(infoPlistPath);
    const finalDeveloperPath = path.join(binRoot, "floorp");
    assertEquals(
      content.split("MozillaDeveloperRepoPath").length - 1,
      1,
    );
    assertEquals(
      content.split("MozillaDeveloperObjPath").length - 1,
      1,
    );
    assertEquals(
      content.split(`<string>${finalDeveloperPath}</string>`).length - 1,
      2,
    );
    assertEquals(content.includes(`runtime-staging-${nonce}`), false);
    await assertRejects(() => Deno.stat(`${binRoot}.runtime-staging-${nonce}`));

    const noDownload = fakeLockedOperations(artifact, {
      nonce: "mac-reuse",
    });
    noDownload.download = () =>
      Promise.reject(new Error("stale macOS plist triggered replacement"));
    const reused = await installLockedRuntime({
      lock: lockedRuntime(artifact),
      target: { platform: "macos", architecture: "universal" },
      binRootDir: binRoot,
      profileDir: `${root}/profile/test`,
      operations: noDownload,
    });
    assertEquals(reused.reused, true);

    await Deno.writeTextFile(
      infoPlistPath,
      content.replace(
        `<string>${finalDeveloperPath}</string>`,
        "<string>/stale/runtime-staging-path/floorp</string>",
      ),
    );
    await assertRejects(
      () =>
        installLockedRuntime({
          lock: lockedRuntime(artifact),
          target: { platform: "macos", architecture: "universal" },
          binRootDir: binRoot,
          profileDir: `${root}/profile/test`,
          operations: noDownload,
        }),
      Error,
      "stale macOS plist triggered replacement",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("locked macOS Runtime preserves dollar replacement tokens in final developer paths", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedMacArtifact();
    const binRoot = path.resolve(root, "$1-$&", "bin");

    await installLockedRuntime({
      lock: lockedRuntime(artifact),
      target: { platform: "macos", architecture: "universal" },
      binRootDir: binRoot,
      profileDir: path.resolve(root, "profile/test"),
      operations: fakeLockedOperations(artifact, {
        nonce: "mac-dollar-path",
      }),
    });

    const layout = runtimeLayoutFor(binRoot, artifact);
    const infoPlistPath = path.join(
      path.dirname(path.dirname(layout.applicationIni)),
      "Info.plist",
    );
    const content = await Deno.readTextFile(infoPlistPath);
    const finalDeveloperPath = path.join(binRoot, "floorp");
    const escapedFinalDeveloperPath = finalDeveloperPath.replaceAll(
      "&",
      "&amp;",
    );
    assertEquals(
      content.split(`<string>${escapedFinalDeveloperPath}</string>`).length - 1,
      2,
    );
    assertEquals(content.includes("$1-$&amp;"), true);
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("locked macOS plist patch failure aborts before swap and invalidates control", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedMacArtifact();
    const binRoot = path.resolve(`${root}/bin`);
    const profile = path.resolve(`${root}/profile/test`);
    const nonce = "mac-plist-failure";
    await writeOldRuntime(binRoot);
    await Deno.mkdir(profile, { recursive: true });
    await Deno.writeTextFile(`${root}/marionette-port.txt`, "2828");
    await Deno.writeTextFile(
      `${profile}/nora-tests-control.json`,
      "current-control",
    );
    await Deno.writeTextFile(`${profile}/prefs.js`, "current-prefs");

    await assertRejects(
      () =>
        installLockedRuntime({
          lock: lockedRuntime(artifact),
          target: { platform: "macos", architecture: "universal" },
          binRootDir: binRoot,
          profileDir: profile,
          operations: fakeLockedOperations(artifact, {
            nonce,
            malformedMacInfoPlist: true,
          }),
        }),
      Error,
      "missing its closing </dict>",
    );
    assertEquals(
      await Deno.readTextFile(`${binRoot}/floorp/old.txt`),
      "old-runtime",
    );
    assertEquals(
      await Deno.readTextFile(`${root}/marionette-port.txt`),
      "2828",
    );
    await assertRejects(() => Deno.stat(`${profile}/nora-tests-control.json`));
    assertEquals(
      await Deno.readTextFile(
        `${profile}/nora-tests-control.runtime-quarantine-${nonce}.json`,
      ),
      "current-control",
    );
    assertEquals(
      await Deno.readTextFile(`${profile}/prefs.js`),
      "current-prefs",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("locked Runtime ignores release metadata download URLs", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedArtifact();
    const downloadUrls: string[] = [];
    const operations = fakeLockedOperations(artifact, {
      nonce: "verified-public-urls",
      downloadUrls,
    });
    operations.fetchRelease = () => {
      throw new Error("public native validation must not fetch metadata");
    };
    await installLockedRuntime({
      lock: lockedRuntime(artifact),
      target: { platform: "windows", architecture: "x86_64" },
      binRootDir: `${root}/bin`,
      profileDir: `${root}/profile/test`,
      operations,
    });
    assertEquals(
      downloadUrls.toSorted(),
      [
        `https://github.com/Floorp-Projects/Floorp-Runtime/releases/download/daily-test/${artifact.asset.name}`,
        `https://github.com/Floorp-Projects/Floorp-Runtime/releases/download/daily-test/${artifact.applicationIniAsset.name}`,
      ].toSorted(),
    );
    assertEquals(
      downloadUrls.some((url) => url.includes("malicious.invalid")),
      false,
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("trusted Runtime metadata validation uses scoped optional auth", async () => {
  const artifact = lockedArtifact();
  const token = "runtime-token-value";
  let authenticated = false;
  const release = await validateLockedRuntimeReleaseMetadata({
    lock: lockedRuntime(artifact),
    githubToken: `  ${token}  `,
    fetchImpl: (input, init) => {
      assertEquals(
        String(input),
        "https://api.github.com/repos/Floorp-Projects/Floorp-Runtime/releases/100",
      );
      const headers = new Headers(init?.headers);
      assertEquals(headers.get("authorization"), `Bearer ${token}`);
      assertEquals(headers.get("accept"), "application/vnd.github+json");
      assertEquals(init?.redirect, "manual");
      authenticated = true;
      return Promise.resolve(
        new Response(JSON.stringify(releaseMetadataPayload(artifact)), {
          status: 200,
          headers: { "content-type": "application/json" },
        }),
      );
    },
  });
  assertEquals(authenticated, true);
  assertEquals(release.id, 100);

  await validateLockedRuntimeReleaseMetadata({
    lock: lockedRuntime(artifact),
    fetchImpl: (_input, init) => {
      assertEquals(new Headers(init?.headers).get("authorization"), null);
      return Promise.resolve(
        new Response(JSON.stringify(releaseMetadataPayload(artifact)), {
          status: 200,
          headers: { "content-type": "application/json" },
        }),
      );
    },
  });
});

Deno.test("trusted Runtime metadata validation fails closed on lock drift", async () => {
  const artifact = lockedArtifact();
  const lock = lockedRuntime(artifact);
  const driftCases: Array<{
    name: string;
    mutate: (release: LockedReleaseMetadata) => void;
    expectedMessage: string;
  }> = [
    {
      name: "release id",
      mutate: (release) => release.id += 1,
      expectedMessage: "release identity",
    },
    {
      name: "release tag",
      mutate: (release) => release.tagName = "other-tag",
      expectedMessage: "release identity",
    },
    {
      name: "release immutable flag",
      mutate: (release) => release.immutable = !release.immutable,
      expectedMessage: "release identity",
    },
    {
      name: "asset id",
      mutate: (release) => release.assets[0].id += 1,
      expectedMessage: "Expected exactly one GitHub release asset",
    },
    {
      name: "asset name",
      mutate: (release) => release.assets[0].name = "other.zip",
      expectedMessage: "does not match the lock",
    },
    {
      name: "asset size",
      mutate: (release) => release.assets[0].size += 1,
      expectedMessage: "does not match the lock",
    },
    {
      name: "asset digest",
      mutate: (release) => {
        release.assets[0].digest = `sha256:${"0".repeat(64)}`;
      },
      expectedMessage: "does not match the lock",
    },
  ];

  for (const driftCase of driftCases) {
    const mismatch = structuredClone(releaseMetadata(artifact));
    driftCase.mutate(mismatch);
    await assertRejects(
      () =>
        validateLockedRuntimeReleaseMetadata({
          lock,
          operations: {
            fetchRelease: () => Promise.resolve(mismatch),
          },
        }),
      Error,
      driftCase.expectedMessage,
      driftCase.name,
    );
  }
});

Deno.test("trusted Runtime metadata errors redact token and redirect URL", async () => {
  const artifact = lockedArtifact();
  const token = "runtime-secret-token";
  const signedQuery = "signed-query-secret";
  const error = await assertRejects(
    () =>
      validateLockedRuntimeReleaseMetadata({
        lock: lockedRuntime(artifact),
        githubToken: token,
        fetchImpl: () =>
          Promise.resolve(
            new Response(null, {
              status: 302,
              headers: {
                location:
                  `https://release-assets.githubusercontent.com/file?sig=${signedQuery}`,
              },
            }),
          ),
      }),
    Error,
    "Unexpected redirect",
  );
  assertEquals(error.message.includes(token), false);
  assertEquals(error.message.includes(signedQuery), false);
});

Deno.test("public Runtime downloads support direct and redirected HTTPS responses", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedArtifact();
    const calls: Array<{ url: string; authorization: string | null }> = [];
    const destinationRoot = path.resolve(`${root}/validated`);
    await validateLockedRuntimeArtifact({
      lock: lockedRuntime(artifact),
      destinationRoot,
      target: { platform: "windows", architecture: "x86_64" },
      operations: fakeLockedOperationsWithoutNetwork(artifact, {
        nonce: "public-network",
      }),
      fetchImpl: (input, init) => {
        const url = new URL(String(input));
        const headers = new Headers(init?.headers);
        calls.push({
          url: url.toString(),
          authorization: headers.get("authorization"),
        });
        assertEquals(init?.redirect, "manual");
        if (url.hostname === "github.com") {
          if (url.pathname.endsWith(`/${artifact.asset.name}`)) {
            return Promise.resolve(
              new Response(new Uint8Array(artifact.asset.size), {
                status: 200,
              }),
            );
          }
          return Promise.resolve(
            new Response(null, {
              status: 302,
              headers: {
                location:
                  `https://release-assets.githubusercontent.com/${artifact.applicationIniAsset.name}?sig=temporary`,
              },
            }),
          );
        }
        assertEquals(url.hostname, "release-assets.githubusercontent.com");
        return Promise.resolve(
          new Response(
            new Uint8Array(artifact.applicationIniAsset.size),
            { status: 200 },
          ),
        );
      },
    });
    assertEquals(calls.length, 3);
    assertEquals(calls.every((call) => call.authorization === null), true);
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("public Runtime download failures redact signed redirect URLs", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedArtifact();
    const signedQuery = "signed-query-secret";
    const error = await assertRejects(
      () =>
        validateLockedRuntimeArtifact({
          lock: lockedRuntime(artifact),
          destinationRoot: path.resolve(`${root}/validated`),
          target: { platform: "windows", architecture: "x86_64" },
          operations: fakeLockedOperationsWithoutNetwork(artifact, {
            nonce: "public-network-error",
          }),
          fetchImpl: () =>
            Promise.resolve(
              new Response(null, {
                status: 302,
                headers: {
                  location: `http://example.invalid/file?sig=${signedQuery}`,
                },
              }),
            ),
        }),
      AggregateError,
      "after all parallel operations settled",
    );
    assertEquals(error.message.includes(signedQuery), false);
    assertEquals(error.message.includes("example.invalid"), false);
    assert(error.message.includes("must use HTTPS"));
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("browser CI keeps Runtime downloads tokenless", async () => {
  const workflow = await Deno.readTextFile(
    new URL(
      "../../.github/workflows/colocated_runner_test.yml",
      import.meta.url,
    ),
  );
  const nativeValidationStart = workflow.indexOf(
    "- name: Validate locked native Runtime artifact",
  );
  const browserInstallStart = workflow.indexOf(
    "- name: Install locked Runtime for browser suite",
  );
  const sourceReadStart = workflow.indexOf(
    "- name: Read locked Runtime source",
  );
  assert(nativeValidationStart >= 0);
  assert(browserInstallStart > nativeValidationStart);
  assert(sourceReadStart > browserInstallStart);
  const publicRuntimeSteps = workflow.slice(
    nativeValidationStart,
    sourceReadStart,
  );
  assertEquals(publicRuntimeSteps.includes("GITHUB_TOKEN"), false);
  assert(publicRuntimeSteps.includes("validate-native"));
  assert(publicRuntimeSteps.includes("install-native"));

  const trustedMetadataStart = workflow.indexOf(
    "- name: Validate locked Runtime release metadata",
  );
  const smokeStart = workflow.indexOf("- run: deno task test:smoke");
  assert(trustedMetadataStart >= 0);
  assert(smokeStart > trustedMetadataStart);
  const trustedMetadataStep = workflow.slice(
    trustedMetadataStart,
    smokeStart,
  );
  assert(trustedMetadataStep.includes("github.event_name != 'pull_request'"));
  assert(trustedMetadataStep.includes("FLOORP_RUNTIME_GITHUB_TOKEN"));
  assert(trustedMetadataStep.includes("validate-release-metadata"));
});

Deno.test("locked Runtime waits for all failed downloads before cleanup", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedArtifact();
    const nonce = "download-all-settled";
    const destinationRoot = path.resolve(`${root}/validated`);
    const operations = fakeLockedOperations(artifact, { nonce });
    operations.download = (url, destination) => {
      if (url.endsWith(`/${encodeURIComponent(artifact.asset.name)}`)) {
        throw new Error("fast main download failure");
      }
      return (async () => {
        await new Promise((resolve) => setTimeout(resolve, 40));
        await Deno.writeFile(
          destination,
          new Uint8Array(artifact.applicationIniAsset.size),
        );
        throw new Error("delayed companion download failure");
      })();
    };

    const error = await assertRejects(
      () =>
        validateLockedRuntimeArtifact({
          lock: lockedRuntime(artifact),
          destinationRoot,
          target: { platform: "windows", architecture: "x86_64" },
          operations,
        }),
      AggregateError,
      "after all parallel operations settled",
    );
    assert(error.message.includes("fast main download failure"));
    assert(error.message.includes("delayed companion download failure"));
    await new Promise((resolve) => setTimeout(resolve, 80));
    await assertRejects(() => Deno.stat(destinationRoot));
    await assertRejects(() =>
      Deno.stat(
        `${root}/.runtime-download-${nonce}-${artifact.asset.name}`,
      )
    );
    await assertRejects(() =>
      Deno.stat(
        `${root}/.runtime-download-${nonce}-${artifact.applicationIniAsset.name}`,
      )
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("locked Runtime waits for all failed hashes before cleanup", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedArtifact();
    const nonce = "hash-all-settled";
    const destinationRoot = path.resolve(`${root}/validated`);
    let delayedHashObservedArchive = false;
    const operations = fakeLockedOperations(artifact, { nonce });
    operations.sha256 = (filePath) => {
      if (filePath.endsWith(artifact.asset.name)) {
        throw new Error("fast main hash failure");
      }
      return (async () => {
        await new Promise((resolve) => setTimeout(resolve, 40));
        await Deno.readFile(filePath);
        delayedHashObservedArchive = true;
        throw new Error("delayed companion hash failure");
      })();
    };

    const error = await assertRejects(
      () =>
        validateLockedRuntimeArtifact({
          lock: lockedRuntime(artifact),
          destinationRoot,
          target: { platform: "windows", architecture: "x86_64" },
          operations,
        }),
      AggregateError,
      "after all parallel operations settled",
    );
    assert(error.message.includes("fast main hash failure"));
    assert(error.message.includes("delayed companion hash failure"));
    assertEquals(delayedHashObservedArchive, true);
    await assertRejects(() => Deno.stat(destinationRoot));
    await assertRejects(() =>
      Deno.stat(
        `${root}/.runtime-download-${nonce}-${artifact.asset.name}`,
      )
    );
    await assertRejects(() =>
      Deno.stat(
        `${root}/.runtime-download-${nonce}-${artifact.applicationIniAsset.name}`,
      )
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

for (
  const failure of [
    { name: "digest mismatch", digestMismatch: true },
    { name: "BuildID mismatch", buildIdMismatch: true },
    { name: "extraction failure", extractionFailure: true },
    { name: "missing executable", missingExecutable: true },
  ] as const
) {
  Deno.test(`locked Runtime ${failure.name} leaves the live tree untouched`, async () => {
    const root = await Deno.makeTempDir();
    try {
      const artifact = lockedArtifact();
      const binRoot = `${root}/bin`;
      const profile = `${root}/profile/test`;
      const nonce = failure.name.replaceAll(" ", "-");
      await writeOldRuntime(binRoot);
      await Deno.mkdir(profile, { recursive: true });
      await Deno.writeTextFile(`${root}/marionette-port.txt`, "2828");
      await Deno.writeTextFile(
        `${profile}/nora-tests-control.json`,
        "current-control",
      );
      await Deno.writeTextFile(`${profile}/prefs.js`, "current-prefs");
      await assertRejects(
        () =>
          installLockedRuntime({
            lock: lockedRuntime(artifact),
            target: { platform: "windows", architecture: "x86_64" },
            binRootDir: binRoot,
            profileDir: profile,
            operations: fakeLockedOperations(artifact, {
              nonce,
              digestMismatch: failure.digestMismatch,
              buildIdMismatch: failure.buildIdMismatch,
              extractionFailure: failure.extractionFailure,
              missingExecutable: failure.missingExecutable,
            }),
          }),
      );
      assertEquals(
        await Deno.readTextFile(`${binRoot}/floorp/old.txt`),
        "old-runtime",
      );
      assertEquals(
        await Deno.readTextFile(`${root}/marionette-port.txt`),
        "2828",
      );
      assertEquals(
        await Deno.readTextFile(
          `${profile}/nora-tests-control.runtime-quarantine-${nonce}.json`,
        ),
        "current-control",
      );
      await assertRejects(() =>
        Deno.stat(`${profile}/nora-tests-control.json`)
      );
      assertEquals(
        await Deno.readTextFile(`${profile}/prefs.js`),
        "current-prefs",
      );
      await assertRejects(() =>
        Deno.stat(`${root}/runtime-control-backup-${nonce}`)
      );
    } finally {
      await Deno.remove(root, { recursive: true });
    }
  });
}

Deno.test("locked Runtime control backup collision rejects without deleting preexisting recovery data", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedArtifact();
    const binRoot = `${root}/bin`;
    const profile = `${root}/profile/test`;
    const nonce = "control-backup-collision";
    const controlBackupRoot = `${root}/runtime-control-backup-${nonce}`;
    const sentinel = `${controlBackupRoot}/recovery-sentinel`;
    await writeOldRuntime(binRoot);
    await Deno.mkdir(profile, { recursive: true });
    await Deno.mkdir(controlBackupRoot);
    await Deno.writeTextFile(sentinel, "preserve");
    await Deno.writeTextFile(`${root}/marionette-port.txt`, "2828");

    await assertRejects(
      () =>
        installLockedRuntime({
          lock: lockedRuntime(artifact),
          target: { platform: "windows", architecture: "x86_64" },
          binRootDir: binRoot,
          profileDir: profile,
          operations: fakeLockedOperations(artifact, { nonce }),
        }),
      Deno.errors.AlreadyExists,
    );
    assertEquals(await Deno.readTextFile(sentinel), "preserve");
    assertEquals(
      await Deno.readTextFile(`${root}/marionette-port.txt`),
      "2828",
    );
    assertEquals(
      await Deno.readTextFile(`${binRoot}/floorp/old.txt`),
      "old-runtime",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("locked Runtime transaction collision invalidates control without deleting recovery data", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedArtifact();
    const binRoot = `${root}/bin`;
    const profile = `${root}/profile/test`;
    const nonce = "transaction-collision";
    const stageRoot = `${binRoot}.runtime-staging-${nonce}`;
    await writeOldRuntime(binRoot);
    await Deno.mkdir(profile, { recursive: true });
    await Deno.mkdir(stageRoot, { recursive: true });
    await Deno.writeTextFile(`${stageRoot}/recovery-sentinel`, "preserve");
    await Deno.writeTextFile(`${root}/marionette-port.txt`, "2828");
    await Deno.writeTextFile(
      `${profile}/nora-tests-control.json`,
      "current-control",
    );
    await Deno.writeTextFile(`${profile}/prefs.js`, "current-prefs");

    await assertRejects(
      () =>
        installLockedRuntime({
          lock: lockedRuntime(artifact),
          target: { platform: "windows", architecture: "x86_64" },
          binRootDir: binRoot,
          profileDir: profile,
          operations: fakeLockedOperations(artifact, { nonce }),
        }),
      Error,
      "Refusing to reuse an existing locked Runtime transaction path",
    );
    await assertRejects(() => Deno.stat(`${profile}/nora-tests-control.json`));
    assertEquals(
      await Deno.readTextFile(
        `${profile}/nora-tests-control.runtime-quarantine-${nonce}.json`,
      ),
      "current-control",
    );
    assertEquals(
      await Deno.readTextFile(`${stageRoot}/recovery-sentinel`),
      "preserve",
    );
    assertEquals(
      await Deno.readTextFile(`${binRoot}/floorp/old.txt`),
      "old-runtime",
    );
    assertEquals(
      await Deno.readTextFile(`${root}/marionette-port.txt`),
      "2828",
    );
    assertEquals(
      await Deno.readTextFile(`${profile}/prefs.js`),
      "current-prefs",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("locked Runtime swap failure restores the previous live tree", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedArtifact();
    const binRoot = `${root}/bin`;
    const profile = `${root}/profile/test`;
    const nonce = "swap-failure";
    await writeOldRuntime(binRoot);
    await Deno.mkdir(profile, { recursive: true });
    await Deno.writeTextFile(`${root}/marionette-port.txt`, "2828");
    await Deno.writeTextFile(
      `${profile}/nora-tests-control.json`,
      "current-control",
    );
    await Deno.writeTextFile(`${profile}/prefs.js`, "current-prefs");
    const rename: LockedRuntimeOperations["rename"] = (from, to) => {
      if (
        from.endsWith(`bin.runtime-staging-${nonce}`) &&
        path.resolve(to) === path.resolve(binRoot)
      ) {
        return Promise.reject(new Error("injected swap failure"));
      }
      return Deno.rename(from, to);
    };
    await assertRejects(
      () =>
        installLockedRuntime({
          lock: lockedRuntime(artifact),
          target: { platform: "windows", architecture: "x86_64" },
          binRootDir: binRoot,
          profileDir: profile,
          operations: fakeLockedOperations(artifact, { nonce, rename }),
        }),
      Error,
      "injected swap failure",
    );
    assertEquals(
      await Deno.readTextFile(`${binRoot}/floorp/old.txt`),
      "old-runtime",
    );
    await assertRejects(() => Deno.stat(`${binRoot}.runtime-backup-${nonce}`));
    assertEquals(
      await Deno.readTextFile(`${root}/marionette-port.txt`),
      "2828",
    );
    assertEquals(
      await Deno.readTextFile(
        `${profile}/nora-tests-control.runtime-quarantine-${nonce}.json`,
      ),
      "current-control",
    );
    await assertRejects(() => Deno.stat(`${profile}/nora-tests-control.json`));
    assertEquals(
      await Deno.readTextFile(`${profile}/prefs.js`),
      "current-prefs",
    );
    await assertRejects(() =>
      Deno.stat(`${root}/runtime-control-backup-${nonce}`)
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("locked Runtime failure removes live control when quarantine rename fails", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedArtifact();
    const binRoot = `${root}/bin`;
    const profile = `${root}/profile/test`;
    const nonce = "quarantine-fallback";
    const liveControl = path.resolve(
      `${profile}/nora-tests-control.json`,
    );
    await writeOldRuntime(binRoot);
    await Deno.mkdir(profile, { recursive: true });
    await Deno.writeTextFile(`${root}/marionette-port.txt`, "2828");
    await Deno.writeTextFile(liveControl, "current-control");
    await Deno.writeTextFile(`${profile}/prefs.js`, "current-prefs");
    const rename: LockedRuntimeOperations["rename"] = (from, to) => {
      if (path.resolve(from) === liveControl) {
        return Promise.reject(new Error("injected quarantine failure"));
      }
      return Deno.rename(from, to);
    };

    await assertRejects(
      () =>
        installLockedRuntime({
          lock: lockedRuntime(artifact),
          target: { platform: "windows", architecture: "x86_64" },
          binRootDir: binRoot,
          profileDir: profile,
          operations: fakeLockedOperations(artifact, {
            nonce,
            digestMismatch: true,
            rename,
          }),
        }),
      Error,
      "SHA-256 mismatch",
    );
    await assertRejects(() => Deno.stat(liveControl));
    await assertRejects(() =>
      Deno.stat(
        `${profile}/nora-tests-control.runtime-quarantine-${nonce}.json`,
      )
    );
    assertEquals(
      await Deno.readTextFile(`${binRoot}/floorp/old.txt`),
      "old-runtime",
    );
    assertEquals(
      await Deno.readTextFile(`${root}/marionette-port.txt`),
      "2828",
    );
    assertEquals(
      await Deno.readTextFile(`${profile}/prefs.js`),
      "current-prefs",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("locked Runtime failure reports live and recovery paths when control invalidation fails", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedArtifact();
    const binRoot = `${root}/bin`;
    const profile = `${root}/profile/test`;
    const nonce = "quarantine-and-removal-failure";
    const liveControl = path.resolve(
      `${profile}/nora-tests-control.json`,
    );
    const quarantinePath = path.resolve(
      `${profile}/nora-tests-control.runtime-quarantine-${nonce}.json`,
    );
    await writeOldRuntime(binRoot);
    await Deno.mkdir(profile, { recursive: true });
    await Deno.writeTextFile(`${root}/marionette-port.txt`, "2828");
    await Deno.writeTextFile(liveControl, "current-control");
    await Deno.writeTextFile(`${profile}/prefs.js`, "current-prefs");
    const rename: LockedRuntimeOperations["rename"] = (from, to) => {
      if (path.resolve(from) === liveControl) {
        return Promise.reject(new Error("injected quarantine failure"));
      }
      return Deno.rename(from, to);
    };
    const remove: LockedRuntimeOperations["remove"] = () =>
      Promise.reject(new Error("injected removal failure"));

    const error = await assertRejects(
      () =>
        installLockedRuntime({
          lock: lockedRuntime(artifact),
          target: { platform: "windows", architecture: "x86_64" },
          binRootDir: binRoot,
          profileDir: profile,
          operations: fakeLockedOperations(artifact, {
            nonce,
            digestMismatch: true,
            rename,
            remove,
          }),
        }),
      Error,
      "browser-test control invalidation also failed",
    );
    assert(error.message.includes(liveControl));
    assert(error.message.includes(quarantinePath));
    assert(error.message.includes("injected quarantine failure"));
    assert(error.message.includes("injected removal failure"));
    assertEquals(await Deno.readTextFile(liveControl), "current-control");
    assertEquals(
      await Deno.readTextFile(`${binRoot}/floorp/old.txt`),
      "old-runtime",
    );
    assertEquals(
      await Deno.readTextFile(`${root}/marionette-port.txt`),
      "2828",
    );
    assertEquals(
      await Deno.readTextFile(`${profile}/prefs.js`),
      "current-prefs",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("matching locked Runtime reuse preserves current test control state", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedArtifact();
    const binRoot = `${root}/bin`;
    const profile = `${root}/profile/test`;
    await installLockedRuntime({
      lock: lockedRuntime(artifact),
      target: { platform: "windows", architecture: "x86_64" },
      binRootDir: binRoot,
      profileDir: profile,
      operations: fakeLockedOperations(artifact, { nonce: "initial" }),
    });

    const control = '{"testFiles":["targeted.test.ts"]}';
    const marionettePort = "2828";
    const prefs = "current-prefs";
    await Deno.mkdir(profile, { recursive: true });
    await Deno.writeTextFile(
      `${profile}/nora-tests-control.json`,
      control,
    );
    await Deno.writeTextFile(`${profile}/prefs.js`, prefs);
    await Deno.writeTextFile(`${root}/marionette-port.txt`, marionettePort);

    const noDownload = fakeLockedOperations(artifact, { nonce: "reuse" });
    noDownload.download = () =>
      Promise.reject(new Error("matching Runtime unexpectedly downloaded"));
    const reused = await installLockedRuntime({
      lock: lockedRuntime(artifact),
      target: { platform: "windows", architecture: "x86_64" },
      binRootDir: binRoot,
      profileDir: profile,
      operations: noDownload,
    });

    assertEquals(reused.reused, true);
    assertEquals(reused.controlBackupPath, undefined);
    assertEquals(
      await Deno.readTextFile(`${profile}/nora-tests-control.json`),
      control,
    );
    assertEquals(await Deno.readTextFile(`${profile}/prefs.js`), prefs);
    assertEquals(
      await Deno.readTextFile(`${root}/marionette-port.txt`),
      marionettePort,
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("matching locked Runtime marker still validates the installed tree", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedArtifact();
    const binRoot = `${root}/bin`;
    await installLockedRuntime({
      lock: lockedRuntime(artifact),
      target: { platform: "windows", architecture: "x86_64" },
      binRootDir: binRoot,
      profileDir: `${root}/profile/test`,
      operations: fakeLockedOperations(artifact, { nonce: "initial" }),
    });

    const noDownload = fakeLockedOperations(artifact, { nonce: "reuse" });
    noDownload.download = () =>
      Promise.reject(new Error("matching Runtime unexpectedly downloaded"));
    const reused = await installLockedRuntime({
      lock: lockedRuntime(artifact),
      target: { platform: "windows", architecture: "x86_64" },
      binRootDir: binRoot,
      profileDir: `${root}/profile/test`,
      operations: noDownload,
    });
    assertEquals(reused.reused, true);

    const layout = runtimeLayoutFor(binRoot, artifact);
    await Deno.writeFile(layout.executable, new Uint8Array());
    await assertRejects(
      () =>
        installLockedRuntime({
          lock: lockedRuntime(artifact),
          target: { platform: "windows", architecture: "x86_64" },
          binRootDir: binRoot,
          profileDir: `${root}/profile/test`,
          operations: noDownload,
        }),
      Error,
      "matching Runtime unexpectedly downloaded",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("locked Runtime rollback failure retains the recovery backup path", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedArtifact();
    const binRoot = path.resolve(`${root}/bin`);
    const nonce = "rollback-failure";
    const backupRoot = `${binRoot}.runtime-backup-${nonce}`;
    await writeOldRuntime(binRoot);
    const rename: LockedRuntimeOperations["rename"] = (from, to) => {
      if (
        from.endsWith(`bin.runtime-staging-${nonce}`) && to === binRoot
      ) {
        return Promise.reject(new Error("injected swap failure"));
      }
      if (from === backupRoot && to === binRoot) {
        return Promise.reject(new Error("injected rollback failure"));
      }
      return Deno.rename(from, to);
    };
    await assertRejects(
      () =>
        installLockedRuntime({
          lock: lockedRuntime(artifact),
          target: { platform: "windows", architecture: "x86_64" },
          binRootDir: binRoot,
          profileDir: `${root}/profile/test`,
          operations: fakeLockedOperations(artifact, { nonce, rename }),
        }),
      Error,
      backupRoot,
    );
    assertEquals(
      await Deno.readTextFile(`${backupRoot}/floorp/old.txt`),
      "old-runtime",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});
