// SPDX-License-Identifier: MPL-2.0

export const RUNTIME_LOCK_PATH = new URL(
  "../../floorp-runtime.lock.json",
  import.meta.url,
);

export const RUNTIME_REPOSITORY = "Floorp-Projects/Floorp-Runtime";

export type RuntimePlatform = "linux" | "macos" | "windows";
export type RuntimeArchitecture = "aarch64" | "universal" | "x86_64";
export type RuntimeArtifactFormat = "dmg" | "tar.xz" | "zip";
export type RuntimeExtractionPolicy =
  | "dmg-floorp-app"
  | "tar-xz-floorp"
  | "zip-direct-floorp";

export interface RuntimeAsset {
  id: number;
  name: string;
  size: number;
  sha256: string;
}

export interface RuntimeArtifact {
  platform: RuntimePlatform;
  architecture: RuntimeArchitecture;
  format: RuntimeArtifactFormat;
  extractionPolicy: RuntimeExtractionPolicy;
  asset: RuntimeAsset;
  applicationIniAsset: RuntimeAsset;
  version: string;
  buildId: string;
}

export type RuntimeMaterialRole =
  | "head-support"
  | "manifest"
  | "support"
  | "test";

export interface RuntimeMaterial {
  path: string;
  role: RuntimeMaterialRole;
  bytes: number;
  mode: "100644";
  gitBlob: string;
  sha256: string;
}

export type RuntimePreference =
  | { name: string; type: "boolean"; value: boolean }
  | { name: string; type: "integer"; value: number }
  | { name: string; type: "string"; value: string };

export interface RuntimeTest {
  path: string;
  manifest: string;
  expectedTasks: number;
  headPolicy: "harness-replaced";
  supportPolicy: "locked-not-loaded";
}

export interface RuntimeTestManifest {
  path: string;
  preferences: RuntimePreference[];
  supportPaths: string[];
}

export interface RuntimeLock {
  schemaVersion: 1;
  source: {
    repository: string;
    trackingRef: string;
    ref: string;
    commit: string;
    tree: string;
    release: {
      id: number;
      immutable: boolean;
    };
    materials: {
      count: number;
      totalBytes: number;
      entries: RuntimeMaterial[];
    };
    tests: {
      count: number;
      totalTasks: number;
      supportDependencyEdges: number;
      entries: RuntimeTest[];
      manifests: RuntimeTestManifest[];
    };
  };
  artifacts: RuntimeArtifact[];
}

export class RuntimeLockValidationError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "RuntimeLockValidationError";
  }
}

type UnknownRecord = Record<string, unknown>;

interface RequiredArtifactShape {
  platform: RuntimePlatform;
  architecture: RuntimeArchitecture;
  format: RuntimeArtifactFormat;
  extractionPolicy: RuntimeExtractionPolicy;
  assetName: string;
  applicationIniAssetName: string;
}

const REQUIRED_ARTIFACTS: readonly RequiredArtifactShape[] = [
  {
    platform: "linux",
    architecture: "aarch64",
    format: "tar.xz",
    extractionPolicy: "tar-xz-floorp",
    assetName: "floorp-linux-aarch64-moz-artifact.tar.xz",
    applicationIniAssetName: "linux-aarch64-application-ini.zip",
  },
  {
    platform: "linux",
    architecture: "x86_64",
    format: "tar.xz",
    extractionPolicy: "tar-xz-floorp",
    assetName: "floorp-linux-x86_64-moz-artifact.tar.xz",
    applicationIniAssetName: "linux-x86_64-application-ini.zip",
  },
  {
    platform: "macos",
    architecture: "universal",
    format: "dmg",
    extractionPolicy: "dmg-floorp-app",
    assetName: "floorp-macOS-universal-moz-artifact.dmg",
    applicationIniAssetName: "macOS-universal-application-ini.zip",
  },
  {
    platform: "windows",
    architecture: "x86_64",
    format: "zip",
    extractionPolicy: "zip-direct-floorp",
    assetName: "floorp-windows-x86_64-moz-artifact.zip",
    applicationIniAssetName: "windows-x86_64-application-ini.zip",
  },
] as const;

const HEX_40 = /^[0-9a-f]{40}$/;
const HEX_64 = /^[0-9a-f]{64}$/;
const ASSET_NAME = /^[A-Za-z0-9._-]+$/;
const PREFERENCE_NAME = /^[A-Za-z0-9._-]+$/;
const REPOSITORY_NAME = /^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$/;
const REF_NAME = /^[A-Za-z0-9][A-Za-z0-9._/-]*$/;
const VERSION = /^(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)(?:\.(?:0|[1-9]\d*))?$/;

function fail(path: string, message: string): never {
  throw new RuntimeLockValidationError(`${path}: ${message}`);
}

function record(value: unknown, path: string): UnknownRecord {
  if (value === null || typeof value !== "object" || Array.isArray(value)) {
    fail(path, "expected an object");
  }
  return value as UnknownRecord;
}

function exactKeys(
  value: UnknownRecord,
  allowedKeys: readonly string[],
  path: string,
): void {
  const allowed = new Set(allowedKeys);
  for (const key of Object.keys(value)) {
    if (!allowed.has(key)) {
      fail(`${path}.${key}`, "unknown key");
    }
  }
  for (const key of allowedKeys) {
    if (!Object.hasOwn(value, key)) {
      fail(`${path}.${key}`, "missing required key");
    }
  }
}

function array(value: unknown, path: string): unknown[] {
  if (!Array.isArray(value)) {
    fail(path, "expected an array");
  }
  return value;
}

function string(value: unknown, path: string): string {
  if (typeof value !== "string") {
    fail(path, "expected a string");
  }
  return value;
}

function boolean(value: unknown, path: string): boolean {
  if (typeof value !== "boolean") {
    fail(path, "expected a boolean");
  }
  return value;
}

function safeInteger(
  value: unknown,
  path: string,
  minimum = 0,
): number {
  if (!Number.isSafeInteger(value) || (value as number) < minimum) {
    fail(path, `expected a safe integer >= ${minimum}`);
  }
  return value as number;
}

function int32(value: unknown, path: string): number {
  const candidate = safeInteger(value, path, -2147483648);
  if (candidate > 2147483647) {
    fail(path, "expected a signed 32-bit integer");
  }
  return candidate;
}

function literal<T extends string | number>(
  value: unknown,
  expected: T,
  path: string,
): T {
  if (value !== expected) {
    fail(path, `expected ${JSON.stringify(expected)}`);
  }
  return expected;
}

function enumValue<const T extends readonly string[]>(
  value: unknown,
  allowed: T,
  path: string,
): T[number] {
  const candidate = string(value, path);
  if (!allowed.includes(candidate)) {
    fail(path, `unsupported value ${JSON.stringify(candidate)}`);
  }
  return candidate as T[number];
}

function hex(value: unknown, pattern: RegExp, path: string): string {
  const candidate = string(value, path);
  if (!pattern.test(candidate)) {
    fail(path, "expected lowercase hexadecimal digest");
  }
  return candidate;
}

function repositoryPath(value: unknown, path: string): string {
  const candidate = string(value, path);
  if (!REPOSITORY_NAME.test(candidate) || candidate.includes("..")) {
    fail(path, "expected an owner/repository name");
  }
  return candidate;
}

function refName(value: unknown, path: string): string {
  const candidate = string(value, path);
  if (
    !REF_NAME.test(candidate) || candidate.includes("..") ||
    candidate.endsWith("/") || candidate.includes("//")
  ) {
    fail(path, "expected a safe Git ref name");
  }
  return candidate;
}

function safePath(value: unknown, path: string): string {
  const candidate = string(value, path);
  if (
    candidate.length === 0 || candidate.startsWith("/") ||
    candidate.includes("\\") || candidate.includes("\0") ||
    /[<>:"|?*]/.test(candidate) ||
    [...candidate].some((character) => character.charCodeAt(0) < 0x20) ||
    candidate.normalize("NFC") !== candidate
  ) {
    fail(path, "expected a repository-relative POSIX path");
  }
  const segments = candidate.split("/");
  if (
    segments.some((segment) =>
      segment.length === 0 || segment === "." || segment === ".." ||
      /[ .]$/.test(segment) ||
      /^(?:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)/i.test(segment)
    )
  ) {
    fail(path, "path contains an unsafe or non-portable segment");
  }
  return candidate;
}

function assetName(value: unknown, path: string): string {
  const candidate = string(value, path);
  if (!ASSET_NAME.test(candidate)) {
    fail(path, "expected a safe asset basename");
  }
  return candidate;
}

function buildId(value: unknown, path: string): string {
  const candidate = string(value, path);
  if (!/^\d{14}$/.test(candidate)) {
    fail(path, "expected a 14-digit BuildID");
  }
  const parts = [
    candidate.slice(0, 4),
    candidate.slice(4, 6),
    candidate.slice(6, 8),
    candidate.slice(8, 10),
    candidate.slice(10, 12),
    candidate.slice(12, 14),
  ].map(Number);
  const [year, month, day, hour, minute, second] = parts;
  const date = new Date(Date.UTC(year, month - 1, day, hour, minute, second));
  if (
    date.getUTCFullYear() !== year || date.getUTCMonth() !== month - 1 ||
    date.getUTCDate() !== day || date.getUTCHours() !== hour ||
    date.getUTCMinutes() !== minute || date.getUTCSeconds() !== second
  ) {
    fail(path, "BuildID contains an invalid UTC date/time");
  }
  return candidate;
}

function version(value: unknown, path: string): string {
  const candidate = string(value, path);
  if (!VERSION.test(candidate)) {
    fail(path, "expected a canonical two- or three-component numeric version");
  }
  return candidate;
}

function assertStrictlySorted(values: readonly string[], path: string): void {
  for (let index = 1; index < values.length; index += 1) {
    if (values[index - 1] >= values[index]) {
      fail(path, "entries must be strictly sorted and unique");
    }
  }
}

function assertCaseInsensitiveUnique(
  values: readonly string[],
  path: string,
): void {
  const seen = new Set<string>();
  for (const value of values) {
    const folded = value.toLowerCase();
    if (seen.has(folded)) {
      fail(path, `duplicate case-insensitive value ${JSON.stringify(value)}`);
    }
    seen.add(folded);
  }
}

function parseAsset(value: unknown, path: string): RuntimeAsset {
  const source = record(value, path);
  exactKeys(source, ["id", "name", "size", "sha256"], path);
  return {
    id: safeInteger(source.id, `${path}.id`, 1),
    name: assetName(source.name, `${path}.name`),
    size: safeInteger(source.size, `${path}.size`, 1),
    sha256: hex(source.sha256, HEX_64, `${path}.sha256`),
  };
}

function parseArtifact(value: unknown, path: string): RuntimeArtifact {
  const source = record(value, path);
  exactKeys(
    source,
    [
      "platform",
      "architecture",
      "format",
      "extractionPolicy",
      "asset",
      "applicationIniAsset",
      "version",
      "buildId",
    ],
    path,
  );
  return {
    platform: enumValue(
      source.platform,
      ["linux", "macos", "windows"] as const,
      `${path}.platform`,
    ),
    architecture: enumValue(
      source.architecture,
      ["aarch64", "universal", "x86_64"] as const,
      `${path}.architecture`,
    ),
    format: enumValue(
      source.format,
      ["dmg", "tar.xz", "zip"] as const,
      `${path}.format`,
    ),
    extractionPolicy: enumValue(
      source.extractionPolicy,
      ["dmg-floorp-app", "tar-xz-floorp", "zip-direct-floorp"] as const,
      `${path}.extractionPolicy`,
    ),
    asset: parseAsset(source.asset, `${path}.asset`),
    applicationIniAsset: parseAsset(
      source.applicationIniAsset,
      `${path}.applicationIniAsset`,
    ),
    version: version(source.version, `${path}.version`),
    buildId: buildId(source.buildId, `${path}.buildId`),
  };
}

function parseMaterial(value: unknown, path: string): RuntimeMaterial {
  const source = record(value, path);
  exactKeys(
    source,
    ["path", "role", "bytes", "mode", "gitBlob", "sha256"],
    path,
  );
  return {
    path: safePath(source.path, `${path}.path`),
    role: enumValue(
      source.role,
      ["head-support", "manifest", "support", "test"] as const,
      `${path}.role`,
    ),
    bytes: safeInteger(source.bytes, `${path}.bytes`),
    mode: literal(source.mode, "100644", `${path}.mode`),
    gitBlob: hex(source.gitBlob, HEX_40, `${path}.gitBlob`),
    sha256: hex(source.sha256, HEX_64, `${path}.sha256`),
  };
}

function parsePreference(value: unknown, path: string): RuntimePreference {
  const source = record(value, path);
  exactKeys(source, ["name", "type", "value"], path);
  const name = string(source.name, `${path}.name`);
  if (!PREFERENCE_NAME.test(name)) {
    fail(`${path}.name`, "expected a Firefox preference name");
  }
  const type = enumValue(
    source.type,
    ["boolean", "integer", "string"] as const,
    `${path}.type`,
  );
  switch (type) {
    case "boolean":
      return { name, type, value: boolean(source.value, `${path}.value`) };
    case "integer":
      return {
        name,
        type,
        value: int32(source.value, `${path}.value`),
      };
    case "string":
      return { name, type, value: string(source.value, `${path}.value`) };
  }
}

function parseTest(value: unknown, path: string): RuntimeTest {
  const source = record(value, path);
  exactKeys(
    source,
    [
      "path",
      "manifest",
      "expectedTasks",
      "headPolicy",
      "supportPolicy",
    ],
    path,
  );
  return {
    path: safePath(source.path, `${path}.path`),
    manifest: safePath(source.manifest, `${path}.manifest`),
    expectedTasks: safeInteger(
      source.expectedTasks,
      `${path}.expectedTasks`,
      1,
    ),
    headPolicy: literal(
      source.headPolicy,
      "harness-replaced",
      `${path}.headPolicy`,
    ),
    supportPolicy: literal(
      source.supportPolicy,
      "locked-not-loaded",
      `${path}.supportPolicy`,
    ),
  };
}

function parseTestManifest(
  value: unknown,
  path: string,
): RuntimeTestManifest {
  const source = record(value, path);
  exactKeys(source, ["path", "preferences", "supportPaths"], path);
  const preferences = array(source.preferences, `${path}.preferences`).map(
    (preference, index) =>
      parsePreference(preference, `${path}.preferences[${index}]`),
  );
  const supportPaths = array(source.supportPaths, `${path}.supportPaths`).map(
    (supportPath, index) =>
      safePath(supportPath, `${path}.supportPaths[${index}]`),
  );
  assertStrictlySorted(
    preferences.map((preference) => preference.name),
    `${path}.preferences`,
  );
  assertStrictlySorted(supportPaths, `${path}.supportPaths`);
  assertCaseInsensitiveUnique(supportPaths, `${path}.supportPaths`);
  return {
    path: safePath(source.path, `${path}.path`),
    preferences,
    supportPaths,
  };
}

function parseMaterials(
  value: unknown,
  path: string,
): RuntimeLock["source"]["materials"] {
  const source = record(value, path);
  exactKeys(source, ["count", "totalBytes", "entries"], path);
  const entries = array(source.entries, `${path}.entries`).map(
    (entry, index) => parseMaterial(entry, `${path}.entries[${index}]`),
  );
  const count = safeInteger(source.count, `${path}.count`);
  const totalBytes = safeInteger(source.totalBytes, `${path}.totalBytes`);
  if (count !== entries.length) {
    fail(`${path}.count`, `expected ${entries.length}`);
  }
  const computedBytes = entries.reduce((sum, entry) => sum + entry.bytes, 0);
  if (!Number.isSafeInteger(computedBytes) || totalBytes !== computedBytes) {
    fail(`${path}.totalBytes`, `expected ${computedBytes}`);
  }
  const paths = entries.map((entry) => entry.path);
  assertStrictlySorted(paths, `${path}.entries`);
  assertCaseInsensitiveUnique(paths, `${path}.entries`);
  for (const entry of entries) {
    if (entry.role === "head-support" && !entry.path.endsWith("/head.js")) {
      fail(`${path}.entries`, `head-support is not head.js: ${entry.path}`);
    }
  }
  return { count, totalBytes, entries };
}

function parseTests(
  value: unknown,
  path: string,
): RuntimeLock["source"]["tests"] {
  const source = record(value, path);
  exactKeys(
    source,
    [
      "count",
      "totalTasks",
      "supportDependencyEdges",
      "entries",
      "manifests",
    ],
    path,
  );
  const entries = array(source.entries, `${path}.entries`).map(
    (entry, index) => parseTest(entry, `${path}.entries[${index}]`),
  );
  const manifests = array(source.manifests, `${path}.manifests`).map(
    (manifest, index) =>
      parseTestManifest(manifest, `${path}.manifests[${index}]`),
  );
  const count = safeInteger(source.count, `${path}.count`);
  const totalTasks = safeInteger(source.totalTasks, `${path}.totalTasks`);
  const supportDependencyEdges = safeInteger(
    source.supportDependencyEdges,
    `${path}.supportDependencyEdges`,
  );
  if (count !== entries.length) {
    fail(`${path}.count`, `expected ${entries.length}`);
  }
  const computedTasks = entries.reduce(
    (sum, entry) => sum + entry.expectedTasks,
    0,
  );
  if (totalTasks !== computedTasks) {
    fail(`${path}.totalTasks`, `expected ${computedTasks}`);
  }
  assertStrictlySorted(
    entries.map((entry) => entry.path),
    `${path}.entries`,
  );
  assertCaseInsensitiveUnique(
    entries.map((entry) => entry.path),
    `${path}.entries`,
  );
  assertStrictlySorted(
    manifests.map((manifest) => manifest.path),
    `${path}.manifests`,
  );
  assertCaseInsensitiveUnique(
    manifests.map((manifest) => manifest.path),
    `${path}.manifests`,
  );
  return {
    count,
    totalTasks,
    supportDependencyEdges,
    entries,
    manifests,
  };
}

function assertSetEqual(
  actual: ReadonlySet<string>,
  expected: ReadonlySet<string>,
  path: string,
): void {
  const missing = [...expected].filter((value) => !actual.has(value));
  const extra = [...actual].filter((value) => !expected.has(value));
  if (missing.length > 0 || extra.length > 0) {
    fail(
      path,
      `set mismatch (missing: ${missing.join(", ") || "none"}; extra: ${
        extra.join(", ") || "none"
      })`,
    );
  }
}

function validateTestMaterialClosure(
  materials: RuntimeLock["source"]["materials"],
  tests: RuntimeLock["source"]["tests"],
): void {
  const materialsByPath = new Map(
    materials.entries.map((entry) => [entry.path, entry]),
  );
  const testPaths = new Set(tests.entries.map((entry) => entry.path));
  const materialTestPaths = new Set(
    materials.entries.filter((entry) => entry.role === "test").map((entry) =>
      entry.path
    ),
  );
  assertSetEqual(testPaths, materialTestPaths, "$.source.tests.entries");

  const manifestPaths = new Set(tests.manifests.map((entry) => entry.path));
  const materialManifestPaths = new Set(
    materials.entries.filter((entry) => entry.role === "manifest").map(
      (entry) => entry.path,
    ),
  );
  assertSetEqual(
    manifestPaths,
    materialManifestPaths,
    "$.source.tests.manifests",
  );

  const supportPaths = new Set(
    tests.manifests.flatMap((manifest) => manifest.supportPaths),
  );
  const materialSupportPaths = new Set(
    materials.entries.filter((entry) =>
      entry.role === "support" || entry.role === "head-support"
    ).map((entry) => entry.path),
  );
  assertSetEqual(
    supportPaths,
    materialSupportPaths,
    "$.source.tests.manifests.supportPaths",
  );

  const testsPerManifest = new Map<string, number>();
  for (const test of tests.entries) {
    const material = materialsByPath.get(test.path);
    if (material?.role !== "test") {
      fail("$.source.tests.entries", `test material missing: ${test.path}`);
    }
    if (!manifestPaths.has(test.manifest)) {
      fail(
        "$.source.tests.entries",
        `manifest is not locked: ${test.manifest}`,
      );
    }
    testsPerManifest.set(
      test.manifest,
      (testsPerManifest.get(test.manifest) ?? 0) + 1,
    );
  }
  for (const manifest of tests.manifests) {
    if (!testsPerManifest.has(manifest.path)) {
      fail(
        "$.source.tests.manifests",
        `manifest is not used by a locked test: ${manifest.path}`,
      );
    }
    for (const supportPath of manifest.supportPaths) {
      const material = materialsByPath.get(supportPath);
      if (material?.role !== "support" && material?.role !== "head-support") {
        fail(
          "$.source.tests.manifests.supportPaths",
          `support material missing: ${supportPath}`,
        );
      }
    }
  }
  const computedEdges = tests.manifests.reduce(
    (sum, manifest) =>
      sum +
      manifest.supportPaths.length * (testsPerManifest.get(manifest.path) ?? 0),
    0,
  );
  if (tests.supportDependencyEdges !== computedEdges) {
    fail(
      "$.source.tests.supportDependencyEdges",
      `expected ${computedEdges}`,
    );
  }
}

function validateArtifacts(artifacts: RuntimeArtifact[]): void {
  if (artifacts.length !== REQUIRED_ARTIFACTS.length) {
    fail("$.artifacts", `expected ${REQUIRED_ARTIFACTS.length} entries`);
  }
  const tupleKeys = artifacts.map((artifact) =>
    `${artifact.platform}/${artifact.architecture}`
  );
  assertStrictlySorted(tupleKeys, "$.artifacts");
  assertCaseInsensitiveUnique(tupleKeys, "$.artifacts");

  for (const required of REQUIRED_ARTIFACTS) {
    const tuple = `${required.platform}/${required.architecture}`;
    const artifact = artifacts.find((candidate) =>
      candidate.platform === required.platform &&
      candidate.architecture === required.architecture
    );
    if (artifact === undefined) {
      fail("$.artifacts", `missing required platform tuple ${tuple}`);
    }
    if (
      artifact.format !== required.format ||
      artifact.extractionPolicy !== required.extractionPolicy
    ) {
      fail("$.artifacts", `unexpected format/extraction policy for ${tuple}`);
    }
    if (
      artifact.asset.name !== required.assetName ||
      artifact.applicationIniAsset.name !== required.applicationIniAssetName
    ) {
      fail("$.artifacts", `unexpected asset names for ${tuple}`);
    }
  }

  const assets = artifacts.flatMap((artifact) => [
    artifact.asset,
    artifact.applicationIniAsset,
  ]);
  const assetIds = assets.map((asset) => String(asset.id)).sort();
  const assetNames = assets.map((asset) => asset.name).sort();
  assertStrictlySorted(assetIds, "$.artifacts asset IDs");
  assertCaseInsensitiveUnique(assetNames, "$.artifacts asset names");
  if (new Set(artifacts.map((artifact) => artifact.version)).size !== 1) {
    fail("$.artifacts", "all platform artifacts must share one version");
  }
}

export function parseRuntimeLock(value: unknown): RuntimeLock {
  const root = record(value, "$");
  exactKeys(root, ["schemaVersion", "source", "artifacts"], "$");
  const schemaVersion = literal(root.schemaVersion, 1, "$.schemaVersion");

  const sourceRecord = record(root.source, "$.source");
  exactKeys(
    sourceRecord,
    [
      "repository",
      "trackingRef",
      "ref",
      "commit",
      "tree",
      "release",
      "materials",
      "tests",
    ],
    "$.source",
  );
  const repository = repositoryPath(
    sourceRecord.repository,
    "$.source.repository",
  );
  if (repository !== RUNTIME_REPOSITORY) {
    fail("$.source.repository", `expected ${RUNTIME_REPOSITORY}`);
  }
  const releaseRecord = record(sourceRecord.release, "$.source.release");
  exactKeys(releaseRecord, ["id", "immutable"], "$.source.release");
  const materials = parseMaterials(
    sourceRecord.materials,
    "$.source.materials",
  );
  const tests = parseTests(sourceRecord.tests, "$.source.tests");
  validateTestMaterialClosure(materials, tests);

  const artifacts = array(root.artifacts, "$.artifacts").map(
    (artifact, index) => parseArtifact(artifact, `$.artifacts[${index}]`),
  );
  validateArtifacts(artifacts);

  return {
    schemaVersion,
    source: {
      repository,
      trackingRef: refName(
        sourceRecord.trackingRef,
        "$.source.trackingRef",
      ),
      ref: refName(sourceRecord.ref, "$.source.ref"),
      commit: hex(sourceRecord.commit, HEX_40, "$.source.commit"),
      tree: hex(sourceRecord.tree, HEX_40, "$.source.tree"),
      release: {
        id: safeInteger(releaseRecord.id, "$.source.release.id", 1),
        immutable: boolean(
          releaseRecord.immutable,
          "$.source.release.immutable",
        ),
      },
      materials,
      tests,
    },
    artifacts,
  };
}

export async function loadRuntimeLock(
  path: string | URL = RUNTIME_LOCK_PATH,
): Promise<RuntimeLock> {
  const text = await Deno.readTextFile(path);
  let value: unknown;
  try {
    value = JSON.parse(text);
  } catch (error: unknown) {
    const detail = error instanceof Error ? error.message : String(error);
    throw new RuntimeLockValidationError(
      `invalid runtime lock JSON: ${detail}`,
    );
  }
  return parseRuntimeLock(value);
}
