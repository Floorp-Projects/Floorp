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
  assertEquals(canonicalLock.source.ref, "daily-1029");
  assertEquals(
    canonicalLock.source.commit,
    "a34aa5dc40c4c36a5b608872ab7d557b54c3e1e1",
  );
  assertEquals(
    canonicalLock.source.tree,
    "6544e641efb98b9addfc5f72b8a5eb54a981c7fa",
  );
  assertEquals(canonicalLock.source.release, {
    id: 372407375,
    immutable: false,
  });
  assertEquals(canonicalLock.source.materials.count, 53);
  assertEquals(canonicalLock.source.materials.totalBytes, 221339);
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
      assetId: 519550922,
      assetSize: 73114556,
      assetSha:
        "bb34ccbd3807663b31fb1ac73fab020026a6c4858e047f1b9f6ea0b6ee47c0a5",
      iniId: 519550929,
      iniSize: 534,
      iniSha:
        "3a53633db7f3bff918034d26b440736a9b638220f9d7739eebc85cf53230a395",
      buildId: "20260818062758",
    },
    {
      tuple: "linux/x86_64",
      assetId: 519550920,
      assetSize: 84177852,
      assetSha:
        "1371dd852a32935feeb834cf41e570c5f4b10174f1bccafb3a7d8c0f13293328",
      iniId: 519550924,
      iniSize: 534,
      iniSha:
        "ab6ba7395b01664f6b4a2ce08d8a6dc4dc765e6ca0a1b46c4c64c5ba90381e8a",
      buildId: "20260818062758",
    },
    {
      tuple: "macos/universal",
      assetId: 519550931,
      assetSize: 182356546,
      assetSha:
        "70332eed562ae563c3231469db83b45972fefac7ed61386ac83202e2dbb36ceb",
      iniId: 519550923,
      iniSize: 534,
      iniSha:
        "be37e72e1eed0cb5df44658a46347d2ddf1e5c3f5f8d8ad0df682b46c4120341",
      buildId: "20260818062758",
    },
    {
      tuple: "windows/x86_64",
      assetId: 519550918,
      assetSize: 136081012,
      assetSha:
        "851b83faa43fc1732093e18ed81bed3665b6a1998f8e582e2e9ec6fac0e477f6",
      iniId: 519550926,
      iniSize: 534,
      iniSha:
        "0c30b164240148fb9452c2cc286be61d0d238cc9ba4efdce8ed0b082d44d88ec",
      buildId: "20260818062758",
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
    ["154.0", "154.0", "154.0", "154.0"],
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
