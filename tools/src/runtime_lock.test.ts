// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertThrows } from "@std/assert";
import {
  loadRuntimeLock,
  parseRuntimeLock,
  RUNTIME_LOCK_PATH,
  RUNTIME_REPOSITORY,
  type RuntimeArtifact,
  type RuntimeLock,
  RuntimeLockValidationError,
  type RuntimeMaterial,
} from "./runtime_lock.ts";

const canonicalLock = await loadRuntimeLock();

function cloneLock(): RuntimeLock {
  return structuredClone(canonicalLock);
}

function expectInvalid(
  mutate: (lock: RuntimeLock) => void,
  message: string,
): void {
  const lock = cloneLock();
  mutate(lock);
  assertThrows(
    () => parseRuntimeLock(lock),
    RuntimeLockValidationError,
    message,
  );
}

function artifact(
  lock: RuntimeLock,
  platform: RuntimeArtifact["platform"],
  architecture: RuntimeArtifact["architecture"],
): RuntimeArtifact {
  const result = lock.artifacts.find((entry) =>
    entry.platform === platform && entry.architecture === architecture
  );
  if (result === undefined) {
    throw new Error(
      `missing test fixture artifact ${platform}/${architecture}`,
    );
  }
  return result;
}

function material(lock: RuntimeLock, sourcePath: string): RuntimeMaterial {
  const result = lock.source.materials.entries.find((entry) =>
    entry.path === sourcePath
  );
  if (result === undefined) {
    throw new Error(`missing test fixture material ${sourcePath}`);
  }
  return result;
}

Deno.test("canonical Runtime lock pins the complete reviewed source closure", () => {
  assertEquals(RUNTIME_LOCK_PATH.protocol, "file:");
  assertEquals(canonicalLock.schemaVersion, 1);
  assertEquals(canonicalLock.source.repository, RUNTIME_REPOSITORY);
  assertEquals(canonicalLock.source.trackingRef, "nora-0.2.0");
  assertEquals(canonicalLock.source.ref, "daily-984");
  assertEquals(
    canonicalLock.source.commit,
    "cc0c8e744c9c4697722c7455888e29f1138dcd4f",
  );
  assertEquals(
    canonicalLock.source.tree,
    "19cdccd436b48f96af644f61a4c0a28e4a00e3ad",
  );
  assertEquals(canonicalLock.source.release, {
    id: 356105522,
    immutable: false,
  });
  assertEquals(canonicalLock.source.materials.count, 54);
  assertEquals(canonicalLock.source.materials.totalBytes, 223622);
  assertEquals(canonicalLock.source.tests.count, 8);
  assertEquals(canonicalLock.source.tests.totalTasks, 15);
  assertEquals(canonicalLock.source.tests.supportDependencyEdges, 94);

  const roleCounts = Object.groupBy(
    canonicalLock.source.materials.entries,
    (entry) => entry.role,
  );
  assertEquals(roleCounts.test?.length, 8);
  assertEquals(roleCounts.manifest?.length, 6);
  assertEquals(roleCounts["head-support"]?.length, 5);
  assertEquals(roleCounts.support?.length, 35);

  assertEquals(
    Object.fromEntries(
      canonicalLock.source.tests.entries.map((entry) => [
        entry.path,
        entry.expectedTasks,
      ]),
    ),
    {
      "browser/base/content/test/caps/browser_principalSerialization_version1.js":
        2,
      "browser/base/content/test/general/browser_bug537474.js": 1,
      "browser/base/content/test/general/browser_bug565575.js": 1,
      "browser/base/content/test/general/browser_bug596687.js": 1,
      "browser/base/content/test/performance/browser_hiddenwindow_existence.js":
        1,
      "browser/components/customizableui/test/browser_996364_registerArea_different_properties.js":
        7,
      "browser/components/tabbrowser/test/browser/tabs/browser_pinned_and_hidden_tabs.js":
        1,
      "browser/components/urlbar/tests/browser-UrlbarInput/browser_a11y.js": 1,
    },
  );
});

Deno.test("canonical Runtime lock pins each platform's own binary identity", () => {
  const expected = [
    {
      tuple: "linux/aarch64",
      assetId: 481483241,
      assetSize: 70443176,
      assetSha:
        "ea665c85df7ddc437d22722c387248dcca265952c2071e88437ea279be9df269",
      iniId: 481483244,
      iniSize: 537,
      iniSha:
        "acbfe303c1d756ca68e3b7305dc7a1b4fd144e566bd7f12b9be5d091f38b6f8e",
      buildId: "20260718073540",
    },
    {
      tuple: "linux/x86_64",
      assetId: 481483240,
      assetSize: 81878192,
      assetSha:
        "558c5c540f7b6a87421d1bc9e63adbf84a34c98945439381803f98a96465a957",
      iniId: 481483247,
      iniSize: 537,
      iniSha:
        "aa5d537da1ed4af5701704c82c244252da715d9d8d5f53933cf547bcaa10151f",
      buildId: "20260718102500",
    },
    {
      tuple: "macos/universal",
      assetId: 481483236,
      assetSize: 175982069,
      assetSha:
        "171b7d2446e554eeeffcc11f86cb747441c2d0b9cb3d431a5887674aa6d69845",
      iniId: 481483249,
      iniSize: 537,
      iniSha:
        "dde5aa33136dd545e2afb7aa5757e88a5a970fd38a0d42bdf8f55ecd25493bc8",
      buildId: "20260718072840",
    },
    {
      tuple: "windows/x86_64",
      assetId: 481483239,
      assetSize: 131544355,
      assetSha:
        "0f424c8698a35039f470debf3e875996cd571cf6abf415727e2efdf686fa62fd",
      iniId: 481483243,
      iniSize: 536,
      iniSha:
        "b22af470c7ab9f65822299fb4edd037512f1f804060502b59eff6d4822ac8564",
      buildId: "20260718112318",
    },
  ];

  assertEquals(
    canonicalLock.artifacts.map((entry) => ({
      tuple: `${entry.platform}/${entry.architecture}`,
      assetId: entry.asset.id,
      assetSize: entry.asset.size,
      assetSha: entry.asset.sha256,
      iniId: entry.applicationIniAsset.id,
      iniSize: entry.applicationIniAsset.size,
      iniSha: entry.applicationIniAsset.sha256,
      buildId: entry.buildId,
    })),
    expected,
  );
  assertEquals(
    canonicalLock.artifacts.map((entry) => entry.version),
    ["152.0.7", "152.0.7", "152.0.7", "152.0.7"],
  );
  assertEquals(
    artifact(canonicalLock, "windows", "x86_64").extractionPolicy,
    "zip-direct-floorp",
  );
});

Deno.test("loadRuntimeLock validates an explicit local file without network", async () => {
  const path = await Deno.makeTempFile({ suffix: ".json" });
  try {
    await Deno.writeTextFile(path, JSON.stringify(canonicalLock));
    assertEquals(await loadRuntimeLock(path), canonicalLock);
  } finally {
    await Deno.remove(path);
  }
});

Deno.test("loadRuntimeLock reports malformed JSON as a lock validation error", async () => {
  const path = await Deno.makeTempFile({ suffix: ".json" });
  try {
    await Deno.writeTextFile(path, "{");
    await assertRejects(
      () => loadRuntimeLock(path),
      RuntimeLockValidationError,
      "invalid runtime lock JSON",
    );
  } finally {
    await Deno.remove(path);
  }
});

Deno.test("parseRuntimeLock rejects unknown keys at every level", () => {
  const root = cloneLock() as unknown as Record<string, unknown>;
  root.unexpected = true;
  assertThrows(
    () => parseRuntimeLock(root),
    RuntimeLockValidationError,
    "unknown key",
  );

  expectInvalid((lock) => {
    const asset = lock.artifacts[0].asset as unknown as Record<string, unknown>;
    asset.url = "https://example.invalid/asset";
  }, "unknown key");

  expectInvalid((lock) => {
    const preference = lock.source.tests.manifests[2]
      .preferences[0] as unknown as Record<string, unknown>;
    preference.defaultBranch = true;
  }, "unknown key");
});

Deno.test("parseRuntimeLock rejects unsafe and root-escaping material paths", () => {
  for (
    const unsafe of [
      "../escape.js",
      "/absolute.js",
      "C:/drive.js",
      "browser\\windows.js",
      "browser//empty.js",
      "browser/./dot.js",
      "browser/segment. ",
      "browser/NUL.txt",
      "browser/file:stream.js",
    ]
  ) {
    expectInvalid((lock) => {
      lock.source.materials.entries[0].path = unsafe;
    }, "path");
  }
});

Deno.test("parseRuntimeLock rejects malformed source and material identities", () => {
  expectInvalid((lock) => {
    lock.source.repository = "Other/Runtime";
  }, RUNTIME_REPOSITORY);
  expectInvalid((lock) => {
    lock.source.ref = "../moving";
  }, "Git ref");
  expectInvalid((lock) => {
    lock.source.trackingRef = "";
  }, "Git ref");
  expectInvalid((lock) => {
    lock.source.commit = "A".repeat(40);
  }, "lowercase hexadecimal");
  expectInvalid((lock) => {
    lock.source.tree = "0".repeat(39);
  }, "lowercase hexadecimal");
  expectInvalid((lock) => {
    lock.source.release.id = 0;
  }, "safe integer");
  expectInvalid((lock) => {
    material(
      lock,
      "browser/base/content/test/caps/browser.toml",
    ).gitBlob = "g".repeat(40);
  }, "lowercase hexadecimal");
  expectInvalid((lock) => {
    material(
      lock,
      "browser/base/content/test/caps/browser.toml",
    ).sha256 = "0".repeat(63);
  }, "lowercase hexadecimal");
  expectInvalid((lock) => {
    material(
      lock,
      "browser/base/content/test/caps/browser.toml",
    ).mode = "100755" as "100644";
  }, "100644");
});

Deno.test("parseRuntimeLock rejects material totals, duplicates, and drift", () => {
  expectInvalid((lock) => {
    lock.source.materials.count += 1;
  }, "expected 54");
  expectInvalid((lock) => {
    lock.source.materials.totalBytes += 1;
  }, "expected 223622");
  expectInvalid((lock) => {
    lock.source.materials.entries[1].path =
      lock.source.materials.entries[0].path;
  }, "sorted and unique");
  expectInvalid((lock) => {
    lock.source.materials.entries[0].path = lock.source.materials.entries[1]
      .path.toUpperCase();
  }, "case-insensitive");
  expectInvalid((lock) => {
    const first = lock.source.materials.entries[0];
    lock.source.materials.entries[0] = lock.source.materials.entries[1];
    lock.source.materials.entries[1] = first;
  }, "sorted and unique");
  expectInvalid((lock) => {
    material(
      lock,
      "browser/base/content/test/caps/browser_principalSerialization_version1.js",
    ).role = "head-support";
  }, "head-support");
});

Deno.test("parseRuntimeLock rejects task, manifest, and support closure drift", () => {
  expectInvalid((lock) => {
    lock.source.tests.count += 1;
  }, "expected 8");
  expectInvalid((lock) => {
    lock.source.tests.totalTasks += 1;
  }, "expected 15");
  expectInvalid((lock) => {
    lock.source.tests.supportDependencyEdges -= 1;
  }, "expected 94");
  expectInvalid((lock) => {
    lock.source.tests.entries[0].expectedTasks = 0;
  }, "safe integer");
  expectInvalid((lock) => {
    lock.source.tests.entries[0].headPolicy =
      "native-head" as "harness-replaced";
  }, "harness-replaced");
  expectInvalid((lock) => {
    lock.source.tests.entries[0].supportPolicy =
      "load-support" as "locked-not-loaded";
  }, "locked-not-loaded");
  expectInvalid((lock) => {
    lock.source.tests.entries[0].manifest = "browser/not-locked/browser.toml";
  }, "manifest is not locked");
  expectInvalid((lock) => {
    lock.source.tests.manifests[0].supportPaths.push(
      "browser/not-locked/support.js",
    );
  }, "set mismatch");
  expectInvalid((lock) => {
    lock.source.tests.manifests[2].preferences[0].value =
      "true" as unknown as boolean;
  }, "expected a boolean");
  expectInvalid((lock) => {
    const migrationVersion = lock.source.tests.manifests[2].preferences.find(
      (preference) => preference.name === "browser.migration.version",
    );
    if (migrationVersion?.type !== "integer") {
      throw new Error("missing integer preference fixture");
    }
    migrationVersion.value = 2147483648;
  }, "signed 32-bit integer");
});

Deno.test("parseRuntimeLock rejects incomplete or unexpected platform tuples", () => {
  expectInvalid((lock) => {
    lock.artifacts.pop();
  }, "expected 4 entries");
  expectInvalid((lock) => {
    lock.artifacts[1] = structuredClone(lock.artifacts[0]);
  }, "sorted and unique");
  expectInvalid((lock) => {
    artifact(lock, "windows", "x86_64").architecture = "aarch64";
  }, "required platform tuple");
  expectInvalid((lock) => {
    artifact(lock, "windows", "x86_64").extractionPolicy = "tar-xz-floorp";
  }, "format/extraction policy");
  expectInvalid((lock) => {
    artifact(lock, "windows", "x86_64").format = "tar.xz";
  }, "format/extraction policy");
  expectInvalid((lock) => {
    artifact(lock, "windows", "x86_64").asset.name =
      "floorp-windows-x86_64-moz-artifact.tar.xz";
  }, "asset names");
});

Deno.test("parseRuntimeLock rejects malformed or duplicate release assets", () => {
  expectInvalid((lock) => {
    lock.artifacts[0].asset.id = lock.artifacts[1].asset.id;
  }, "asset IDs");
  expectInvalid((lock) => {
    lock.artifacts[0].applicationIniAsset.name =
      lock.artifacts[1].applicationIniAsset.name;
  }, "asset names");
  expectInvalid((lock) => {
    lock.artifacts[0].asset.size = 0;
  }, "safe integer");
  expectInvalid((lock) => {
    lock.artifacts[0].asset.sha256 = "ABC";
  }, "lowercase hexadecimal");
  expectInvalid((lock) => {
    lock.artifacts[0].version = "152.0";
  }, "three-component");
  expectInvalid((lock) => {
    lock.artifacts[0].version = "153.0.0";
  }, "share one version");
  expectInvalid((lock) => {
    lock.artifacts[0].buildId = "2026071807354";
  }, "14-digit");
  expectInvalid((lock) => {
    lock.artifacts[0].buildId = "20260230073540";
  }, "invalid UTC");
});
