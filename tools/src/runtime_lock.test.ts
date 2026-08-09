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
  assertEquals(canonicalLock.source.ref, "daily-998");
  assertEquals(
    canonicalLock.source.commit,
    "ca3d0003976321fd67061d463ab56958b0f38cd9",
  );
  assertEquals(
    canonicalLock.source.tree,
    "a524742367f62767f607764cb5944ec3d613a77c",
  );
  assertEquals(canonicalLock.source.release, {
    id: 359773143,
    immutable: false,
  });
  assertEquals(canonicalLock.source.materials.count, 53);
  assertEquals(canonicalLock.source.materials.totalBytes, 220264);
  assertEquals(canonicalLock.source.tests.count, 8);
  assertEquals(canonicalLock.source.tests.totalTasks, 15);
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
      "browser/components/urlbar/tests/browser-UrlbarInput/browser_a11y.js": 1,
    },
  );
});

Deno.test("canonical Runtime lock pins each platform's own binary identity", () => {
  const expected = [
    {
      tuple: "linux/aarch64",
      assetId: 489491168,
      assetSize: 71744816,
      assetSha:
        "89506cb64f9b4b717d683ecd4e4623c529e67466b89c0b722d64c0a52d9b3df4",
      iniId: 489491163,
      iniSize: 535,
      iniSha:
        "64d53c8d58efe71ede5db1d8608c42b435eaf7b01e00aa03a1e1a517446e0bdc",
      buildId: "20260725075208",
    },
    {
      tuple: "linux/x86_64",
      assetId: 489491173,
      assetSize: 82741760,
      assetSha:
        "5bdfca2b437e884929bef19ebf1093598cabaf6fe70ee84bac8883e4c2a9392b",
      iniId: 489491164,
      iniSize: 535,
      iniSha:
        "fa6e0eb6867331244ae9de01859f28f77079bc44c2d96cdf7f9ec1790e09def4",
      buildId: "20260725075208",
    },
    {
      tuple: "macos/universal",
      assetId: 489491167,
      assetSize: 176824587,
      assetSha:
        "899f43aee0da04209bbddf5afab8473cad9ddc5785ce428c1feee5a96fbfa391",
      iniId: 489491175,
      iniSize: 535,
      iniSha:
        "3a943f943967fbe4af72e5781a41377ded486ec299a2e778bb654cc67d8c252c",
      buildId: "20260725075208",
    },
    {
      tuple: "windows/x86_64",
      assetId: 489491165,
      assetSize: 133135974,
      assetSha:
        "d5c3cbcb3c7c3f0852d36047f45fcf98c10fdb3b9160316b1cd242e9119bbe8c",
      iniId: 489491166,
      iniSize: 535,
      iniSha:
        "49a78a5484e97e2ce80b1341522da6e1616aa878cf163e2964cd3c7863b90ae4",
      buildId: "20260725075208",
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
    ["153.0", "153.0", "153.0", "153.0"],
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
