// SPDX-License-Identifier: MPL-2.0

import type {
  RuntimeLock,
  RuntimeMaterial,
  RuntimeTest,
  RuntimeTestManifest,
} from "../src/runtime_lock.ts";

interface TestRuntimeLockOptions {
  commit?: string;
  tree?: string;
  materials?: RuntimeMaterial[];
  tests?: RuntimeTest[];
  manifests?: RuntimeTestManifest[];
}

export function createTestRuntimeLock(
  options: TestRuntimeLockOptions = {},
): RuntimeLock {
  const materials = options.materials ?? [];
  const tests = options.tests ?? [];
  const manifests = options.manifests ?? [];
  const testsPerManifest = new Map<string, number>();
  for (const test of tests) {
    testsPerManifest.set(
      test.manifest,
      (testsPerManifest.get(test.manifest) ?? 0) + 1,
    );
  }
  return {
    schemaVersion: 1,
    source: {
      repository: "Floorp-Projects/Floorp-Runtime",
      trackingRef: "fixture-ref",
      ref: "fixture-lock-ref",
      commit: options.commit ?? "0".repeat(40),
      tree: options.tree ?? "1".repeat(40),
      release: { id: 1, immutable: true },
      materials: {
        count: materials.length,
        totalBytes: materials.reduce(
          (sum, material) => sum + material.bytes,
          0,
        ),
        entries: materials,
      },
      tests: {
        count: tests.length,
        totalTasks: tests.reduce((sum, test) => sum + test.expectedTasks, 0),
        supportDependencyEdges: manifests.reduce(
          (sum, manifest) =>
            sum +
            manifest.supportPaths.length *
              (testsPerManifest.get(manifest.path) ?? 0),
          0,
        ),
        entries: tests,
        manifests,
      },
    },
    artifacts: [],
  };
}
