// SPDX-License-Identifier: MPL-2.0

/**
 * Host tests for the FDH-prepared-runtime control receipt. They use temporary
 * trees only: no registry, FTP, network, browser or app launch.
 */

import { assertEquals, assertThrows } from "@std/assert";
import * as path from "@std/path";
import {
  assertPreparedRuntimeReceiptMatchesTree,
  buildPreparedRuntimeReceipt,
  DEBUG_RUNTIME_MARKER_FILE,
  normalizePreparedRuntimeSourcePath,
  PREPARED_RUNTIME_RECEIPT_FILE,
  readPreparedRuntimeReceipt,
  runtimePathsOverlap,
  writePreparedRuntimeReceipt,
} from "./prepared_runtime.ts";
import type { PreparedRuntimeMarkerIdentity } from "./prepared_runtime_types.ts";

function withTempDir(
  fn: (dir: string) => void | Promise<void>,
): Promise<void> {
  const dir = Deno.makeTempDirSync({ prefix: "fdh-prepared-runtime-" });
  return (async () => {
    try {
      await fn(dir);
    } finally {
      Deno.removeSync(dir, { recursive: true });
    }
  })();
}

function marker(
  overrides: Partial<PreparedRuntimeMarkerIdentity> = {},
): PreparedRuntimeMarkerIdentity {
  return {
    schemaVersion: 1,
    distribution: "debug",
    platform: "windows",
    architecture: "x86_64",
    artifactName: "floorp-windows-x86_64-moz-artifact.zip",
    artifactSize: 190337065,
    artifactSha256:
      "11fa924d1a9c427749efc74d8c594b46c5223c4e4049de7adec1ae72d27dbc0a",
    version: "160.0.1",
    buildId: "20261114215908",
    ...overrides,
  };
}

function writeRuntimeTree(
  binRootDir: string,
  value: PreparedRuntimeMarkerIdentity = marker(),
): void {
  Deno.mkdirSync(binRootDir, { recursive: true });
  Deno.writeTextFileSync(
    path.join(binRootDir, DEBUG_RUNTIME_MARKER_FILE),
    JSON.stringify(value),
  );
  const floorpRoot = path.join(binRootDir, "floorp");
  const appRoot = value.platform === "macos"
    ? path.join(floorpRoot, "Floorp.app", "Contents", "Resources")
    : floorpRoot;
  const modulesDir = path.join(appRoot, "modules");
  Deno.mkdirSync(modulesDir, { recursive: true });
  Deno.writeTextFileSync(
    path.join(modulesDir, "AppConstants.sys.mjs"),
    `export var AppConstants = Object.freeze({
  DEBUG: true,
  MOZ_APP_NAME: "floorp",
  MOZ_APP_VERSION: "${value.version}",
  MOZ_BUILDID: "${value.buildId}",
});
`,
  );
  Deno.writeTextFileSync(
    path.join(appRoot, "application.ini"),
    `[App]
Name=Floorp
Version=${value.version}
BuildID=${value.buildId}
`,
  );
  const executable = value.platform === "macos"
    ? path.join(floorpRoot, "Floorp.app", "Contents", "MacOS", "floorp")
    : value.platform === "windows"
    ? path.join(floorpRoot, "floorp.exe")
    : path.join(floorpRoot, "floorp-bin");
  Deno.mkdirSync(path.dirname(executable), { recursive: true });
  Deno.writeTextFileSync(executable, "binary");
}

Deno.test("prepared receipt validates against a matching live tree", () =>
  withTempDir((root) => {
    writeRuntimeTree(root);
    const receipt = buildPreparedRuntimeReceipt(
      marker(),
      root,
      "2026-11-15T00:00:00Z",
    );
    writePreparedRuntimeReceipt(root, receipt);
    const state = readPreparedRuntimeReceipt(root);
    assertEquals(state.status, "present");
    if (state.status === "present") {
      assertEquals(state.receipt.identity.buildId, "20261114215908");
      const identity = assertPreparedRuntimeReceiptMatchesTree(
        root,
        state.receipt,
        {
          platform: "windows",
          architecture: "x86_64",
        },
      );
      assertEquals(identity.version, "160.0.1");
      assertEquals(identity.buildId, "20261114215908");
    }
  }));

Deno.test("prepared receipt treats absence and malformed content differently", () =>
  withTempDir((root) => {
    assertEquals(readPreparedRuntimeReceipt(root).status, "absent");
    Deno.writeTextFileSync(
      path.join(root, PREPARED_RUNTIME_RECEIPT_FILE),
      "{not-json",
    );
    assertEquals(readPreparedRuntimeReceipt(root).status, "invalid");
  }));

Deno.test("prepared receipt fails closed on a non-Debug AppConstants", () =>
  withTempDir((root) => {
    writeRuntimeTree(root);
    Deno.writeTextFileSync(
      path.join(root, "floorp", "modules", "AppConstants.sys.mjs"),
      `export var AppConstants = Object.freeze({
  DEBUG: false,
  MOZ_APP_NAME: "floorp",
  MOZ_APP_VERSION: "160.0.1",
  MOZ_BUILDID: "20261114215908",
});
`,
    );
    const receipt = buildPreparedRuntimeReceipt(marker(), root);
    assertThrows(
      () =>
        assertPreparedRuntimeReceiptMatchesTree(root, receipt, {
          platform: "windows",
          architecture: "x86_64",
        }),
      "real Debug build",
    );
  }));

Deno.test("prepared receipt fails closed on marker identity mismatch", () =>
  withTempDir((root) => {
    writeRuntimeTree(root);
    const receipt = buildPreparedRuntimeReceipt(
      marker({ version: "158.0.1" }),
      root,
    );
    assertThrows(
      () =>
        assertPreparedRuntimeReceiptMatchesTree(root, receipt, {
          platform: "windows",
          architecture: "x86_64",
        }),
      "differs from the control receipt",
    );
  }));

Deno.test("prepared receipt fails closed when the executable is missing", () =>
  withTempDir((root) => {
    writeRuntimeTree(root);
    Deno.removeSync(path.join(root, "floorp", "floorp.exe"));
    const receipt = buildPreparedRuntimeReceipt(marker(), root);
    assertThrows(
      () =>
        assertPreparedRuntimeReceiptMatchesTree(root, receipt, {
          platform: "windows",
          architecture: "x86_64",
        }),
      "executable is missing or empty",
    );
  }));

Deno.test("prepared receipt validates Windows/Linux/macOS bundled layouts", () =>
  withTempDir((root) => {
    for (
      const [platform, architecture] of [
        ["windows", "x86_64"],
        ["linux", "x86_64"],
        ["macos", "universal"],
      ] as const
    ) {
      const binRoot = path.join(root, platform);
      writeRuntimeTree(binRoot, marker({ platform, architecture }));
      const receipt = buildPreparedRuntimeReceipt(
        marker({ platform, architecture }),
        binRoot,
      );
      const identity = assertPreparedRuntimeReceiptMatchesTree(
        binRoot,
        receipt,
        { platform, architecture },
      );
      assertEquals(identity.buildId, "20261114215908");
    }
  }));

Deno.test("prepared receipt rejects a comment-only DEBUG true in release", () =>
  withTempDir((root) => {
    writeRuntimeTree(root);
    Deno.writeTextFileSync(
      path.join(root, "floorp", "modules", "AppConstants.sys.mjs"),
      `// DEBUG: true in a comment is not the canonical property
export var AppConstants = Object.freeze({
  DEBUG: false,
  MOZ_APP_NAME: "floorp",
  MOZ_APP_VERSION: "160.0.1",
  MOZ_BUILDID: "20261114215908",
});
`,
    );
    const receipt = buildPreparedRuntimeReceipt(marker(), root);
    assertThrows(
      () =>
        assertPreparedRuntimeReceiptMatchesTree(root, receipt, {
          platform: "windows",
          architecture: "x86_64",
        }),
      "real Debug build",
    );
  }));

Deno.test("runtime overlap resolves an absent destination through an existing alias parent", () =>
  withTempDir((root) => {
    const source = path.join(root, "source");
    const alias = path.join(root, "alias");
    Deno.mkdirSync(source);
    Deno.writeTextFileSync(path.join(source, "sentinel"), "unchanged");
    Deno.symlinkSync(source, alias, {
      type: Deno.build.os === "windows" ? "junction" : "dir",
    });
    const destination = path.join(alias, "not-created", "bin");
    assertEquals(runtimePathsOverlap(source, destination), true);
    assertEquals(
      Deno.readTextFileSync(path.join(source, "sentinel")),
      "unchanged",
    );
  }));

Deno.test("runtime overlap recognizes a filesystem root as an ancestor", () =>
  withTempDir((root) => {
    assertEquals(runtimePathsOverlap(path.parse(root).root, root), true);
    assertEquals(runtimePathsOverlap(root, path.parse(root).root), true);
  }));

for (
  const [label, decoy] of [
    ["block comment", "/*\n  DEBUG: true\n*/\n"],
    ["template literal", "const note = `\n  DEBUG: true\n`;\n"],
    ["continued quoted string", 'const note = "note\\\n  DEBUG: true\\\n";\n'],
    ["nested property", "const unrelated = {\n  DEBUG: true\n};\n"],
  ]
) {
  Deno.test(`prepared receipt ignores a DEBUG decoy in ${label}`, () =>
    withTempDir((root) => {
      writeRuntimeTree(root);
      Deno.writeTextFileSync(
        path.join(root, "floorp", "modules", "AppConstants.sys.mjs"),
        decoy + `export var AppConstants = Object.freeze({
  DEBUG: false,
  MOZ_APP_NAME: "floorp",
  MOZ_APP_VERSION: "160.0.1",
  MOZ_BUILDID: "20261114215908",
});
`,
      );
      assertThrows(() =>
        assertPreparedRuntimeReceiptMatchesTree(
          root,
          buildPreparedRuntimeReceipt(marker(), root),
          { platform: "windows", architecture: "x86_64" },
        )
      );
    }));
}

Deno.test("prepared receipt accepts Debug with a false block-comment decoy", () =>
  withTempDir((root) => {
    writeRuntimeTree(root);
    const file = path.join(root, "floorp", "modules", "AppConstants.sys.mjs");
    Deno.writeTextFileSync(
      file,
      "/*\n  DEBUG: false\n*/\n" + Deno.readTextFileSync(file),
    );
    const identity = assertPreparedRuntimeReceiptMatchesTree(
      root,
      buildPreparedRuntimeReceipt(marker(), root),
      { platform: "windows", architecture: "x86_64" },
    );
    assertEquals(identity.version, "160.0.1");
  }));

Deno.test("prepared receipt rejects an unterminated AppConstants comment", () =>
  withTempDir((root) => {
    writeRuntimeTree(root);
    const file = path.join(root, "floorp", "modules", "AppConstants.sys.mjs");
    Deno.writeTextFileSync(file, Deno.readTextFileSync(file) + "\n/*");
    assertThrows(() =>
      assertPreparedRuntimeReceiptMatchesTree(
        root,
        buildPreparedRuntimeReceipt(marker(), root),
        { platform: "windows", architecture: "x86_64" },
      )
    );
  }));

Deno.test("prepared receipt normalizes the source intent path", () => {
  const absolute = path.join(Deno.cwd(), "registry", "bin");
  const normalized = normalizePreparedRuntimeSourcePath(absolute);
  assertEquals(normalized, normalizePreparedRuntimeSourcePath(normalized));
  if (Deno.build.os === "windows") {
    assertEquals(normalized, normalized.toLowerCase());
  }
});
