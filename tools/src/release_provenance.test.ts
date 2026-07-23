// SPDX-License-Identifier: MPL-2.0

import {
  assertEquals,
  assertRejects,
  assertStringIncludes,
  assertThrows,
} from "@std/assert";
import {
  assembleReleaseBundle,
  canonicalJson,
  computeManifestSetId,
  createNativeVerificationRecord,
  parseFullVersionOutput,
  ProvenanceError,
  TARGETS,
  validateNativeVerificationSet,
  validateReleaseBundle,
  validateRuntimeProvenance,
  validateUuidV7,
} from "./release_provenance.ts";

const RUNTIME_SHA = "b".repeat(40);
const FLOORP_SHA = "a".repeat(40);
const BUILD_ID = "20260723143615";
const RUN_ID = 1000;
const UUIDS = [
  "019f9000-0000-7000-8000-000000000001",
  "019f9000-0000-7000-8000-000000000002",
  "019f9000-0000-7000-8000-000000000003",
  "019f9000-0000-7000-8000-000000000004",
];

async function digest(
  algorithm: "SHA-256" | "SHA-512",
  bytes: Uint8Array,
): Promise<string> {
  const result = await crypto.subtle.digest(
    algorithm,
    bytes as Uint8Array<ArrayBuffer>,
  );
  return Array.from(
    new Uint8Array(result),
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("");
}

function runtimeManifest() {
  return {
    schema_version: 2,
    repository: "Floorp-Projects/Floorp-Runtime",
    head_sha: RUNTIME_SHA,
    workflow_run_id: RUN_ID,
    run_created_at: "2026-07-23T14:36:15Z",
    run_attempt: 1,
    expected_build_id: BUILD_ID,
    targets: TARGETS.map((target, index) => ({
      platform: target.runtimePlatform,
      arch: target.runtimeArch,
      artifact_name: target.runtimeArtifactName,
      artifact_id: 2000 + index,
      artifact_digest: `sha256:${String(index + 5).repeat(64)}`,
      expected_build_id: BUILD_ID,
    })),
  };
}

function restSnapshot() {
  const manifest = runtimeManifest();
  const artifact = (
    id: number,
    name: string,
    value: string,
  ) => ({
    id,
    name,
    digest: value,
    expired: false,
    workflow_run_id: RUN_ID,
    workflow_run_head_sha: RUNTIME_SHA,
  });
  return {
    schema_version: 2,
    repository: "Floorp-Projects/Floorp-Runtime",
    workflow_path: ".github/workflows/daily-build.yml",
    run: {
      id: RUN_ID,
      status: "completed",
      conclusion: "success",
      head_sha: RUNTIME_SHA,
      run_attempt: 1,
      created_at: "2026-07-23T14:36:15Z",
    },
    manifest_artifact: artifact(
      1999,
      "floorp-runtime-build-manifest-v2",
      `sha256:${"f".repeat(64)}`,
    ),
    target_artifacts: manifest.targets.map((target) =>
      artifact(target.artifact_id, target.artifact_name, target.artifact_digest)
    ),
  };
}

function normalizedRuntime() {
  return validateRuntimeProvenance(runtimeManifest(), restSnapshot());
}

function nativeRecord(
  targetKey: "windows" | "linux" | "linuxAarch64" | "mac",
  nativeArch: "x86_64" | "aarch64",
  packageId: number,
) {
  const targetIndex = TARGETS.findIndex((target) => target.key === targetKey);
  const target = runtimeManifest().targets[targetIndex];
  const packageName = {
    windows: "noraneko-windows-x86_64-installer",
    linux: "noraneko-linux-x86_64-installer",
    linuxAarch64: "noraneko-linux-aarch64-installer",
    mac: "noraneko-mac-universal-installer",
  }[targetKey];
  return {
    schema_version: 2,
    target_key: targetKey,
    native_arch: nativeArch,
    firefox_version: "153.0",
    app_build_id: BUILD_ID,
    platform_build_id: BUILD_ID,
    build_id2: UUIDS[targetIndex],
    runtime: {
      repository: "Floorp-Projects/Floorp-Runtime",
      head_sha: RUNTIME_SHA,
      workflow_run_id: RUN_ID,
      artifact_id: target.artifact_id,
      artifact_digest: target.artifact_digest,
      expected_build_id: BUILD_ID,
    },
    floorp: {
      repository: "Floorp-Projects/Floorp",
      head_sha: FLOORP_SHA,
      workflow_run_id: 3000,
    },
    floorp_package: {
      artifact_name: packageName,
      artifact_id: packageId,
      artifact_digest: `sha256:${String((packageId % 4) + 1).repeat(64)}`,
      unsigned: false,
    },
    verification: { status: "verified", method: "full-version" },
  };
}

function nativeRecords() {
  const macX64 = nativeRecord("mac", "x86_64", 3004);
  const macArm = structuredClone(macX64);
  macArm.native_arch = "aarch64";
  return [
    nativeRecord("windows", "x86_64", 3001),
    nativeRecord("linux", "x86_64", 3002),
    nativeRecord("linuxAarch64", "aarch64", 3003),
    macX64,
    macArm,
  ];
}

Deno.test("canonicalJson recursively sorts object keys and preserves arrays", () => {
  assertEquals(
    canonicalJson({ z: 1, a: [{ d: 4, c: 3 }, 2] }),
    '{"a":[{"c":3,"d":4},2],"z":1}',
  );
});

Deno.test("manifest identity is byte-compatible with the Floorp-Updates golden fixture", async () => {
  const buildIds = [
    "20260722010101",
    "20260722010202",
    "20260722010303",
    "20260722010404",
  ];
  const metadata = Object.fromEntries(
    await Promise.all(TARGETS.map(async (target, index) => {
      const marBytes = new TextEncoder().encode(`test MAR ${target.key}`);
      return [target.key, {
        schema_version: 2,
        version_display: "12.16.4@153.0",
        version: "153.0",
        noraneko_version: "12.16.4",
        buildid: buildIds[index],
        noraneko_buildid: UUIDS[index],
        channel: "release",
        platform: target.platform,
        arch: target.arch,
        manifest_set_id: `sha256:${"0".repeat(64)}`,
        mar: {
          url:
            `https://github.com/Floorp-Projects/Floorp/releases/download/v12.16.4/${target.marName}`,
          name: target.marName,
          size: marBytes.byteLength,
          sha512: await digest("SHA-512", marBytes),
        },
        provenance: {
          runtime_repository: "Floorp-Projects/Floorp-Runtime",
          runtime_head_sha: RUNTIME_SHA,
          runtime_run_id: 1000,
          runtime_artifact_id: 2000 + index,
          runtime_artifact_digest: `sha256:${String(index + 5).repeat(64)}`,
          floorp_repository: "Floorp-Projects/Floorp",
          floorp_head_sha: FLOORP_SHA,
          floorp_run_id: 3000,
          release_tag: "v12.16.4",
        },
        verification: {
          status: "verified",
          method: "full-version",
          app_build_id: buildIds[index],
          build_id2: UUIDS[index],
        },
      }];
    })),
  );
  assertEquals(
    await computeManifestSetId(
      metadata as Parameters<typeof computeManifestSetId>[0],
    ),
    "sha256:cfed918fc37125c130af2ddb1a0846cea7299ea368b3d027525caece9580657e",
  );
});

Deno.test("validates a completed successful Runtime run and exact immutable artifacts", () => {
  const result = normalizedRuntime();
  assertEquals(result.expected_build_id, BUILD_ID);
  assertEquals(result.targets.map((target) => target.artifact_id), [
    2000,
    2001,
    2002,
    2003,
  ]);
  assertEquals(result.manifest_artifact.id, 1999);
});

Deno.test("Runtime trust boundaries reject extra fields, mixing, stale artifacts, and bad BuildIDs", () => {
  const extra = restSnapshot() as Record<string, unknown>;
  extra.token = "must not be accepted";
  assertThrows(
    () => validateRuntimeProvenance(runtimeManifest(), extra),
    ProvenanceError,
    "unexpected: token",
  );

  const mixed = restSnapshot();
  mixed.target_artifacts[1].workflow_run_id = RUN_ID + 1;
  assertThrows(
    () => validateRuntimeProvenance(runtimeManifest(), mixed),
    ProvenanceError,
    "different workflow run",
  );

  const expired = restSnapshot();
  expired.target_artifacts[0].expired = true;
  assertThrows(
    () => validateRuntimeProvenance(runtimeManifest(), expired),
    ProvenanceError,
    "expired",
  );

  const badBuild = runtimeManifest();
  badBuild.expected_build_id = "20260230010101";
  assertThrows(
    () => validateRuntimeProvenance(badBuild, restSnapshot()),
    ProvenanceError,
    "valid UTC timestamp",
  );
});

Deno.test("full-version parser requires one adjacent pair and extracts Firefox version", () => {
  assertEquals(
    parseFullVersionOutput(`Floorp Floorp 153.0 ${BUILD_ID} ${BUILD_ID}\n`),
    {
      firefoxVersion: "153.0",
      appBuildId: BUILD_ID,
      platformBuildId: BUILD_ID,
    },
  );
  assertThrows(
    () =>
      parseFullVersionOutput(`Floorp 153.0 ${BUILD_ID}\nnoise\n${BUILD_ID}`),
    ProvenanceError,
    "adjacent",
  );
  assertThrows(
    () => parseFullVersionOutput(`Floorp stable ${BUILD_ID} ${BUILD_ID}`),
    ProvenanceError,
    "Firefox version",
  );
});

Deno.test("UUID validation only accepts lowercase RFC UUIDv7 shape", () => {
  assertEquals(validateUuidV7(UUIDS[0]), UUIDS[0]);
  assertThrows(() => validateUuidV7(UUIDS[0].toUpperCase()), ProvenanceError);
  assertThrows(
    () => validateUuidV7("019f9000-0000-6000-8000-000000000001"),
    ProvenanceError,
  );
});

Deno.test("record-native-verification binds native output to raw Runtime manifest", async () => {
  const root = await Deno.makeTempDir();
  try {
    const manifestPath = `${root}/runtime.json`;
    const outputPath = `${root}/full-version.txt`;
    const buildId2Path = `${root}/buildid2`;
    await Deno.writeTextFile(manifestPath, JSON.stringify(runtimeManifest()));
    await Deno.writeTextFile(
      outputPath,
      `Floorp Floorp 153.0 ${BUILD_ID} ${BUILD_ID}\n`,
    );
    await Deno.writeTextFile(buildId2Path, `${UUIDS[0]}\n`);
    const descriptor = {
      schema_version: 2,
      target_key: "windows",
      native_arch: "x86_64",
      runtime_manifest_path: manifestPath,
      full_version_output_path: outputPath,
      build_id2_path: buildId2Path,
      floorp: {
        repository: "Floorp-Projects/Floorp",
        head_sha: FLOORP_SHA,
        workflow_run_id: 3000,
      },
      floorp_package: {
        artifact_name: "noraneko-windows-x86_64-installer",
        artifact_id: 3001,
        artifact_digest: `sha256:${"1".repeat(64)}`,
        unsigned: false,
      },
    };
    const record = await createNativeVerificationRecord(descriptor);
    assertEquals(record.runtime.artifact_id, 2000);
    assertEquals(record.build_id2, UUIDS[0]);

    descriptor.floorp_package.artifact_name = "noraneko-linux-x86_64-installer";
    await assertRejects(
      () => createNativeVerificationRecord(descriptor),
      ProvenanceError,
      "artifact_name does not match target",
    );
    descriptor.floorp_package.artifact_name =
      "noraneko-windows-x86_64-installer";
    await Deno.writeTextFile(
      outputPath,
      "Floorp Floorp 153.0 20260723143614 20260723143614\n",
    );
    await assertRejects(
      () => createNativeVerificationRecord(descriptor),
      ProvenanceError,
      "does not match Runtime",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("five native records reject Runtime/package mixing and mac disagreement", () => {
  const records = nativeRecords();
  assertEquals(
    validateNativeVerificationSet(records, normalizedRuntime()).length,
    5,
  );

  const mixed = structuredClone(records);
  mixed[1].runtime.artifact_id = 9999;
  assertThrows(
    () => validateNativeVerificationSet(mixed, normalizedRuntime()),
    ProvenanceError,
    "mixed Runtime target",
  );

  const macMismatch = structuredClone(records);
  macMismatch[4].build_id2 = "019f9000-0000-7000-8000-000000000099";
  assertThrows(
    () => validateNativeVerificationSet(macMismatch, normalizedRuntime()),
    ProvenanceError,
    "mac native verification records disagree",
  );

  const packageMismatch = structuredClone(records);
  packageMismatch[4].floorp_package.artifact_digest = `sha256:${
    "9".repeat(64)
  }`;
  assertThrows(
    () => validateNativeVerificationSet(packageMismatch, normalizedRuntime()),
    ProvenanceError,
    "package identity",
  );
});

async function releaseFixture(root: string, unsigned = false) {
  await Deno.mkdir(root, { recursive: true });
  const bundleDir = `${root}/bundle`;
  await Deno.mkdir(bundleDir, { recursive: true });
  const runtimePath = `${root}/normalized-runtime.json`;
  await Deno.writeTextFile(runtimePath, JSON.stringify(normalizedRuntime()));
  const records = nativeRecords();
  if (unsigned) {
    for (const record of records) {
      record.floorp_package.artifact_name += "-unsigned";
      record.floorp_package.unsigned = true;
    }
  }
  const recordPaths: string[] = [];
  for (let index = 0; index < records.length; index++) {
    const path = `${root}/record-${index}.json`;
    await Deno.writeTextFile(path, JSON.stringify(records[index]));
    recordPaths.push(path);
  }

  const mars: Record<string, unknown> = {};
  for (const target of TARGETS) {
    const path = `${bundleDir}/${target.marName}`;
    const bytes = new TextEncoder().encode(`real MAR ${target.key}`);
    await Deno.writeFile(path, bytes);
    mars[target.key] = {
      path,
      url:
        `https://github.com/Floorp-Projects/Floorp/releases/download/v12.16.4/${target.marName}`,
      size: bytes.byteLength,
      sha512: await digest("SHA-512", bytes),
    };
  }
  const releaseNames = {
    windows: "floorp-windows-x86_64.installer.exe",
    windowsStub: "floorp-stub.installer.exe",
    linux: "floorp-linux-x86_64.tar.xz",
    linuxAarch64: "floorp-linux-aarch64.tar.xz",
    mac: "floorp-macOS-universal.dmg",
    linuxDeb: "floorp-12.16.4.deb",
  };
  const releaseFiles: Record<string, unknown> = {};
  for (const [key, name] of Object.entries(releaseNames)) {
    const path = `${bundleDir}/${name}`;
    const bytes = new TextEncoder().encode(`release file ${key}`);
    await Deno.writeFile(path, bytes);
    releaseFiles[key] = {
      path,
      name,
      size: bytes.byteLength,
      sha512: await digest("SHA-512", bytes),
    };
  }
  const stubBytes = await Deno.readFile(
    `${bundleDir}/${releaseNames.windowsStub}`,
  );
  return {
    bundleDir,
    descriptor: {
      schema_version: 2,
      mode: "production",
      floorp_version: "12.16.4",
      release_tag: "v12.16.4",
      runtime_provenance_path: runtimePath,
      native_verification_paths: recordPaths,
      mars,
      release_files: releaseFiles,
      stub_source: {
        repository: "Floorp-Projects/Floorp",
        release_tag: "v12.16.3",
        release_id: 4000,
        asset_id: 4001,
        asset_name: releaseNames.windowsStub,
        asset_digest: `sha256:${await digest("SHA-256", stubBytes)}`,
        size: stubBytes.byteLength,
      },
    },
  };
}

Deno.test("assembles and revalidates four Updates-v2 metadata files from real files", async () => {
  const root = await Deno.makeTempDir();
  try {
    const fixture = await releaseFixture(root);
    const assembled = await assembleReleaseBundle(
      fixture.descriptor,
      fixture.bundleDir,
    );
    assertEquals(assembled.manifest.version_display, "12.16.4@153.0");
    assertEquals(assembled.metadata.windows.version_display, "12.16.4@153.0");
    assertEquals(
      assembled.metadata.windows.mar.name,
      "floorp-windows-x86_64-full.mar",
    );
    assertEquals(
      assembled.manifest.release_files.windowsStub.name,
      "floorp-stub.installer.exe",
    );
    assertEquals(assembled.manifest.stub_source.release_tag, "v12.16.3");
    assertEquals(
      [...Deno.readDirSync(fixture.bundleDir)].length,
      16,
    );
    assertEquals(
      (await validateReleaseBundle(fixture.bundleDir, true)).manifest_set_id,
      assembled.manifest.manifest_set_id,
    );

    const windowsMar =
      (fixture.descriptor.mars as Record<string, { path: string }>).windows
        .path;
    await Deno.writeTextFile(windowsMar, "changed MAR");
    await assertRejects(
      () => validateReleaseBundle(fixture.bundleDir, true),
      ProvenanceError,
      "does not match release manifest",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("stub source is strict and binds the inherited asset bytes", async () => {
  const root = await Deno.makeTempDir();
  try {
    const badDigest = await releaseFixture(`${root}/bad-digest`);
    badDigest.descriptor.stub_source.asset_digest = `sha256:${"0".repeat(64)}`;
    await assertRejects(
      () =>
        assembleReleaseBundle(
          badDigest.descriptor,
          badDigest.bundleDir,
        ),
      ProvenanceError,
      "stub_source.asset_digest does not match the real stub file",
    );

    const fixture = await releaseFixture(`${root}/strict-manifest`);
    await assembleReleaseBundle(fixture.descriptor, fixture.bundleDir);
    const manifestPath = `${fixture.bundleDir}/release-manifest-set-v2.json`;
    const manifestText = await Deno.readTextFile(manifestPath);
    const manifest = JSON.parse(manifestText);
    manifest.stub_source.unexpected = true;
    await Deno.writeTextFile(
      manifestPath,
      `${JSON.stringify(manifest, null, 2)}\n`,
    );
    await assertRejects(
      () => validateReleaseBundle(fixture.bundleDir, true),
      ProvenanceError,
      "release manifest.stub_source has an invalid schema",
    );

    await Deno.writeTextFile(manifestPath, manifestText);
    await Deno.writeTextFile(
      `${fixture.bundleDir}/floorp-stub.installer.exe`,
      "tampered inherited stub",
    );
    await assertRejects(
      () => validateReleaseBundle(fixture.bundleDir, true),
      ProvenanceError,
      "floorp-stub.installer.exe does not match release manifest",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("production rejects unsigned verification and descriptor schema extras", async () => {
  const root = await Deno.makeTempDir();
  try {
    const unsigned = await releaseFixture(root, true);
    await assertRejects(
      () => assembleReleaseBundle(unsigned.descriptor, unsigned.bundleDir),
      ProvenanceError,
      "production release assembly rejects unsigned",
    );
    const second = await releaseFixture(`${root}/second`);
    const extra = { ...second.descriptor, secret: "no" };
    await assertRejects(
      () => assembleReleaseBundle(extra, second.bundleDir),
      ProvenanceError,
      "unexpected: secret",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("validation mode accepts consistently marked unsigned artifacts", async () => {
  const root = await Deno.makeTempDir();
  try {
    const fixture = await releaseFixture(root, true);
    fixture.descriptor.mode = "validation";
    const bundle = await assembleReleaseBundle(
      fixture.descriptor,
      fixture.bundleDir,
    );
    assertEquals(bundle.manifest.mode, "validation");
    assertEquals(
      bundle.manifest.targets.every((target) => target.floorp_package.unsigned),
      true,
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("bundle validation rejects metadata tampering", async () => {
  const root = await Deno.makeTempDir();
  try {
    const fixture = await releaseFixture(root);
    await assembleReleaseBundle(fixture.descriptor, fixture.bundleDir);
    const path = `${fixture.bundleDir}/win-meta.json`;
    const text = await Deno.readTextFile(path);
    await Deno.writeTextFile(
      path,
      text.replace("12.16.4@153.0", "153.0@12.16.4"),
    );
    await assertRejects(
      () => validateReleaseBundle(fixture.bundleDir, true),
      ProvenanceError,
      "not the exact metadata",
    );
    await Deno.writeTextFile(path, text);
    const hashesPath = `${fixture.bundleDir}/hashes.txt`;
    const hashes = await Deno.readTextFile(hashesPath);
    await Deno.writeTextFile(hashesPath, `${hashes}0  injected\n`);
    await assertRejects(
      () => validateReleaseBundle(fixture.bundleDir, true),
      ProvenanceError,
      "hashes.txt does not match",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("errors remain diagnostic without exposing descriptor contents", () => {
  try {
    validateRuntimeProvenance({}, {});
  } catch (error) {
    assertStringIncludes((error as Error).message, "missing");
    assertEquals((error as Error).message.includes("token"), false);
  }
});

Deno.test("workflow artifact handoffs normalize upload digests to REST form", async () => {
  const workflow = async (name: string) =>
    await Deno.readTextFile(
      new URL(`../../.github/workflows/${name}`, import.meta.url),
    );
  const packageWorkflow = await workflow("package.yml");
  const preflightWorkflow = await workflow("validate-runtime-provenance.yml");
  const verifierWorkflow = await workflow("verify-release-artifact.yml");
  const assemblerWorkflow = await workflow("assemble-release-bundle.yml");

  for (
    const expected of [
      "format('sha256:{0}', steps.upload-installer.outputs.artifact-digest)",
      "format('sha256:{0}', steps.upload-mar.outputs.artifact-digest)",
      "format('sha256:{0}', steps.upload-deb.outputs.artifact-digest)",
    ]
  ) {
    assertStringIncludes(packageWorkflow, expected);
  }
  assertStringIncludes(
    preflightWorkflow,
    "format('sha256:{0}', steps.upload-provenance.outputs.artifact-digest)",
  );
  assertStringIncludes(
    verifierWorkflow,
    "format('sha256:{0}', steps.upload-record.outputs.artifact-digest)",
  );
  assertStringIncludes(
    assemblerWorkflow,
    "format('sha256:{0}', steps.upload.outputs.artifact-digest)",
  );
});

Deno.test("legacy stub updater refuses to mutate provenance-v2 releases", async () => {
  const workflow = await Deno.readTextFile(
    new URL("../../.github/workflows/build_installers.yml", import.meta.url),
  );
  const manifestGuard = workflow.indexOf("release-manifest-set-v2.json");
  const refusal = workflow.indexOf("Refusing to mutate provenance-v2 release");
  const deleteStep = workflow.indexOf(
    "- name: Delete existing stub asset (if any)",
  );
  const uploadStep = workflow.indexOf(
    "- name: Upload signed installer to latest release",
  );
  assertEquals(
    manifestGuard >= 0 &&
      refusal > manifestGuard &&
      deleteStep > refusal &&
      uploadStep > deleteStep,
    true,
  );
});

Deno.test("production package entrypoint enforces Floorp and Runtime default refs", async () => {
  const packageWorkflow = await Deno.readTextFile(
    new URL("../../.github/workflows/package.yml", import.meta.url),
  );

  for (
    const expected of [
      "CURRENT_FLOORP_REF: ${{ github.ref }}",
      "FLOORP_DEFAULT_BRANCH: ${{ github.event.repository.default_branch }}",
      'const productionV2 = mode === "stable-v2"',
      'floorpRepository !== "Floorp-Projects/Floorp"',
      "process.env.CURRENT_FLOORP_REF !== expectedFloorpRef",
      "await github.rest.repos.get",
      "run.head_branch !== runtimeDefaultBranch",
      '"workflow_dispatch"',
      '"schedule"',
    ]
  ) {
    assertStringIncludes(packageWorkflow, expected);
  }
  assertEquals(
    packageWorkflow.match(/if \(productionV2\) \{/g)?.length,
    2,
  );
});

Deno.test("exact Runtime artifact download uses the flat consolidated layout", async () => {
  const packageWorkflow = await Deno.readTextFile(
    new URL("../../.github/workflows/package.yml", import.meta.url),
  );
  const start = packageWorkflow.indexOf(
    "- name: Download verified Runtime artifact by ID",
  );
  const end = packageWorkflow.indexOf(
    "- name: Stage exact Runtime v2 bundle",
    start,
  );
  assertEquals(start >= 0 && end > start, true);
  const downloadStep = packageWorkflow.slice(start, end);
  assertStringIncludes(downloadStep, "artifact-ids:");
  assertStringIncludes(downloadStep, "merge-multiple: true");
});

Deno.test("release publication is serialized across source workflow runs", async () => {
  const publishWorkflow = await Deno.readTextFile(
    new URL("../../.github/workflows/publish_release.yml", import.meta.url),
  );
  assertStringIncludes(
    publishWorkflow,
    "group: publish-release-v2-${{ github.repository }}",
  );
  assertEquals(
    publishWorkflow.includes(
      "group: publish-release-v2-${{ inputs.source_workflow_run_id }}",
    ),
    false,
  );
});
