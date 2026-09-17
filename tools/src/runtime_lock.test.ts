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

Deno.test("Runtime browser lock rejects non-browser harness tests", () => {
  expectInvalid(
    (lock) => {
      lock.source.tests.entries[0].path =
        "services/sync/tests/unit/test_floorp_notes_prefs.js";
    },
    "browser-chrome test path",
  );
  expectInvalid(
    (lock) => {
      lock.source.tests.manifests[0].path =
        "services/sync/tests/unit/xpcshell-floorp-notes.toml";
    },
    "browser-chrome manifest",
  );
  expectInvalid(
    (lock) => {
      lock.source.tests.entries[0].path =
        "browser/base/content/test/caps/test_not_browser_chrome.js";
    },
    "browser-chrome test path",
  );
  expectInvalid(
    (lock) => {
      lock.source.tests.manifests[0].path =
        "browser/base/content/test/caps/xpcshell.toml";
    },
    "browser-chrome manifest",
  );
});

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
  assertEquals(canonicalLock.source.ref, "daily-1094");
  assertEquals(
    canonicalLock.source.commit,
    "04556dbea6f1c761b36dfe7f973b8c538aebacfd",
  );
  assertEquals(
    canonicalLock.source.tree,
    "7dac8a01fc750e8fff01597a91a0ad8d8b2446cf",
  );
  assertEquals(canonicalLock.source.release, {
    id: 390214516,
    immutable: false,
  });
  assertEquals(canonicalLock.source.materials.count, 53);
  assertEquals(canonicalLock.source.materials.totalBytes, 224465);
  assertEquals(canonicalLock.source.tests.count, 8);
  assertEquals(canonicalLock.source.tests.totalTasks, 16);
  assertEquals(canonicalLock.source.tests.supportDependencyEdges, 93);

  const roleCounts = Object.groupBy(
    canonicalLock.source.materials.entries,
    (entry) => entry.role,
  );
  assertEquals(roleCounts.test?.length, 8);
  assertEquals(roleCounts.manifest?.length, 6);
  assertEquals(roleCounts["head-support"]?.length, 5);
  assertEquals(roleCounts.support?.length, 34);

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
      "browser/components/urlbar/tests/browser-UrlbarInput/browser_a11y.js": 2,
    },
  );
});

Deno.test("canonical Runtime lock pins each platform's own binary identity", () => {
  const expected = [
    {
      tuple: "linux/aarch64",
      assetId: 568654414,
      assetSize: 73547404,
      assetSha:
        "f147f9cc2d84e6ef8ddfcb2a659c87a91e3b8781da74e986e81117bd3d66d0fe",
      iniId: 568654418,
      iniSize: 537,
      iniSha:
        "6a18e737c1306accdeb4c678922f31f0f1b132a599dbd63007a7b70affcf9c49",
      buildId: "20260916130640",
    },
    {
      tuple: "linux/x86_64",
      assetId: 568654407,
      assetSize: 84940644,
      assetSha:
        "8a16afa8450da0aefee7368fa6a974c2210667866cb51a2a3c5d0565310b0f4d",
      iniId: 568654405,
      iniSize: 537,
      iniSha:
        "2ec1903db281970f05eddf5e100e61822cd8255c99ff391c2da9605eca1075cf",
      buildId: "20260916130640",
    },
    {
      tuple: "macos/universal",
      assetId: 568654417,
      assetSize: 183435450,
      assetSha:
        "90ea75dafd8da9939b104ea9f3910d8c43450fb48089d7910652b69e6d37aeb9",
      iniId: 568654416,
      iniSize: 537,
      iniSha:
        "cd77d93205dad51ad84e53e785cf4616988f5a316fee91d3630dc055d238054a",
      buildId: "20260916130640",
    },
    {
      tuple: "windows/x86_64",
      assetId: 568654411,
      assetSize: 137315282,
      assetSha:
        "a19f0d011838f84cfce148515cd42df16f5f54f3614cd3f009235cabfcb75979",
      iniId: 568654419,
      iniSize: 537,
      iniSha:
        "f7a1b720e0388aadb52f7fa5a01cc637ac4cf8dadc47d086fded2191a6becc7d",
      buildId: "20260916130640",
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
    ["156.0.1", "156.0.1", "156.0.1", "156.0.1"],
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
  }, `expected ${canonicalLock.source.materials.entries.length}`);
  expectInvalid((lock) => {
    lock.source.materials.totalBytes += 1;
  }, `expected ${canonicalLock.source.materials.totalBytes}`);
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
  }, `expected ${canonicalLock.source.tests.entries.length}`);
  expectInvalid((lock) => {
    lock.source.tests.totalTasks += 1;
  }, `expected ${canonicalLock.source.tests.totalTasks}`);
  expectInvalid((lock) => {
    lock.source.tests.supportDependencyEdges -= 1;
  }, `expected ${canonicalLock.source.tests.supportDependencyEdges}`);
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
  for (
    const invalidVersion of [
      "153",
      "153.0.0.0",
      "0153.0",
      "153.00",
      "153.0a1",
      "153.0 ",
    ]
  ) {
    expectInvalid((lock) => {
      lock.artifacts[0].version = invalidVersion;
    }, "canonical two- or three-component");
  }
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

Deno.test(
  "parseRuntimeLock preserves canonical two- and three-component versions",
  () => {
    for (const expectedVersion of ["153.0", "152.0.7"]) {
      const lock = cloneLock();
      for (const entry of lock.artifacts) {
        entry.version = expectedVersion;
      }
      const parsed = parseRuntimeLock(lock);
      assertEquals(
        parsed.artifacts.map((entry) => entry.version),
        Array(4).fill(expectedVersion),
      );
    }
  },
);
