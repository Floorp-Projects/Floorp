// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert";
import * as path from "@std/path";
import {
  filterRuntimeEntries,
  type InitializerRunDependencies,
  installLockedRuntime,
  isLockedRuntimeRequested,
  type LockedReleaseMetadata,
  type LockedRuntimeOperations,
  pickRuntimeEntry,
  resolveNativeRuntimeTarget,
  run,
  runtimeLayoutFor,
  scoreRuntimeEntry,
  validateLockedRuntimeArtifact,
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
      const size = url.endsWith(`/${artifact.asset.id}`)
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

async function writeOldRuntime(binRoot: string): Promise<void> {
  await Deno.mkdir(`${binRoot}/floorp/.git`, { recursive: true });
  await Deno.mkdir(`${binRoot}/applied_patches`, { recursive: true });
  await Deno.writeTextFile(`${binRoot}/floorp/old.txt`, "old-runtime");
  await Deno.writeTextFile(`${binRoot}/floorp/.git/config`, "old-git");
  await Deno.writeTextFile(`${binRoot}/applied_patches/marker`, "old-patch");
}

Deno.test("locked Runtime opt-in accepts only the literal value 1", () => {
  assert(isLockedRuntimeRequested("1"));
  for (const value of [undefined, "", "true", "yes", "01", " 1 "]) {
    assertEquals(isLockedRuntimeRequested(value), false);
  }
});

interface RunProbe {
  dependencies: InitializerRunDependencies;
  calls: {
    legacy: number;
    loadLock: number;
    installLock: number;
    savePrefs: number;
  };
}

function runProbe(getEnv: InitializerRunDependencies["getEnv"]): RunProbe {
  const calls = { legacy: 0, loadLock: 0, installLock: 0, savePrefs: 0 };
  return {
    calls,
    dependencies: {
      getEnv,
      loadLock: () => {
        calls.loadLock += 1;
        return Promise.resolve(lockedRuntime());
      },
      installLock: () => {
        calls.installLock += 1;
        return Promise.resolve();
      },
      runLegacy: () => {
        calls.legacy += 1;
        return Promise.resolve();
      },
      savePrefs: () => {
        calls.savePrefs += 1;
      },
    },
  };
}

Deno.test("run falls back to legacy initialization when env access is unavailable", async () => {
  const getters: InitializerRunDependencies["getEnv"][] = [
    () => undefined,
    () => {
      throw new Deno.errors.PermissionDenied("injected permission denial");
    },
    () => {
      throw new Deno.errors.NotCapable("injected env capability denial");
    },
  ];
  for (const getEnv of getters) {
    const probe = runProbe(getEnv);
    await run(probe.dependencies);
    assertEquals(probe.calls, {
      legacy: 1,
      loadLock: 0,
      installLock: 0,
      savePrefs: 1,
    });
  }
});

Deno.test("run uses locked initialization only for the literal opt-in", async () => {
  const probe = runProbe(() => "1");
  await run(probe.dependencies);
  assertEquals(probe.calls, {
    legacy: 0,
    loadLock: 1,
    installLock: 1,
    savePrefs: 1,
  });
});

Deno.test("run rethrows unrelated environment errors", async () => {
  const probe = runProbe(() => {
    throw new TypeError("injected unrelated error");
  });
  await assertRejects(
    () => run(probe.dependencies),
    TypeError,
    "injected unrelated error",
  );
  assertEquals(probe.calls, {
    legacy: 0,
    loadLock: 0,
    installLock: 0,
    savePrefs: 0,
  });
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
    noDownload.fetchRelease = () =>
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
    await installLockedRuntime({
      lock: lockedRuntime(artifact),
      target: { platform: "windows", architecture: "x86_64" },
      binRootDir: `${root}/bin`,
      profileDir: `${root}/profile/test`,
      operations: fakeLockedOperations(artifact, {
        nonce: "verified-api-urls",
        downloadUrls,
      }),
    });
    assertEquals(
      downloadUrls.toSorted(),
      [
        `https://api.github.com/repos/Floorp-Projects/Floorp-Runtime/releases/assets/${artifact.asset.id}`,
        `https://api.github.com/repos/Floorp-Projects/Floorp-Runtime/releases/assets/${artifact.applicationIniAsset.id}`,
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

Deno.test("locked Runtime waits for all failed downloads before cleanup", async () => {
  const root = await Deno.makeTempDir();
  try {
    const artifact = lockedArtifact();
    const nonce = "download-all-settled";
    const destinationRoot = path.resolve(`${root}/validated`);
    const operations = fakeLockedOperations(artifact, { nonce });
    operations.download = (url, destination) => {
      if (url.endsWith(`/${artifact.asset.id}`)) {
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
    noDownload.fetchRelease = () =>
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
    noDownload.fetchRelease = () =>
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
