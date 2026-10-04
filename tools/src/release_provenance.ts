// SPDX-License-Identifier: MPL-2.0
//
// Strict provenance contracts for Floorp stable release artifacts.  Every JSON
// object accepted at a trust boundary is closed: missing and additional fields
// are rejected before any value is used.

const SHA1_RE = /^[0-9a-f]{40}$/;
const SHA256_RE = /^sha256:[0-9a-f]{64}$/;
const SHA512_RE = /^[0-9a-f]{128}$/;
const BUILD_ID_RE = /^\d{14}$/;
const UUID_V7_RE =
  /^[0-9a-f]{8}-[0-9a-f]{4}-7[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/;
const FIREFOX_VERSION_RE =
  /^(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)(?:\.(?:0|[1-9]\d*))?$/;
const SEMVER_RE =
  /^(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)(?:-(?:0|[1-9]\d*|\d*[A-Za-z-][0-9A-Za-z-]*)(?:\.(?:0|[1-9]\d*|\d*[A-Za-z-][0-9A-Za-z-]*))*)?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$/;
const UTC_TIMESTAMP_RE = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,6})?Z$/;
const FLOORP_REPOSITORY = "Floorp-Projects/Floorp";
const STUB_ASSET_NAME = "floorp-stub.installer.exe";

export class ProvenanceError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "ProvenanceError";
  }
}

function fail(message: string): never {
  throw new ProvenanceError(message);
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function exactObject(
  value: unknown,
  keys: readonly string[],
  path: string,
): Record<string, unknown> {
  if (!isRecord(value)) fail(`${path} must be an object`);
  const actual = Object.keys(value).sort();
  const expected = [...keys].sort();
  const missing = expected.filter((key) => !actual.includes(key));
  const extra = actual.filter((key) => !expected.includes(key));
  if (missing.length || extra.length) {
    fail(
      `${path} has an invalid schema` +
        (missing.length ? `; missing: ${missing.join(", ")}` : "") +
        (extra.length ? `; unexpected: ${extra.join(", ")}` : ""),
    );
  }
  return value;
}

function arrayValue(value: unknown, path: string): unknown[] {
  if (!Array.isArray(value)) fail(`${path} must be an array`);
  return value;
}

function stringValue(value: unknown, path: string): string {
  if (typeof value !== "string" || value.length === 0) {
    fail(`${path} must be a non-empty string`);
  }
  return value;
}

function booleanValue(value: unknown, path: string): boolean {
  if (typeof value !== "boolean") fail(`${path} must be a boolean`);
  return value;
}

function positiveInteger(value: unknown, path: string): number {
  if (!Number.isSafeInteger(value) || (value as number) <= 0) {
    fail(`${path} must be a positive safe integer`);
  }
  return value as number;
}

function expectLiteral<T extends string | number | boolean>(
  value: unknown,
  expected: T,
  path: string,
): T {
  if (value !== expected) {
    fail(
      `${path} must equal ${JSON.stringify(expected)}; got ${
        JSON.stringify(value)
      }`,
    );
  }
  return expected;
}

function expectPattern(value: string, pattern: RegExp, path: string): string {
  if (!pattern.test(value)) fail(`${path} has an invalid format`);
  return value;
}

function validateBuildId(value: unknown, path: string): string {
  const result = expectPattern(stringValue(value, path), BUILD_ID_RE, path);
  const year = Number(result.slice(0, 4));
  const month = Number(result.slice(4, 6));
  const day = Number(result.slice(6, 8));
  const hour = Number(result.slice(8, 10));
  const minute = Number(result.slice(10, 12));
  const second = Number(result.slice(12, 14));
  const date = new Date(Date.UTC(year, month - 1, day, hour, minute, second));
  const roundTrip = [
    date.getUTCFullYear().toString().padStart(4, "0"),
    (date.getUTCMonth() + 1).toString().padStart(2, "0"),
    date.getUTCDate().toString().padStart(2, "0"),
    date.getUTCHours().toString().padStart(2, "0"),
    date.getUTCMinutes().toString().padStart(2, "0"),
    date.getUTCSeconds().toString().padStart(2, "0"),
  ].join("");
  if (roundTrip !== result) fail(`${path} is not a valid UTC timestamp`);
  return result;
}

function buildIdFromCreatedAt(value: unknown, path: string): string {
  const timestamp = expectPattern(
    stringValue(value, path),
    UTC_TIMESTAMP_RE,
    path,
  );
  const date = new Date(timestamp);
  if (Number.isNaN(date.valueOf())) {
    fail(`${path} is not a valid UTC timestamp`);
  }
  const canonicalPrefix = [
    date.getUTCFullYear().toString().padStart(4, "0"),
    (date.getUTCMonth() + 1).toString().padStart(2, "0"),
    date.getUTCDate().toString().padStart(2, "0"),
    date.getUTCHours().toString().padStart(2, "0"),
    date.getUTCMinutes().toString().padStart(2, "0"),
    date.getUTCSeconds().toString().padStart(2, "0"),
  ].join("");
  validateBuildId(canonicalPrefix, `${path} derived build ID`);
  return canonicalPrefix;
}

function validateSha1(value: unknown, path: string): string {
  return expectPattern(stringValue(value, path), SHA1_RE, path);
}

function validateSha256(value: unknown, path: string): string {
  return expectPattern(stringValue(value, path), SHA256_RE, path);
}

function validateSha512(value: unknown, path: string): string {
  return expectPattern(stringValue(value, path), SHA512_RE, path);
}

function validateFirefoxVersion(value: unknown, path: string): string {
  return expectPattern(stringValue(value, path), FIREFOX_VERSION_RE, path);
}

function validateSemver(value: unknown, path: string): string {
  return expectPattern(stringValue(value, path), SEMVER_RE, path);
}

export function validateUuidV7(
  value: unknown,
  path = "UUIDv7",
): string {
  return expectPattern(stringValue(value, path), UUID_V7_RE, path);
}

/** Matches the canonical JSON implementation in Floorp-Updates byte-for-byte. */
export function canonicalJson(value: unknown): string {
  if (Array.isArray(value)) return `[${value.map(canonicalJson).join(",")}]`;
  if (isRecord(value)) {
    return "{" + Object.keys(value).sort().map((key) =>
      JSON.stringify(key) + ":" + canonicalJson(value[key])
    ).join(",") + "}";
  }
  const encoded = JSON.stringify(value);
  if (encoded === undefined) fail("canonicalJson only accepts JSON values");
  return encoded;
}

async function digestBytes(
  algorithm: "SHA-256" | "SHA-512",
  bytes: Uint8Array,
): Promise<string> {
  const digest = await crypto.subtle.digest(
    algorithm,
    bytes as Uint8Array<ArrayBuffer>,
  );
  return Array.from(
    new Uint8Array(digest),
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("");
}

async function hashFile(
  path: string,
  algorithm: "sha256" | "sha512",
): Promise<{ size: number; digest: string }> {
  let bytes: Uint8Array;
  try {
    bytes = await Deno.readFile(path);
  } catch (error) {
    fail(`could not hash ${path}: ${(error as Error).message}`);
  }
  if (bytes.byteLength <= 0) fail(`${path} must be a non-empty file`);
  return {
    size: bytes.byteLength,
    digest: await digestBytes(
      algorithm === "sha256" ? "SHA-256" : "SHA-512",
      bytes,
    ),
  };
}

function pathBasename(path: string): string {
  return path.replaceAll("\\", "/").split("/").at(-1) ?? "";
}

async function readJson(path: string, label: string): Promise<unknown> {
  try {
    return JSON.parse(await Deno.readTextFile(path));
  } catch (error) {
    fail(`${label} cannot be read as JSON: ${(error as Error).message}`);
  }
}

async function writeJson(path: string, value: unknown): Promise<void> {
  await Deno.writeTextFile(path, `${JSON.stringify(value, null, 2)}\n`);
}

export const TARGETS = [
  {
    key: "windows",
    runtimePlatform: "Windows",
    runtimeArch: "x86_64",
    runtimeArtifactName: "floorp-windows-x86_64-moz-artifact",
    platform: "WINNT",
    arch: "x86_64",
    marName: "floorp-windows-x86_64-full.mar",
    metadataName: "win-meta.json",
  },
  {
    key: "linux",
    runtimePlatform: "Linux",
    runtimeArch: "x86_64",
    runtimeArtifactName: "floorp-linux-x86_64-moz-artifact",
    platform: "Linux",
    arch: "x86_64",
    marName: "floorp-linux-x86_64-full.mar",
    metadataName: "linux-meta.json",
  },
  {
    key: "linuxAarch64",
    runtimePlatform: "Linux",
    runtimeArch: "aarch64",
    runtimeArtifactName: "floorp-linux-aarch64-moz-artifact",
    platform: "Linux",
    arch: "aarch64",
    marName: "floorp-linux-aarch64-full.mar",
    metadataName: "linux-aarch64-meta.json",
  },
  {
    key: "mac",
    runtimePlatform: "Darwin",
    runtimeArch: "universal",
    runtimeArtifactName: "floorp-mac-universal-moz-artifact",
    platform: "Darwin",
    arch: "universal",
    marName: "floorp-mac-universal-full.mar",
    metadataName: "mac-meta.json",
  },
] as const;

export type TargetKey = (typeof TARGETS)[number]["key"];
export type NativeArch = "x86_64" | "aarch64";

interface RuntimeTarget {
  platform: string;
  arch: string;
  artifact_name: string;
  artifact_id: number;
  artifact_digest: string;
  expected_build_id: string;
}

interface ArtifactIdentity {
  id: number;
  name: string;
  digest: string;
}

export interface RuntimeProvenance {
  schema_version: 2;
  repository: "Floorp-Projects/Floorp-Runtime";
  workflow_path: ".github/workflows/daily-build.yml";
  head_sha: string;
  workflow_run_id: number;
  run_created_at: string;
  run_attempt: number;
  expected_build_id: string;
  manifest_artifact: ArtifactIdentity;
  targets: RuntimeTarget[];
}

function parseRuntimeManifest(
  value: unknown,
  path: string,
): Omit<RuntimeProvenance, "workflow_path" | "manifest_artifact"> {
  const root = exactObject(value, [
    "schema_version",
    "repository",
    "head_sha",
    "workflow_run_id",
    "run_created_at",
    "run_attempt",
    "expected_build_id",
    "targets",
  ], path);
  expectLiteral(root.schema_version, 2, `${path}.schema_version`);
  const repository = expectLiteral(
    root.repository,
    "Floorp-Projects/Floorp-Runtime",
    `${path}.repository`,
  );
  const headSha = validateSha1(root.head_sha, `${path}.head_sha`);
  const workflowRunId = positiveInteger(
    root.workflow_run_id,
    `${path}.workflow_run_id`,
  );
  const runCreatedAt = stringValue(
    root.run_created_at,
    `${path}.run_created_at`,
  );
  const derivedBuildId = buildIdFromCreatedAt(
    runCreatedAt,
    `${path}.run_created_at`,
  );
  const runAttempt = positiveInteger(root.run_attempt, `${path}.run_attempt`);
  const expectedBuildId = validateBuildId(
    root.expected_build_id,
    `${path}.expected_build_id`,
  );
  if (derivedBuildId !== expectedBuildId) {
    fail(`${path}.expected_build_id does not match run_created_at`);
  }
  const rawTargets = arrayValue(root.targets, `${path}.targets`);
  if (rawTargets.length !== TARGETS.length) {
    fail(`${path}.targets must contain exactly ${TARGETS.length} entries`);
  }
  const ids = new Set<number>();
  const digests = new Set<string>();
  const targets = TARGETS.map((definition, index): RuntimeTarget => {
    const targetPath = `${path}.targets[${index}]`;
    const target = exactObject(rawTargets[index], [
      "platform",
      "arch",
      "artifact_name",
      "artifact_id",
      "artifact_digest",
      "expected_build_id",
    ], targetPath);
    const platform = expectLiteral(
      target.platform,
      definition.runtimePlatform,
      `${targetPath}.platform`,
    );
    const arch = expectLiteral(
      target.arch,
      definition.runtimeArch,
      `${targetPath}.arch`,
    );
    const artifactName = expectLiteral(
      target.artifact_name,
      definition.runtimeArtifactName,
      `${targetPath}.artifact_name`,
    );
    const artifactId = positiveInteger(
      target.artifact_id,
      `${targetPath}.artifact_id`,
    );
    const artifactDigest = validateSha256(
      target.artifact_digest,
      `${targetPath}.artifact_digest`,
    );
    const targetBuildId = validateBuildId(
      target.expected_build_id,
      `${targetPath}.expected_build_id`,
    );
    if (targetBuildId !== expectedBuildId) {
      fail(`${targetPath}.expected_build_id is mixed`);
    }
    if (ids.has(artifactId)) fail(`${targetPath}.artifact_id is duplicated`);
    if (digests.has(artifactDigest)) {
      fail(`${targetPath}.artifact_digest is duplicated`);
    }
    ids.add(artifactId);
    digests.add(artifactDigest);
    return {
      platform,
      arch,
      artifact_name: artifactName,
      artifact_id: artifactId,
      artifact_digest: artifactDigest,
      expected_build_id: targetBuildId,
    };
  });
  return {
    schema_version: 2,
    repository,
    head_sha: headSha,
    workflow_run_id: workflowRunId,
    run_created_at: runCreatedAt,
    run_attempt: runAttempt,
    expected_build_id: expectedBuildId,
    targets,
  };
}

function parseRestArtifact(
  value: unknown,
  path: string,
  expected: { id?: number; name: string; digest?: string },
  runId: number,
  headSha: string,
): ArtifactIdentity {
  const root = exactObject(value, [
    "id",
    "name",
    "digest",
    "expired",
    "workflow_run_id",
    "workflow_run_head_sha",
  ], path);
  const id = positiveInteger(root.id, `${path}.id`);
  const name = stringValue(root.name, `${path}.name`);
  const digest = validateSha256(root.digest, `${path}.digest`);
  if (expected.id !== undefined && id !== expected.id) {
    fail(`${path}.id does not match manifest`);
  }
  if (name !== expected.name) {
    fail(`${path}.name does not match expected artifact`);
  }
  if (expected.digest !== undefined && digest !== expected.digest) {
    fail(`${path}.digest does not match manifest`);
  }
  if (booleanValue(root.expired, `${path}.expired`)) fail(`${path} is expired`);
  if (
    positiveInteger(root.workflow_run_id, `${path}.workflow_run_id`) !== runId
  ) {
    fail(`${path} belongs to a different workflow run`);
  }
  if (
    validateSha1(
      root.workflow_run_head_sha,
      `${path}.workflow_run_head_sha`,
    ) !== headSha
  ) {
    fail(`${path} belongs to a different head SHA`);
  }
  return { id, name, digest };
}

export function validateRuntimeProvenance(
  manifestValue: unknown,
  restSnapshotValue: unknown,
): RuntimeProvenance {
  const manifest = parseRuntimeManifest(manifestValue, "runtime manifest");
  const snapshot = exactObject(restSnapshotValue, [
    "schema_version",
    "repository",
    "workflow_path",
    "run",
    "manifest_artifact",
    "target_artifacts",
  ], "REST snapshot");
  expectLiteral(snapshot.schema_version, 2, "REST snapshot.schema_version");
  expectLiteral(
    snapshot.repository,
    manifest.repository,
    "REST snapshot.repository",
  );
  const workflowPath = expectLiteral(
    snapshot.workflow_path,
    ".github/workflows/daily-build.yml",
    "REST snapshot.workflow_path",
  );
  const run = exactObject(snapshot.run, [
    "id",
    "status",
    "conclusion",
    "head_sha",
    "run_attempt",
    "created_at",
  ], "REST snapshot.run");
  const runId = positiveInteger(run.id, "REST snapshot.run.id");
  expectLiteral(run.status, "completed", "REST snapshot.run.status");
  expectLiteral(run.conclusion, "success", "REST snapshot.run.conclusion");
  const headSha = validateSha1(run.head_sha, "REST snapshot.run.head_sha");
  const runAttempt = positiveInteger(
    run.run_attempt,
    "REST snapshot.run.run_attempt",
  );
  const createdAt = stringValue(run.created_at, "REST snapshot.run.created_at");
  const buildId = buildIdFromCreatedAt(
    createdAt,
    "REST snapshot.run.created_at",
  );
  if (runId !== manifest.workflow_run_id) {
    fail("REST workflow run ID does not match manifest");
  }
  if (headSha !== manifest.head_sha) {
    fail("REST head SHA does not match manifest");
  }
  if (runAttempt !== manifest.run_attempt) {
    fail("REST run attempt does not match manifest");
  }
  if (createdAt !== manifest.run_created_at) {
    fail("REST created_at does not match manifest");
  }
  if (buildId !== manifest.expected_build_id) {
    fail("REST created_at BuildID does not match manifest");
  }

  const manifestArtifact = parseRestArtifact(
    snapshot.manifest_artifact,
    "REST snapshot.manifest_artifact",
    { name: "floorp-runtime-build-manifest-v2" },
    runId,
    headSha,
  );
  const rawArtifacts = arrayValue(
    snapshot.target_artifacts,
    "REST snapshot.target_artifacts",
  );
  if (rawArtifacts.length !== TARGETS.length) {
    fail(
      `REST snapshot.target_artifacts must contain exactly ${TARGETS.length} entries`,
    );
  }
  const artifacts = manifest.targets.map((target, index) =>
    parseRestArtifact(
      rawArtifacts[index],
      `REST snapshot.target_artifacts[${index}]`,
      {
        id: target.artifact_id,
        name: target.artifact_name,
        digest: target.artifact_digest,
      },
      runId,
      headSha,
    )
  );
  const ids = new Set([
    manifestArtifact.id,
    ...artifacts.map((artifact) => artifact.id),
  ]);
  const digests = new Set([
    manifestArtifact.digest,
    ...artifacts.map((artifact) => artifact.digest),
  ]);
  if (ids.size !== TARGETS.length + 1) {
    fail("Runtime artifact IDs must be unique");
  }
  if (digests.size !== TARGETS.length + 1) {
    fail("Runtime artifact digests must be unique");
  }

  return {
    ...manifest,
    workflow_path: workflowPath,
    manifest_artifact: manifestArtifact,
  };
}

function parseNormalizedRuntimeProvenance(value: unknown): RuntimeProvenance {
  const root = exactObject(value, [
    "schema_version",
    "repository",
    "workflow_path",
    "head_sha",
    "workflow_run_id",
    "run_created_at",
    "run_attempt",
    "expected_build_id",
    "manifest_artifact",
    "targets",
  ], "normalized Runtime provenance");
  const manifest = parseRuntimeManifest({
    schema_version: root.schema_version,
    repository: root.repository,
    head_sha: root.head_sha,
    workflow_run_id: root.workflow_run_id,
    run_created_at: root.run_created_at,
    run_attempt: root.run_attempt,
    expected_build_id: root.expected_build_id,
    targets: root.targets,
  }, "normalized Runtime provenance");
  const workflowPath = expectLiteral(
    root.workflow_path,
    ".github/workflows/daily-build.yml",
    "normalized Runtime provenance.workflow_path",
  );
  const artifact = exactObject(
    root.manifest_artifact,
    ["id", "name", "digest"],
    "normalized Runtime provenance.manifest_artifact",
  );
  const manifestArtifact = {
    id: positiveInteger(
      artifact.id,
      "normalized Runtime provenance.manifest_artifact.id",
    ),
    name: expectLiteral(
      artifact.name,
      "floorp-runtime-build-manifest-v2",
      "normalized Runtime provenance.manifest_artifact.name",
    ),
    digest: validateSha256(
      artifact.digest,
      "normalized Runtime provenance.manifest_artifact.digest",
    ),
  };
  if (
    manifest.targets.some((target) =>
      target.artifact_id === manifestArtifact.id
    )
  ) {
    fail(
      "normalized Runtime manifest artifact ID collides with a target artifact",
    );
  }
  if (
    manifest.targets.some((target) =>
      target.artifact_digest === manifestArtifact.digest
    )
  ) {
    fail("normalized Runtime manifest digest collides with a target artifact");
  }
  return {
    ...manifest,
    workflow_path: workflowPath,
    manifest_artifact: manifestArtifact,
  };
}

export interface FullVersionIdentity {
  firefoxVersion: string;
  appBuildId: string;
  platformBuildId: string;
}

export function parseFullVersionOutput(output: unknown): FullVersionIdentity {
  const value = stringValue(output, "--full-version output");
  const buildIds = value.match(/(?<!\d)\d{14}(?!\d)/g) ?? [];
  if (buildIds.length !== 2) {
    fail(
      `--full-version output must contain exactly two 14-digit build IDs; found ${buildIds.length}`,
    );
  }
  const pairs = [...value.matchAll(/(?<!\d)(\d{14})\s+(\d{14})(?!\d)/g)];
  if (pairs.length !== 1) {
    fail(
      `--full-version output must contain exactly one adjacent build ID pair; found ${pairs.length}`,
    );
  }
  const pair = pairs[0];
  const prefix = value.slice(0, pair.index).trimEnd();
  const versionMatch =
    /(?:^|\s)((?:0|[1-9]\d*)\.(?:0|[1-9]\d*)(?:\.(?:0|[1-9]\d*))?)$/.exec(
      prefix,
    );
  if (!versionMatch) {
    fail(
      "--full-version output must place a Firefox version immediately before the build ID pair",
    );
  }
  return {
    firefoxVersion: validateFirefoxVersion(
      versionMatch[1],
      "--full-version Firefox version",
    ),
    appBuildId: validateBuildId(pair[1], "--full-version application build ID"),
    platformBuildId: validateBuildId(
      pair[2],
      "--full-version platform build ID",
    ),
  };
}

interface RuntimeRecordIdentity {
  repository: "Floorp-Projects/Floorp-Runtime";
  head_sha: string;
  workflow_run_id: number;
  artifact_id: number;
  artifact_digest: string;
  expected_build_id: string;
}

interface FloorpIdentity {
  repository: "Floorp-Projects/Floorp";
  head_sha: string;
  workflow_run_id: number;
}

interface FloorpPackageIdentity {
  artifact_name: string;
  artifact_id: number;
  artifact_digest: string;
  unsigned: boolean;
}

export interface NativeVerificationRecord {
  schema_version: 2;
  target_key: TargetKey;
  native_arch: NativeArch;
  firefox_version: string;
  app_build_id: string;
  platform_build_id: string;
  build_id2: string;
  runtime: RuntimeRecordIdentity;
  floorp: FloorpIdentity;
  floorp_package: FloorpPackageIdentity;
  verification: {
    status: "verified";
    method: "full-version";
  };
}

interface NativeVerificationDescriptor {
  schema_version: 2;
  target_key: TargetKey;
  native_arch: NativeArch;
  runtime_manifest_path: string;
  full_version_output_path: string;
  build_id2_path: string;
  floorp: FloorpIdentity;
  floorp_package: FloorpPackageIdentity;
}

function validateTargetKey(value: unknown, path: string): TargetKey {
  const key = stringValue(value, path);
  if (!TARGETS.some((target) => target.key === key)) {
    fail(`${path} is not a supported target`);
  }
  return key as TargetKey;
}

function validateNativeArch(value: unknown, path: string): NativeArch {
  const arch = stringValue(value, path);
  if (arch !== "x86_64" && arch !== "aarch64") fail(`${path} is not supported`);
  return arch;
}

function validateTargetNativePair(
  target: TargetKey,
  arch: NativeArch,
  path: string,
): void {
  const expected = target === "linuxAarch64" ? "aarch64" : "x86_64";
  if (target === "mac") return;
  if (arch !== expected) fail(`${path} must use ${expected} for ${target}`);
}

function expectedPackageArtifactName(
  target: TargetKey,
  unsigned: boolean,
): string {
  const base = {
    windows: "noraneko-windows-x86_64-installer",
    linux: "noraneko-linux-x86_64-installer",
    linuxAarch64: "noraneko-linux-aarch64-installer",
    mac: "noraneko-mac-universal-installer",
  }[target];
  return `${base}${unsigned ? "-unsigned" : ""}`;
}

function parseFloorpIdentity(value: unknown, path: string): FloorpIdentity {
  const root = exactObject(
    value,
    ["repository", "head_sha", "workflow_run_id"],
    path,
  );
  return {
    repository: expectLiteral(
      root.repository,
      "Floorp-Projects/Floorp",
      `${path}.repository`,
    ),
    head_sha: validateSha1(root.head_sha, `${path}.head_sha`),
    workflow_run_id: positiveInteger(
      root.workflow_run_id,
      `${path}.workflow_run_id`,
    ),
  };
}

function parseFloorpPackageIdentity(
  value: unknown,
  path: string,
): FloorpPackageIdentity {
  const root = exactObject(
    value,
    ["artifact_name", "artifact_id", "artifact_digest", "unsigned"],
    path,
  );
  const artifactName = stringValue(root.artifact_name, `${path}.artifact_name`);
  if (!/^[A-Za-z0-9][A-Za-z0-9._-]*$/.test(artifactName)) {
    fail(`${path}.artifact_name contains unsafe characters`);
  }
  const unsigned = booleanValue(root.unsigned, `${path}.unsigned`);
  if (artifactName.endsWith("-unsigned") !== unsigned) {
    fail(
      `${path}.unsigned must exactly match the -unsigned artifact-name suffix`,
    );
  }
  return {
    artifact_name: artifactName,
    artifact_id: positiveInteger(root.artifact_id, `${path}.artifact_id`),
    artifact_digest: validateSha256(
      root.artifact_digest,
      `${path}.artifact_digest`,
    ),
    unsigned,
  };
}

function parseNativeVerificationDescriptor(
  value: unknown,
): NativeVerificationDescriptor {
  const root = exactObject(value, [
    "schema_version",
    "target_key",
    "native_arch",
    "runtime_manifest_path",
    "full_version_output_path",
    "build_id2_path",
    "floorp",
    "floorp_package",
  ], "native verification descriptor");
  expectLiteral(
    root.schema_version,
    2,
    "native verification descriptor.schema_version",
  );
  const targetKey = validateTargetKey(
    root.target_key,
    "native verification descriptor.target_key",
  );
  const nativeArch = validateNativeArch(
    root.native_arch,
    "native verification descriptor.native_arch",
  );
  validateTargetNativePair(
    targetKey,
    nativeArch,
    "native verification descriptor.native_arch",
  );
  const floorpPackage = parseFloorpPackageIdentity(
    root.floorp_package,
    "native verification descriptor.floorp_package",
  );
  if (
    floorpPackage.artifact_name !==
      expectedPackageArtifactName(targetKey, floorpPackage.unsigned)
  ) {
    fail(
      "native verification descriptor.floorp_package.artifact_name does not match target",
    );
  }
  return {
    schema_version: 2,
    target_key: targetKey,
    native_arch: nativeArch,
    runtime_manifest_path: stringValue(
      root.runtime_manifest_path,
      "native verification descriptor.runtime_manifest_path",
    ),
    full_version_output_path: stringValue(
      root.full_version_output_path,
      "native verification descriptor.full_version_output_path",
    ),
    build_id2_path: stringValue(
      root.build_id2_path,
      "native verification descriptor.build_id2_path",
    ),
    floorp: parseFloorpIdentity(
      root.floorp,
      "native verification descriptor.floorp",
    ),
    floorp_package: floorpPackage,
  };
}

export async function createNativeVerificationRecord(
  descriptorValue: unknown,
): Promise<NativeVerificationRecord> {
  const descriptor = parseNativeVerificationDescriptor(descriptorValue);
  const runtime = parseRuntimeManifest(
    await readJson(descriptor.runtime_manifest_path, "Runtime manifest"),
    "runtime manifest",
  );
  const targetIndex = TARGETS.findIndex((target) =>
    target.key === descriptor.target_key
  );
  const runtimeTarget = runtime.targets[targetIndex];
  let fullVersionOutput: string;
  let rawBuildId2: string;
  try {
    [fullVersionOutput, rawBuildId2] = await Promise.all([
      Deno.readTextFile(descriptor.full_version_output_path),
      Deno.readTextFile(descriptor.build_id2_path),
    ]);
  } catch (error) {
    fail(
      `native verification input cannot be read: ${(error as Error).message}`,
    );
  }
  const identity = parseFullVersionOutput(fullVersionOutput);
  if (identity.appBuildId !== runtime.expected_build_id) {
    fail("native application BuildID does not match Runtime provenance");
  }
  if (identity.platformBuildId !== runtime.expected_build_id) {
    fail("native platform BuildID does not match Runtime provenance");
  }
  const buildId2 = validateUuidV7(rawBuildId2.trim(), "native buildID2");
  return {
    schema_version: 2,
    target_key: descriptor.target_key,
    native_arch: descriptor.native_arch,
    firefox_version: identity.firefoxVersion,
    app_build_id: identity.appBuildId,
    platform_build_id: identity.platformBuildId,
    build_id2: buildId2,
    runtime: {
      repository: runtime.repository,
      head_sha: runtime.head_sha,
      workflow_run_id: runtime.workflow_run_id,
      artifact_id: runtimeTarget.artifact_id,
      artifact_digest: runtimeTarget.artifact_digest,
      expected_build_id: runtimeTarget.expected_build_id,
    },
    floorp: descriptor.floorp,
    floorp_package: descriptor.floorp_package,
    verification: { status: "verified", method: "full-version" },
  };
}

function parseNativeVerificationRecord(
  value: unknown,
  path: string,
): NativeVerificationRecord {
  const root = exactObject(value, [
    "schema_version",
    "target_key",
    "native_arch",
    "firefox_version",
    "app_build_id",
    "platform_build_id",
    "build_id2",
    "runtime",
    "floorp",
    "floorp_package",
    "verification",
  ], path);
  expectLiteral(root.schema_version, 2, `${path}.schema_version`);
  const targetKey = validateTargetKey(root.target_key, `${path}.target_key`);
  const nativeArch = validateNativeArch(
    root.native_arch,
    `${path}.native_arch`,
  );
  validateTargetNativePair(targetKey, nativeArch, `${path}.native_arch`);
  const floorpPackage = parseFloorpPackageIdentity(
    root.floorp_package,
    `${path}.floorp_package`,
  );
  if (
    floorpPackage.artifact_name !==
      expectedPackageArtifactName(targetKey, floorpPackage.unsigned)
  ) {
    fail(`${path}.floorp_package.artifact_name does not match target`);
  }
  const runtimeRoot = exactObject(root.runtime, [
    "repository",
    "head_sha",
    "workflow_run_id",
    "artifact_id",
    "artifact_digest",
    "expected_build_id",
  ], `${path}.runtime`);
  const runtime: RuntimeRecordIdentity = {
    repository: expectLiteral(
      runtimeRoot.repository,
      "Floorp-Projects/Floorp-Runtime",
      `${path}.runtime.repository`,
    ),
    head_sha: validateSha1(runtimeRoot.head_sha, `${path}.runtime.head_sha`),
    workflow_run_id: positiveInteger(
      runtimeRoot.workflow_run_id,
      `${path}.runtime.workflow_run_id`,
    ),
    artifact_id: positiveInteger(
      runtimeRoot.artifact_id,
      `${path}.runtime.artifact_id`,
    ),
    artifact_digest: validateSha256(
      runtimeRoot.artifact_digest,
      `${path}.runtime.artifact_digest`,
    ),
    expected_build_id: validateBuildId(
      runtimeRoot.expected_build_id,
      `${path}.runtime.expected_build_id`,
    ),
  };
  const verificationRoot = exactObject(
    root.verification,
    ["status", "method"],
    `${path}.verification`,
  );
  expectLiteral(
    verificationRoot.status,
    "verified",
    `${path}.verification.status`,
  );
  expectLiteral(
    verificationRoot.method,
    "full-version",
    `${path}.verification.method`,
  );
  const appBuildId = validateBuildId(root.app_build_id, `${path}.app_build_id`);
  const platformBuildId = validateBuildId(
    root.platform_build_id,
    `${path}.platform_build_id`,
  );
  if (
    appBuildId !== runtime.expected_build_id ||
    platformBuildId !== runtime.expected_build_id
  ) {
    fail(`${path} BuildIDs do not match its Runtime provenance`);
  }
  return {
    schema_version: 2,
    target_key: targetKey,
    native_arch: nativeArch,
    firefox_version: validateFirefoxVersion(
      root.firefox_version,
      `${path}.firefox_version`,
    ),
    app_build_id: appBuildId,
    platform_build_id: platformBuildId,
    build_id2: validateUuidV7(root.build_id2, `${path}.build_id2`),
    runtime,
    floorp: parseFloorpIdentity(root.floorp, `${path}.floorp`),
    floorp_package: floorpPackage,
    verification: { status: "verified", method: "full-version" },
  };
}

const NATIVE_RECORD_KEYS = [
  "windows/x86_64",
  "linux/x86_64",
  "linuxAarch64/aarch64",
  "mac/x86_64",
  "mac/aarch64",
] as const;

function recordKey(record: NativeVerificationRecord): string {
  return `${record.target_key}/${record.native_arch}`;
}

export function validateNativeVerificationSet(
  values: unknown,
  runtimeValue: unknown,
): NativeVerificationRecord[] {
  const rawRecords = arrayValue(values, "native verification records");
  if (rawRecords.length !== NATIVE_RECORD_KEYS.length) {
    fail(
      `native verification records must contain exactly ${NATIVE_RECORD_KEYS.length} entries`,
    );
  }
  const runtime = parseNormalizedRuntimeProvenance(runtimeValue);
  const byKey = new Map<string, NativeVerificationRecord>();
  for (let index = 0; index < rawRecords.length; index++) {
    const record = parseNativeVerificationRecord(
      rawRecords[index],
      `native verification records[${index}]`,
    );
    const key = recordKey(record);
    if (byKey.has(key)) fail(`duplicate native verification record ${key}`);
    byKey.set(key, record);
  }
  for (const key of NATIVE_RECORD_KEYS) {
    if (!byKey.has(key)) fail(`missing native verification record ${key}`);
  }
  const records = NATIVE_RECORD_KEYS.map((key) => byKey.get(key)!);
  const first = records[0];
  for (const record of records) {
    if (canonicalJson(record.floorp) !== canonicalJson(first.floorp)) {
      fail(`${recordKey(record)} has mixed Floorp provenance`);
    }
    if (
      record.runtime.repository !== runtime.repository ||
      record.runtime.head_sha !== runtime.head_sha ||
      record.runtime.workflow_run_id !== runtime.workflow_run_id ||
      record.runtime.expected_build_id !== runtime.expected_build_id
    ) {
      fail(`${recordKey(record)} has mixed Runtime run provenance`);
    }
    const targetIndex = TARGETS.findIndex((target) =>
      target.key === record.target_key
    );
    const runtimeTarget = runtime.targets[targetIndex];
    if (
      record.runtime.artifact_id !== runtimeTarget.artifact_id ||
      record.runtime.artifact_digest !== runtimeTarget.artifact_digest
    ) {
      fail(`${recordKey(record)} has mixed Runtime target artifact provenance`);
    }
    if (record.firefox_version !== first.firefox_version) {
      fail(`${recordKey(record)} has a mixed Firefox version`);
    }
  }
  const macX64 = byKey.get("mac/x86_64")!;
  const macArm64 = byKey.get("mac/aarch64")!;
  for (
    const field of [
      "firefox_version",
      "app_build_id",
      "platform_build_id",
      "build_id2",
    ] as const
  ) {
    if (macX64[field] !== macArm64[field]) {
      fail(`mac native verification records disagree on ${field}`);
    }
  }
  if (
    canonicalJson(macX64.floorp_package) !==
      canonicalJson(macArm64.floorp_package)
  ) {
    fail("mac native verification records disagree on package identity");
  }
  const packageIds = new Set<number>();
  const packageDigests = new Set<string>();
  for (const target of TARGETS) {
    const targetRecords = records.filter((record) =>
      record.target_key === target.key
    );
    const packageIdentity = targetRecords[0].floorp_package;
    if (packageIds.has(packageIdentity.artifact_id)) {
      fail(`${target.key} reuses another target's Floorp package artifact ID`);
    }
    if (packageDigests.has(packageIdentity.artifact_digest)) {
      fail(
        `${target.key} reuses another target's Floorp package artifact digest`,
      );
    }
    packageIds.add(packageIdentity.artifact_id);
    packageDigests.add(packageIdentity.artifact_digest);
    for (const record of targetRecords.slice(1)) {
      if (
        canonicalJson(record.floorp_package) !== canonicalJson(packageIdentity)
      ) {
        fail(`${target.key} records disagree on Floorp package identity`);
      }
    }
  }
  return records;
}

interface MarMetadata {
  url: string;
  name: string;
  size: number;
  sha512: string;
}

interface ReleaseMetadata {
  schema_version: 2;
  version_display: string;
  version: string;
  noraneko_version: string;
  buildid: string;
  noraneko_buildid: string;
  channel: "release";
  platform: "WINNT" | "Linux" | "Darwin";
  arch: "x86_64" | "aarch64" | "universal";
  manifest_set_id: string;
  mar: MarMetadata;
  provenance: {
    runtime_repository: "Floorp-Projects/Floorp-Runtime";
    runtime_head_sha: string;
    runtime_run_id: number;
    runtime_artifact_id: number;
    runtime_artifact_digest: string;
    floorp_repository: "Floorp-Projects/Floorp";
    floorp_head_sha: string;
    floorp_run_id: number;
    release_tag: string;
  };
  verification: {
    status: "verified";
    method: "full-version";
    app_build_id: string;
    build_id2: string;
  };
}

export function buildManifestSetIdentity(
  metadata: Record<TargetKey, ReleaseMetadata>,
): Record<string, unknown> {
  const first = metadata.windows;
  return {
    schema_version: 2,
    floorp: {
      repository: first.provenance.floorp_repository,
      head_sha: first.provenance.floorp_head_sha,
      run_id: first.provenance.floorp_run_id,
      release_tag: first.provenance.release_tag,
    },
    targets: TARGETS.map((definition) => {
      const value = metadata[definition.key];
      return {
        platform: value.platform,
        arch: value.arch,
        mar: {
          name: value.mar.name,
          size: value.mar.size,
          sha512: value.mar.sha512,
        },
        runtime: {
          repository: value.provenance.runtime_repository,
          head_sha: value.provenance.runtime_head_sha,
          run_id: value.provenance.runtime_run_id,
          artifact_id: value.provenance.runtime_artifact_id,
          artifact_digest: value.provenance.runtime_artifact_digest,
        },
        verification: {
          app_build_id: value.verification.app_build_id,
          build_id2: value.verification.build_id2,
        },
      };
    }),
  };
}

export async function computeManifestSetId(
  metadata: Record<TargetKey, ReleaseMetadata>,
): Promise<string> {
  const bytes = new TextEncoder().encode(
    canonicalJson(buildManifestSetIdentity(metadata)),
  );
  return `sha256:${await digestBytes("SHA-256", bytes)}`;
}

type ReleaseMode = "production" | "validation";

interface FileDescriptor {
  path: string;
  name: string;
  size: number;
  sha512: string;
}

interface MarDescriptor {
  path: string;
  url: string;
  size: number;
  sha512: string;
}

export interface StubSource {
  repository: "Floorp-Projects/Floorp";
  release_tag: string;
  release_id: number;
  asset_id: number;
  asset_name: "floorp-stub.installer.exe";
  asset_digest: string;
  size: number;
}

interface AssemblyDescriptor {
  schema_version: 2;
  mode: ReleaseMode;
  floorp_version: string;
  release_tag: string;
  runtime_provenance_path: string;
  native_verification_paths: string[];
  mars: Record<TargetKey, MarDescriptor>;
  release_files: Record<
    | "windows"
    | "windowsStub"
    | "linux"
    | "linuxAarch64"
    | "mac"
    | "linuxDeb",
    FileDescriptor
  >;
  stub_source: StubSource;
}

interface ReleaseFileIdentity {
  name: string;
  size: number;
  sha512: string;
}

interface ReleaseManifestTarget {
  target_key: TargetKey;
  platform: string;
  arch: string;
  metadata_name: string;
  metadata_sha256: string;
  mar: MarMetadata;
  runtime_artifact: ArtifactIdentity;
  floorp_package: FloorpPackageIdentity;
  native_verifications: Array<{
    native_arch: NativeArch;
    app_build_id: string;
    platform_build_id: string;
    build_id2: string;
  }>;
}

export interface ReleaseManifestSet {
  schema_version: 2;
  mode: ReleaseMode;
  manifest_set_id: string;
  version_display: string;
  version: string;
  noraneko_version: string;
  channel: "release";
  runtime: RuntimeProvenance;
  floorp: FloorpIdentity & { release_tag: string };
  release_files: Record<
    | "windows"
    | "windowsStub"
    | "linux"
    | "linuxAarch64"
    | "mac"
    | "linuxDeb",
    ReleaseFileIdentity
  >;
  stub_source: StubSource;
  targets: ReleaseManifestTarget[];
}

export interface AssembledReleaseBundle {
  manifest: ReleaseManifestSet;
  metadata: Record<TargetKey, ReleaseMetadata>;
  metadataFiles: Record<TargetKey, string>;
}

function parseMode(value: unknown, path: string): ReleaseMode {
  if (value !== "production" && value !== "validation") {
    fail(`${path} must be production or validation`);
  }
  return value;
}

function parseCanonicalReleaseTag(value: unknown, path: string): string {
  const releaseTag = stringValue(value, path);
  if (!releaseTag.startsWith("v")) {
    fail(`${path} must be a canonical v<SemVer> tag`);
  }
  const version = validateSemver(releaseTag.slice(1), path);
  if (releaseTag !== `v${version}`) {
    fail(`${path} must be a canonical v<SemVer> tag`);
  }
  return releaseTag;
}

function parseStubSource(value: unknown, path: string): StubSource {
  const root = exactObject(value, [
    "repository",
    "release_tag",
    "release_id",
    "asset_id",
    "asset_name",
    "asset_digest",
    "size",
  ], path);
  return {
    repository: expectLiteral(
      root.repository,
      FLOORP_REPOSITORY,
      `${path}.repository`,
    ),
    release_tag: parseCanonicalReleaseTag(
      root.release_tag,
      `${path}.release_tag`,
    ),
    release_id: positiveInteger(root.release_id, `${path}.release_id`),
    asset_id: positiveInteger(root.asset_id, `${path}.asset_id`),
    asset_name: expectLiteral(
      root.asset_name,
      STUB_ASSET_NAME,
      `${path}.asset_name`,
    ),
    asset_digest: validateSha256(
      root.asset_digest,
      `${path}.asset_digest`,
    ),
    size: positiveInteger(root.size, `${path}.size`),
  };
}

function parseAssemblyDescriptor(value: unknown): AssemblyDescriptor {
  const root = exactObject(value, [
    "schema_version",
    "mode",
    "floorp_version",
    "release_tag",
    "runtime_provenance_path",
    "native_verification_paths",
    "mars",
    "release_files",
    "stub_source",
  ], "release assembly descriptor");
  expectLiteral(
    root.schema_version,
    2,
    "release assembly descriptor.schema_version",
  );
  const floorpVersion = validateSemver(
    root.floorp_version,
    "release assembly descriptor.floorp_version",
  );
  const releaseTag = stringValue(
    root.release_tag,
    "release assembly descriptor.release_tag",
  );
  if (releaseTag !== `v${floorpVersion}`) {
    fail("release assembly descriptor.release_tag must be v<floorp_version>");
  }
  const rawNativePaths = arrayValue(
    root.native_verification_paths,
    "release assembly descriptor.native_verification_paths",
  );
  if (rawNativePaths.length !== NATIVE_RECORD_KEYS.length) {
    fail(
      `release assembly descriptor.native_verification_paths must contain exactly ${NATIVE_RECORD_KEYS.length} paths`,
    );
  }
  const nativePaths = rawNativePaths.map((path, index) =>
    stringValue(
      path,
      `release assembly descriptor.native_verification_paths[${index}]`,
    )
  );
  if (new Set(nativePaths).size !== nativePaths.length) {
    fail(
      "release assembly descriptor.native_verification_paths contains duplicates",
    );
  }
  const rawMars = exactObject(
    root.mars,
    TARGETS.map((target) => target.key),
    "release assembly descriptor.mars",
  );
  const mars = {} as Record<TargetKey, MarDescriptor>;
  for (const target of TARGETS) {
    const path = `release assembly descriptor.mars.${target.key}`;
    const item = exactObject(rawMars[target.key], [
      "path",
      "url",
      "size",
      "sha512",
    ], path);
    const expectedUrl =
      `https://github.com/Floorp-Projects/Floorp/releases/download/${releaseTag}/${target.marName}`;
    const url = stringValue(item.url, `${path}.url`);
    if (url !== expectedUrl) {
      fail(`${path}.url is not the canonical release URL`);
    }
    mars[target.key] = {
      path: stringValue(item.path, `${path}.path`),
      url,
      size: positiveInteger(item.size, `${path}.size`),
      sha512: validateSha512(item.sha512, `${path}.sha512`),
    };
  }

  const expectedReleaseNames = {
    windows: "floorp-windows-x86_64.installer.exe",
    windowsStub: STUB_ASSET_NAME,
    linux: "floorp-linux-x86_64.tar.xz",
    linuxAarch64: "floorp-linux-aarch64.tar.xz",
    mac: "floorp-macOS-universal.dmg",
    linuxDeb: `floorp-${floorpVersion}.deb`,
  } as const;
  const rawReleaseFiles = exactObject(
    root.release_files,
    Object.keys(expectedReleaseNames),
    "release assembly descriptor.release_files",
  );
  const releaseFiles = {} as AssemblyDescriptor["release_files"];
  for (
    const key of Object.keys(expectedReleaseNames) as Array<
      keyof typeof expectedReleaseNames
    >
  ) {
    const path = `release assembly descriptor.release_files.${key}`;
    const item = exactObject(rawReleaseFiles[key], [
      "path",
      "name",
      "size",
      "sha512",
    ], path);
    const filePath = stringValue(item.path, `${path}.path`);
    const name = stringValue(item.name, `${path}.name`);
    if (name !== expectedReleaseNames[key]) {
      fail(`${path}.name is not the fixed release asset name`);
    }
    if (pathBasename(filePath) !== name) {
      fail(`${path}.name does not match the path basename`);
    }
    releaseFiles[key] = {
      path: filePath,
      name,
      size: positiveInteger(item.size, `${path}.size`),
      sha512: validateSha512(item.sha512, `${path}.sha512`),
    };
  }
  return {
    schema_version: 2,
    mode: parseMode(root.mode, "release assembly descriptor.mode"),
    floorp_version: floorpVersion,
    release_tag: releaseTag,
    runtime_provenance_path: stringValue(
      root.runtime_provenance_path,
      "release assembly descriptor.runtime_provenance_path",
    ),
    native_verification_paths: nativePaths,
    mars,
    release_files: releaseFiles,
    stub_source: parseStubSource(
      root.stub_source,
      "release assembly descriptor.stub_source",
    ),
  };
}

async function verifyDescribedFile(
  descriptor: { path: string; size: number; sha512: string },
  path: string,
): Promise<{ size: number; sha512: string }> {
  const actual = await hashFile(descriptor.path, "sha512");
  if (actual.size !== descriptor.size) {
    fail(`${path}.size does not match the real file`);
  }
  if (actual.digest !== descriptor.sha512) {
    fail(`${path}.sha512 does not match the real file`);
  }
  return { size: actual.size, sha512: actual.digest };
}

async function verifyStubSourceFile(
  filePath: string,
  source: StubSource,
  path: string,
): Promise<void> {
  const actual = await hashFile(filePath, "sha256");
  if (actual.size !== source.size) {
    fail(`${path}.size does not match the real stub file`);
  }
  if (`sha256:${actual.digest}` !== source.asset_digest) {
    fail(`${path}.asset_digest does not match the real stub file`);
  }
}

async function prepareReleaseBundle(
  descriptorValue: unknown,
): Promise<AssembledReleaseBundle> {
  const descriptor = parseAssemblyDescriptor(descriptorValue);
  const runtime = parseNormalizedRuntimeProvenance(
    await readJson(
      descriptor.runtime_provenance_path,
      "normalized Runtime provenance",
    ),
  );
  const rawRecords = await Promise.all(
    descriptor.native_verification_paths.map((path, index) =>
      readJson(path, `native verification record ${index}`)
    ),
  );
  const records = validateNativeVerificationSet(rawRecords, runtime);
  const floorp = records[0].floorp;
  const unsignedStates = new Set(
    records.map((record) => record.floorp_package.unsigned),
  );
  if (unsignedStates.size !== 1) {
    fail("native records mix signed and unsigned Floorp packages");
  }
  const unsigned = records[0].floorp_package.unsigned;
  if (descriptor.mode === "production" && unsigned) {
    fail(
      "production release assembly rejects unsigned Floorp package evidence",
    );
  }

  const marIdentities = {} as Record<TargetKey, MarMetadata>;
  for (const target of TARGETS) {
    const described = descriptor.mars[target.key];
    const actual = await verifyDescribedFile(
      described,
      `release assembly descriptor.mars.${target.key}`,
    );
    if (pathBasename(described.path) !== target.marName) {
      fail(
        `release assembly descriptor.mars.${target.key}.path must end in ${target.marName}`,
      );
    }
    marIdentities[target.key] = {
      url: described.url,
      name: target.marName,
      size: actual.size,
      sha512: actual.sha512,
    };
  }
  const releaseFiles = {} as ReleaseManifestSet["release_files"];
  for (
    const key of Object.keys(descriptor.release_files) as Array<
      keyof AssemblyDescriptor["release_files"]
    >
  ) {
    const described = descriptor.release_files[key];
    const actual = await verifyDescribedFile(
      described,
      `release assembly descriptor.release_files.${key}`,
    );
    releaseFiles[key] = {
      name: described.name,
      size: actual.size,
      sha512: actual.sha512,
    };
  }
  await verifyStubSourceFile(
    descriptor.release_files.windowsStub.path,
    descriptor.stub_source,
    "release assembly descriptor.stub_source",
  );

  const metadata = {} as Record<TargetKey, ReleaseMetadata>;
  for (const target of TARGETS) {
    const record = records.find((candidate) =>
      candidate.target_key === target.key
    )!;
    metadata[target.key] = {
      schema_version: 2,
      version_display: `${descriptor.floorp_version}@${record.firefox_version}`,
      version: record.firefox_version,
      noraneko_version: descriptor.floorp_version,
      buildid: record.app_build_id,
      noraneko_buildid: record.build_id2,
      channel: "release",
      platform: target.platform,
      arch: target.arch,
      manifest_set_id: "",
      mar: marIdentities[target.key],
      provenance: {
        runtime_repository: record.runtime.repository,
        runtime_head_sha: record.runtime.head_sha,
        runtime_run_id: record.runtime.workflow_run_id,
        runtime_artifact_id: record.runtime.artifact_id,
        runtime_artifact_digest: record.runtime.artifact_digest,
        floorp_repository: floorp.repository,
        floorp_head_sha: floorp.head_sha,
        floorp_run_id: floorp.workflow_run_id,
        release_tag: descriptor.release_tag,
      },
      verification: {
        status: "verified",
        method: "full-version",
        app_build_id: record.app_build_id,
        build_id2: record.build_id2,
      },
    };
  }
  const manifestSetId = await computeManifestSetId(metadata);
  for (const target of TARGETS) {
    metadata[target.key].manifest_set_id = manifestSetId;
  }

  const metadataFiles = {} as Record<TargetKey, string>;
  const manifestTargets: ReleaseManifestTarget[] = [];
  for (const target of TARGETS) {
    const content = `${JSON.stringify(metadata[target.key], null, 2)}\n`;
    metadataFiles[target.key] = content;
    const targetRecords = records.filter((record) =>
      record.target_key === target.key
    );
    manifestTargets.push({
      target_key: target.key,
      platform: target.platform,
      arch: target.arch,
      metadata_name: target.metadataName,
      metadata_sha256: `sha256:${await digestBytes(
        "SHA-256",
        new TextEncoder().encode(content),
      )}`,
      mar: marIdentities[target.key],
      runtime_artifact: {
        id: targetRecords[0].runtime.artifact_id,
        name: target.runtimeArtifactName,
        digest: targetRecords[0].runtime.artifact_digest,
      },
      floorp_package: targetRecords[0].floorp_package,
      native_verifications: targetRecords.map((record) => ({
        native_arch: record.native_arch,
        app_build_id: record.app_build_id,
        platform_build_id: record.platform_build_id,
        build_id2: record.build_id2,
      })),
    });
  }
  const firefoxVersion = records[0].firefox_version;
  return {
    metadata,
    metadataFiles,
    manifest: {
      schema_version: 2,
      mode: descriptor.mode,
      manifest_set_id: manifestSetId,
      version_display: `${descriptor.floorp_version}@${firefoxVersion}`,
      version: firefoxVersion,
      noraneko_version: descriptor.floorp_version,
      channel: "release",
      runtime,
      floorp: { ...floorp, release_tag: descriptor.release_tag },
      release_files: releaseFiles,
      stub_source: descriptor.stub_source,
      targets: manifestTargets,
    },
  };
}

function expectedBundleFileNames(floorpVersion: string): string[] {
  return [
    ...TARGETS.flatMap((target) => [target.marName, target.metadataName]),
    "floorp-windows-x86_64.installer.exe",
    STUB_ASSET_NAME,
    "floorp-linux-x86_64.tar.xz",
    "floorp-linux-aarch64.tar.xz",
    "floorp-macOS-universal.dmg",
    `floorp-${floorpVersion}.deb`,
    "release-manifest-set-v2.json",
    "hashes.txt",
  ].sort();
}

async function renderHashesFile(
  bundleDirectory: string,
  fileNames: string[],
): Promise<string> {
  const lines: string[] = [];
  for (
    const name of fileNames.filter((name) => name !== "hashes.txt").sort()
  ) {
    const hash = await hashFile(`${bundleDirectory}/${name}`, "sha256");
    lines.push(`${hash.digest}  ${name}`);
  }
  return `${lines.join("\n")}\n`;
}

function parseReleaseManifestSet(value: unknown): {
  manifest: ReleaseManifestSet;
  metadata: Record<TargetKey, ReleaseMetadata>;
} {
  const root = exactObject(value, [
    "schema_version",
    "mode",
    "manifest_set_id",
    "version_display",
    "version",
    "noraneko_version",
    "channel",
    "runtime",
    "floorp",
    "release_files",
    "stub_source",
    "targets",
  ], "release manifest");
  expectLiteral(root.schema_version, 2, "release manifest.schema_version");
  const mode = parseMode(root.mode, "release manifest.mode");
  const manifestSetId = validateSha256(
    root.manifest_set_id,
    "release manifest.manifest_set_id",
  );
  const firefoxVersion = validateFirefoxVersion(
    root.version,
    "release manifest.version",
  );
  const floorpVersion = validateSemver(
    root.noraneko_version,
    "release manifest.noraneko_version",
  );
  const versionDisplay = stringValue(
    root.version_display,
    "release manifest.version_display",
  );
  if (versionDisplay !== `${floorpVersion}@${firefoxVersion}`) {
    fail("release manifest.version_display must be Floorp@Firefox");
  }
  expectLiteral(root.channel, "release", "release manifest.channel");
  const runtime = parseNormalizedRuntimeProvenance(root.runtime);
  const floorpRoot = exactObject(
    root.floorp,
    ["repository", "head_sha", "workflow_run_id", "release_tag"],
    "release manifest.floorp",
  );
  const floorpIdentity = parseFloorpIdentity({
    repository: floorpRoot.repository,
    head_sha: floorpRoot.head_sha,
    workflow_run_id: floorpRoot.workflow_run_id,
  }, "release manifest.floorp");
  const releaseTag = stringValue(
    floorpRoot.release_tag,
    "release manifest.floorp.release_tag",
  );
  if (releaseTag !== `v${floorpVersion}`) {
    fail("release manifest release tag does not match version");
  }

  const expectedReleaseNames = {
    windows: "floorp-windows-x86_64.installer.exe",
    windowsStub: STUB_ASSET_NAME,
    linux: "floorp-linux-x86_64.tar.xz",
    linuxAarch64: "floorp-linux-aarch64.tar.xz",
    mac: "floorp-macOS-universal.dmg",
    linuxDeb: `floorp-${floorpVersion}.deb`,
  } as const;
  const rawFiles = exactObject(
    root.release_files,
    Object.keys(expectedReleaseNames),
    "release manifest.release_files",
  );
  const releaseFiles = {} as ReleaseManifestSet["release_files"];
  for (
    const key of Object.keys(expectedReleaseNames) as Array<
      keyof typeof expectedReleaseNames
    >
  ) {
    const path = `release manifest.release_files.${key}`;
    const file = exactObject(
      rawFiles[key],
      ["name", "size", "sha512"],
      path,
    );
    releaseFiles[key] = {
      name: expectLiteral(
        file.name,
        expectedReleaseNames[key],
        `${path}.name`,
      ),
      size: positiveInteger(file.size, `${path}.size`),
      sha512: validateSha512(file.sha512, `${path}.sha512`),
    };
  }
  const stubSource = parseStubSource(
    root.stub_source,
    "release manifest.stub_source",
  );

  const rawTargets = arrayValue(root.targets, "release manifest.targets");
  if (rawTargets.length !== TARGETS.length) {
    fail("release manifest.targets must contain exactly four entries");
  }
  const targets: ReleaseManifestTarget[] = [];
  const metadata = {} as Record<TargetKey, ReleaseMetadata>;
  for (let index = 0; index < TARGETS.length; index++) {
    const definition = TARGETS[index];
    const path = `release manifest.targets[${index}]`;
    const target = exactObject(rawTargets[index], [
      "target_key",
      "platform",
      "arch",
      "metadata_name",
      "metadata_sha256",
      "mar",
      "runtime_artifact",
      "floorp_package",
      "native_verifications",
    ], path);
    expectLiteral(target.target_key, definition.key, `${path}.target_key`);
    const platform = expectLiteral(
      target.platform,
      definition.platform,
      `${path}.platform`,
    );
    const arch = expectLiteral(target.arch, definition.arch, `${path}.arch`);
    const metadataName = expectLiteral(
      target.metadata_name,
      definition.metadataName,
      `${path}.metadata_name`,
    );
    const metadataSha256 = validateSha256(
      target.metadata_sha256,
      `${path}.metadata_sha256`,
    );
    const marRoot = exactObject(
      target.mar,
      ["url", "name", "size", "sha512"],
      `${path}.mar`,
    );
    const expectedUrl =
      `https://github.com/Floorp-Projects/Floorp/releases/download/${releaseTag}/${definition.marName}`;
    const mar: MarMetadata = {
      url: expectLiteral(marRoot.url, expectedUrl, `${path}.mar.url`),
      name: expectLiteral(
        marRoot.name,
        definition.marName,
        `${path}.mar.name`,
      ),
      size: positiveInteger(marRoot.size, `${path}.mar.size`),
      sha512: validateSha512(marRoot.sha512, `${path}.mar.sha512`),
    };
    const runtimeArtifactRoot = exactObject(
      target.runtime_artifact,
      ["id", "name", "digest"],
      `${path}.runtime_artifact`,
    );
    const runtimeTarget = runtime.targets[index];
    const runtimeArtifact: ArtifactIdentity = {
      id: positiveInteger(
        runtimeArtifactRoot.id,
        `${path}.runtime_artifact.id`,
      ),
      name: expectLiteral(
        runtimeArtifactRoot.name,
        definition.runtimeArtifactName,
        `${path}.runtime_artifact.name`,
      ),
      digest: validateSha256(
        runtimeArtifactRoot.digest,
        `${path}.runtime_artifact.digest`,
      ),
    };
    if (
      runtimeArtifact.id !== runtimeTarget.artifact_id ||
      runtimeArtifact.digest !== runtimeTarget.artifact_digest
    ) {
      fail(
        `${path}.runtime_artifact does not match embedded Runtime provenance`,
      );
    }
    const floorpPackage = parseFloorpPackageIdentity(
      target.floorp_package,
      `${path}.floorp_package`,
    );
    if (
      floorpPackage.artifact_name !==
        expectedPackageArtifactName(definition.key, floorpPackage.unsigned)
    ) {
      fail(`${path}.floorp_package.artifact_name does not match target`);
    }
    const rawNative = arrayValue(
      target.native_verifications,
      `${path}.native_verifications`,
    );
    const expectedNativeArchs: NativeArch[] = definition.key === "mac"
      ? ["x86_64", "aarch64"]
      : [definition.key === "linuxAarch64" ? "aarch64" : "x86_64"];
    if (rawNative.length !== expectedNativeArchs.length) {
      fail(`${path}.native_verifications has an invalid architecture count`);
    }
    const nativeVerifications = rawNative.map((raw, nativeIndex) => {
      const nativePath = `${path}.native_verifications[${nativeIndex}]`;
      const item = exactObject(
        raw,
        ["native_arch", "app_build_id", "platform_build_id", "build_id2"],
        nativePath,
      );
      const nativeArch = expectLiteral(
        item.native_arch,
        expectedNativeArchs[nativeIndex],
        `${nativePath}.native_arch`,
      );
      const appBuildId = validateBuildId(
        item.app_build_id,
        `${nativePath}.app_build_id`,
      );
      const platformBuildId = validateBuildId(
        item.platform_build_id,
        `${nativePath}.platform_build_id`,
      );
      if (
        appBuildId !== runtime.expected_build_id ||
        platformBuildId !== runtime.expected_build_id
      ) {
        fail(`${nativePath} BuildIDs do not match Runtime provenance`);
      }
      return {
        native_arch: nativeArch,
        app_build_id: appBuildId,
        platform_build_id: platformBuildId,
        build_id2: validateUuidV7(
          item.build_id2,
          `${nativePath}.build_id2`,
        ),
      };
    });
    if (
      nativeVerifications.length === 2 &&
      canonicalJson({ ...nativeVerifications[0], native_arch: "" }) !==
        canonicalJson({ ...nativeVerifications[1], native_arch: "" })
    ) {
      fail("release manifest mac native verifications disagree");
    }
    const primary = nativeVerifications[0];
    metadata[definition.key] = {
      schema_version: 2,
      version_display: versionDisplay,
      version: firefoxVersion,
      noraneko_version: floorpVersion,
      buildid: primary.app_build_id,
      noraneko_buildid: primary.build_id2,
      channel: "release",
      platform,
      arch,
      manifest_set_id: manifestSetId,
      mar,
      provenance: {
        runtime_repository: runtime.repository,
        runtime_head_sha: runtime.head_sha,
        runtime_run_id: runtime.workflow_run_id,
        runtime_artifact_id: runtimeArtifact.id,
        runtime_artifact_digest: runtimeArtifact.digest,
        floorp_repository: floorpIdentity.repository,
        floorp_head_sha: floorpIdentity.head_sha,
        floorp_run_id: floorpIdentity.workflow_run_id,
        release_tag: releaseTag,
      },
      verification: {
        status: "verified",
        method: "full-version",
        app_build_id: primary.app_build_id,
        build_id2: primary.build_id2,
      },
    };
    targets.push({
      target_key: definition.key,
      platform,
      arch,
      metadata_name: metadataName,
      metadata_sha256: metadataSha256,
      mar,
      runtime_artifact: runtimeArtifact,
      floorp_package: floorpPackage,
      native_verifications: nativeVerifications,
    });
  }
  if (
    new Set(targets.map((target) => target.floorp_package.artifact_id)).size !==
      TARGETS.length
  ) {
    fail("release manifest Floorp package artifact IDs must be unique");
  }
  if (
    new Set(targets.map((target) => target.floorp_package.artifact_digest))
      .size !== TARGETS.length
  ) {
    fail("release manifest Floorp package artifact digests must be unique");
  }
  return {
    metadata,
    manifest: {
      schema_version: 2,
      mode,
      manifest_set_id: manifestSetId,
      version_display: versionDisplay,
      version: firefoxVersion,
      noraneko_version: floorpVersion,
      channel: "release",
      runtime,
      floorp: { ...floorpIdentity, release_tag: releaseTag },
      release_files: releaseFiles,
      stub_source: stubSource,
      targets,
    },
  };
}

export async function assembleReleaseBundle(
  descriptorValue: unknown,
  outputDirectory: string,
): Promise<AssembledReleaseBundle> {
  const descriptor = parseAssemblyDescriptor(descriptorValue);
  const bundle = await prepareReleaseBundle(descriptorValue);
  await Deno.mkdir(outputDirectory, { recursive: true });
  const outputRoot = await Deno.realPath(outputDirectory);
  for (const target of TARGETS) {
    const expected = await Deno.realPath(
      `${outputRoot}/${target.marName}`,
    );
    const actual = await Deno.realPath(descriptor.mars[target.key].path);
    if (actual !== expected) {
      fail(
        `release assembly descriptor.mars.${target.key}.path must point into the flat output directory`,
      );
    }
  }
  for (
    const key of Object.keys(descriptor.release_files) as Array<
      keyof AssemblyDescriptor["release_files"]
    >
  ) {
    const file = descriptor.release_files[key];
    const expected = await Deno.realPath(`${outputRoot}/${file.name}`);
    const actual = await Deno.realPath(file.path);
    if (actual !== expected) {
      fail(
        `release assembly descriptor.release_files.${key}.path must point into the flat output directory`,
      );
    }
  }
  for (const target of TARGETS) {
    await Deno.writeTextFile(
      `${outputDirectory}/${target.metadataName}`,
      bundle.metadataFiles[target.key],
    );
  }
  await writeJson(
    `${outputDirectory}/release-manifest-set-v2.json`,
    bundle.manifest,
  );
  const expectedFiles = expectedBundleFileNames(descriptor.floorp_version);
  await Deno.writeTextFile(
    `${outputDirectory}/hashes.txt`,
    await renderHashesFile(outputDirectory, expectedFiles),
  );
  await validateReleaseBundle(
    outputDirectory,
    descriptor.mode === "production",
  );
  return bundle;
}

export async function validateReleaseBundle(
  bundleDirectory: string,
  requireProduction = false,
): Promise<ReleaseManifestSet> {
  const manifestPath = `${bundleDirectory}/release-manifest-set-v2.json`;
  const { manifest, metadata } = parseReleaseManifestSet(
    await readJson(manifestPath, "release manifest"),
  );
  if (requireProduction && manifest.mode !== "production") {
    fail("--production requires a production release manifest");
  }
  if (
    requireProduction &&
    manifest.targets.some((target) => target.floorp_package.unsigned)
  ) {
    fail("--production rejects unsigned Floorp package evidence");
  }
  const unsignedStates = new Set(
    manifest.targets.map((target) => target.floorp_package.unsigned),
  );
  if (unsignedStates.size !== 1) {
    fail("release manifest mixes signed and unsigned package evidence");
  }
  if (manifest.mode === "production" && [...unsignedStates][0]) {
    fail("production release manifest contains unsigned package evidence");
  }

  const expectedFiles = expectedBundleFileNames(manifest.noraneko_version);
  const entries: string[] = [];
  for await (const entry of Deno.readDir(bundleDirectory)) {
    if (!entry.isFile) {
      fail(`release bundle contains non-file entry ${entry.name}`);
    }
    entries.push(entry.name);
  }
  entries.sort();
  if (canonicalJson(entries) !== canonicalJson(expectedFiles)) {
    fail(
      `release bundle must contain the exact 16-file set; got ${
        entries.join(", ")
      }`,
    );
  }

  for (const target of TARGETS) {
    const manifestTarget = manifest.targets.find((item) =>
      item.target_key === target.key
    )!;
    const marHash = await hashFile(
      `${bundleDirectory}/${target.marName}`,
      "sha512",
    );
    if (
      marHash.size !== manifestTarget.mar.size ||
      marHash.digest !== manifestTarget.mar.sha512
    ) {
      fail(`${target.marName} does not match release manifest size/SHA-512`);
    }
    const expectedMetadata = `${
      JSON.stringify(metadata[target.key], null, 2)
    }\n`;
    const metadataPath = `${bundleDirectory}/${target.metadataName}`;
    let actualMetadata: string;
    try {
      actualMetadata = await Deno.readTextFile(metadataPath);
    } catch (error) {
      fail(
        `${target.metadataName} cannot be read: ${(error as Error).message}`,
      );
    }
    if (actualMetadata !== expectedMetadata) {
      fail(
        `${target.metadataName} is not the exact metadata bound by release manifest`,
      );
    }
    const metadataDigest = `sha256:${await digestBytes(
      "SHA-256",
      new TextEncoder().encode(actualMetadata),
    )}`;
    if (metadataDigest !== manifestTarget.metadata_sha256) {
      fail(`${target.metadataName} SHA-256 does not match release manifest`);
    }
  }
  for (const file of Object.values(manifest.release_files)) {
    const actual = await hashFile(
      `${bundleDirectory}/${file.name}`,
      "sha512",
    );
    if (actual.size !== file.size || actual.digest !== file.sha512) {
      fail(`${file.name} does not match release manifest size/SHA-512`);
    }
  }
  await verifyStubSourceFile(
    `${bundleDirectory}/${manifest.release_files.windowsStub.name}`,
    manifest.stub_source,
    "release manifest.stub_source",
  );
  const recomputedManifestSetId = await computeManifestSetId(metadata);
  if (recomputedManifestSetId !== manifest.manifest_set_id) {
    fail("release manifest_set_id does not match canonical metadata identity");
  }
  const expectedManifestText = `${JSON.stringify(manifest, null, 2)}\n`;
  const actualManifestText = await Deno.readTextFile(manifestPath);
  if (actualManifestText !== expectedManifestText) {
    fail("release-manifest-set-v2.json is not normalized exact JSON");
  }
  const expectedHashes = await renderHashesFile(
    bundleDirectory,
    expectedFiles,
  );
  const actualHashes = await Deno.readTextFile(
    `${bundleDirectory}/hashes.txt`,
  );
  if (actualHashes !== expectedHashes) {
    fail("hashes.txt does not match the exact flat bundle files");
  }
  return manifest;
}

const HELP = `Floorp release provenance v2

Usage:
  deno run --allow-read --allow-write tools/src/release_provenance.ts validate-runtime \\
    --manifest runtime-build-manifest-v2.json \\
    --rest-snapshot runtime-rest-snapshot-v2.json \\
    --output normalized-runtime-provenance-v2.json [--github-output PATH]

  deno run --allow-read --allow-write tools/src/release_provenance.ts record-native-verification \\
    --descriptor native-verification-descriptor-v2.json \\
    --output native-verification-v2.json [--github-output PATH]

  deno run --allow-read --allow-write tools/src/release_provenance.ts assemble-release \\
    --descriptor release-assembly-descriptor-v2.json \\
    --output-dir release-bundle [--github-output PATH]

  deno run --allow-read --allow-write tools/src/release_provenance.ts validate-release-bundle \\
    --bundle-dir release-bundle [--production] [--github-output PATH]

All JSON inputs use closed schema-v2 contracts. Unknown/missing fields, mixed
workflow identities, stale/expired artifacts, non-native evidence, unsigned
production packages, and changed file hashes are rejected. Normalized JSON is
printed to stdout; --github-output appends non-secret scalar outputs to PATH.
`;

function parseFlags(
  args: string[],
  allowed: readonly string[],
): Record<string, string> {
  const result: Record<string, string> = {};
  for (let index = 0; index < args.length; index += 2) {
    const flag = args[index];
    const value = args[index + 1];
    if (!flag?.startsWith("--") || value === undefined) {
      fail(
        `arguments must be --name value pairs; got ${flag ?? "end of input"}`,
      );
    }
    const name = flag.slice(2);
    if (!allowed.includes(name)) fail(`unknown argument --${name}`);
    if (name in result) fail(`duplicate argument --${name}`);
    result[name] = value;
  }
  return result;
}

function requiredFlag(flags: Record<string, string>, name: string): string {
  const value = flags[name];
  if (!value) fail(`missing required argument --${name}`);
  return value;
}

async function appendGithubOutputs(
  path: string | undefined,
  outputs: Record<string, string | number>,
): Promise<void> {
  if (!path) return;
  const lines: string[] = [];
  for (const [name, rawValue] of Object.entries(outputs)) {
    if (!/^[A-Za-z][A-Za-z0-9_-]*$/.test(name)) {
      fail(`invalid GitHub output name ${name}`);
    }
    const value = String(rawValue);
    if (/[\r\n]/.test(value)) fail(`GitHub output ${name} must be one line`);
    lines.push(`${name}=${value}`);
  }
  await Deno.writeTextFile(path, `${lines.join("\n")}\n`, {
    append: true,
    create: true,
  });
}

async function absoluteExistingPath(path: string): Promise<string> {
  try {
    return await Deno.realPath(path);
  } catch (error) {
    fail(`cannot resolve output path ${path}: ${(error as Error).message}`);
  }
}

async function runCli(args: string[]): Promise<void> {
  if (args.length === 0 || args[0] === "--help" || args[0] === "help") {
    console.log(HELP);
    return;
  }
  const command = args[0];
  if (args.slice(1).includes("--help")) {
    console.log(HELP);
    return;
  }
  if (command === "validate-runtime") {
    const flags = parseFlags(args.slice(1), [
      "manifest",
      "rest-snapshot",
      "output",
      "github-output",
    ]);
    const manifest = await readJson(
      requiredFlag(flags, "manifest"),
      "Runtime manifest",
    );
    const snapshot = await readJson(
      requiredFlag(flags, "rest-snapshot"),
      "REST snapshot",
    );
    const provenance = validateRuntimeProvenance(manifest, snapshot);
    const output = requiredFlag(flags, "output");
    await writeJson(output, provenance);
    const outputPath = await absoluteExistingPath(output);
    const targetOutputs: Record<string, string | number> = {};
    for (let index = 0; index < TARGETS.length; index++) {
      const key = TARGETS[index].key;
      targetOutputs[`${key}-artifact-id`] =
        provenance.targets[index].artifact_id;
      targetOutputs[`${key}-artifact-digest`] =
        provenance.targets[index].artifact_digest;
    }
    await appendGithubOutputs(flags["github-output"], {
      "provenance-path": outputPath,
      "runtime-run-id": provenance.workflow_run_id,
      "runtime-head-sha": provenance.head_sha,
      "runtime-build-id": provenance.expected_build_id,
      "runtime-manifest-artifact-id": provenance.manifest_artifact.id,
      "runtime-manifest-artifact-digest": provenance.manifest_artifact.digest,
      ...targetOutputs,
    });
    console.log(JSON.stringify(provenance));
    return;
  }
  if (command === "record-native-verification") {
    const flags = parseFlags(args.slice(1), [
      "descriptor",
      "output",
      "github-output",
    ]);
    const descriptor = await readJson(
      requiredFlag(flags, "descriptor"),
      "native descriptor",
    );
    const record = await createNativeVerificationRecord(descriptor);
    const output = requiredFlag(flags, "output");
    await writeJson(output, record);
    const outputPath = await absoluteExistingPath(output);
    await appendGithubOutputs(flags["github-output"], {
      "record-path": outputPath,
      "target-key": record.target_key,
      "native-arch": record.native_arch,
      "firefox-version": record.firefox_version,
      "app-build-id": record.app_build_id,
      "platform-build-id": record.platform_build_id,
      "build-id2": record.build_id2,
    });
    console.log(JSON.stringify(record));
    return;
  }
  if (command === "assemble-release") {
    const flags = parseFlags(args.slice(1), [
      "descriptor",
      "output-dir",
      "github-output",
    ]);
    const descriptor = await readJson(
      requiredFlag(flags, "descriptor"),
      "release descriptor",
    );
    const outputDirectory = requiredFlag(flags, "output-dir");
    const bundle = await assembleReleaseBundle(descriptor, outputDirectory);
    const bundlePath = await absoluteExistingPath(outputDirectory);
    const metadataOutputs: Record<string, string> = {};
    for (const target of TARGETS) {
      metadataOutputs[`${target.key}-metadata-path`] =
        await absoluteExistingPath(
          `${outputDirectory}/${target.metadataName}`,
        );
    }
    await appendGithubOutputs(flags["github-output"], {
      "bundle-dir": bundlePath,
      "manifest-path": await absoluteExistingPath(
        `${outputDirectory}/release-manifest-set-v2.json`,
      ),
      "manifest-set-id": bundle.manifest.manifest_set_id,
      "firefox-version": bundle.manifest.version,
      "floorp-version": bundle.manifest.noraneko_version,
      ...metadataOutputs,
    });
    console.log(JSON.stringify(bundle.manifest));
    return;
  }
  if (command === "validate-release-bundle") {
    const productionCount = args.slice(1).filter((argument) =>
      argument === "--production"
    ).length;
    if (productionCount > 1) fail("duplicate argument --production");
    const production = productionCount === 1;
    const flags = parseFlags(
      args.slice(1).filter((argument) => argument !== "--production"),
      ["bundle-dir", "github-output"],
    );
    const bundleDirectory = requiredFlag(flags, "bundle-dir");
    const manifest = await validateReleaseBundle(bundleDirectory, production);
    await appendGithubOutputs(flags["github-output"], {
      "bundle-dir": await absoluteExistingPath(bundleDirectory),
      "manifest-path": await absoluteExistingPath(
        `${bundleDirectory}/release-manifest-set-v2.json`,
      ),
      "manifest-set-id": manifest.manifest_set_id,
      "firefox-version": manifest.version,
      "floorp-version": manifest.noraneko_version,
      "validated": "true",
    });
    console.log(JSON.stringify(manifest));
    return;
  }
  fail(`unknown subcommand ${command}; use --help`);
}

if (import.meta.main) {
  try {
    await runCli(Deno.args);
  } catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    Deno.exit(1);
  }
}
