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
        "browser/base/content/test/browser-caps/test_not_browser_chrome.js";
    },
    "browser-chrome test path",
  );
  expectInvalid(
    (lock) => {
      lock.source.tests.manifests[0].path =
        "browser/base/content/test/browser-caps/xpcshell.toml";
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
  const source = canonicalLock.source;
  assertEquals({
    repository: source.repository,
    trackingRef: source.trackingRef,
    ref: source.ref,
    commit: source.commit,
    tree: source.tree,
    release: source.release,
    materialsCount: source.materials.count,
    materialBytes: source.materials.totalBytes,
    testsCount: source.tests.count,
    totalTasks: source.tests.totalTasks,
    supportDependencyEdges: source.tests.supportDependencyEdges,
    roleCounts: Object.fromEntries(
      ["manifest", "test", "head-support", "support"].map(
        (role) => [
          role,
          source.materials.entries.filter((row) => row.role === role).length,
        ],
      ),
    ),
    expectedTasks: Object.fromEntries(
      source.tests.entries.map((row) => [row.path, row.expectedTasks]),
    ),
  }, {
    "repository": "Floorp-Projects/Floorp-Runtime",
    "trackingRef": "nora-0.2.0",
    "ref": "daily-1180",
    "commit": "741c4537e89b550b0297ed7ea493026506930d4d",
    "tree": "37acac42c286b043f83c0f7d76cbaf340c27c8d6",
    "release": {
      "id": 443518288,
      "immutable": false,
    },
    "materialsCount": 54,
    "materialBytes": 220178,
    "testsCount": 9,
    "totalTasks": 17,
    "supportDependencyEdges": 100,
    "roleCounts": {
      "manifest": 6,
      "test": 9,
      "head-support": 5,
      "support": 34,
    },
    "expectedTasks": {
      "browser/base/content/test/browser-caps/browser_principalSerialization_version1.js":
        2,
      "browser/base/content/test/browser-general/browser_bug537474.js": 1,
      "browser/base/content/test/browser-general/browser_bug565575.js": 1,
      "browser/base/content/test/browser-general/browser_bug596687.js": 1,
      "browser/base/content/test/browser-performance/browser_hiddenwindow_existence.js":
        1,
      "browser/components/customizableui/test/browser_996364_registerArea_different_properties.js":
        7,
      "browser/components/tabbrowser/test/browser/tabs/browser_open_newtab_start_observer_notification.js":
        1,
      "browser/components/tabbrowser/test/browser/tabs/browser_pinned_and_hidden_tabs.js":
        1,
      "browser/components/urlbar/tests/browser-UrlbarInput/browser_a11y.js": 2,
    },
  });
});

Deno.test("canonical Runtime lock pins each platform's own binary identity", () => {
  const expected = [
    {
      "tuple": "linux/aarch64",
      "assetId": 741113423,
      "assetSize": 79762680,
      "assetSha":
        "a5000b78ced031b655e035e0373c630bc6f9da4ad2adb217a900d62a7e84ef20",
      "iniId": 741113424,
      "iniSize": 535,
      "iniSha":
        "ee4e299cf15bd3166b516492c22b9461a4b850d023a6975dffa12609a56e9f2e",
      "buildId": "20261112140843",
    },
    {
      "tuple": "linux/x86_64",
      "assetId": 741113420,
      "assetSize": 87953056,
      "assetSha":
        "556e7bc6871ed7af2dc3cdc5d0c1d57b442ce6a596a32c55a98f139787661751",
      "iniId": 741113426,
      "iniSize": 535,
      "iniSha":
        "59540708180df5ea4d32966dd1995ffa58a52bcb9b8df27eb8b67185925a9f72",
      "buildId": "20261112140843",
    },
    {
      "tuple": "macos/universal",
      "assetId": 741113432,
      "assetSize": 199074582,
      "assetSha":
        "1999940772ef5899f01e926543e6d0b41bc0cdeccbaca9d471ae128ce9556158",
      "iniId": 741113430,
      "iniSize": 535,
      "iniSha":
        "02de57989f615d77b37aa65006f26661240ce3bf0828d0937d2aa2cdd8a4e11d",
      "buildId": "20261112140843",
    },
    {
      "tuple": "windows/x86_64",
      "assetId": 741113422,
      "assetSize": 142118367,
      "assetSha":
        "ea676e956136b8a1a2ff189294e028a3d6477a6b8c3e255250da139506e5523e",
      "iniId": 741113427,
      "iniSize": 535,
      "iniSha":
        "edc5efc3e92ecac38167635371d59049d41e94d0df7b9234512600f8881bfd6e",
      "buildId": "20261112140843",
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
    ["160.0.1", "160.0.1", "160.0.1", "160.0.1"],
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
      "browser/base/content/test/browser-caps/browser.toml",
    ).gitBlob = "g".repeat(40);
  }, "lowercase hexadecimal");
  expectInvalid((lock) => {
    material(
      lock,
      "browser/base/content/test/browser-caps/browser.toml",
    ).sha256 = "0".repeat(63);
  }, "lowercase hexadecimal");
  expectInvalid((lock) => {
    material(
      lock,
      "browser/base/content/test/browser-caps/browser.toml",
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
      "browser/base/content/test/browser-caps/browser_principalSerialization_version1.js",
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
