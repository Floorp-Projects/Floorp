// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { createHash } from "node:crypto";
import {
  BIN_DIR,
  BIN_PATH_EXE,
  BIN_ROOT_DIR,
  BIN_VERSION,
  BRANDING,
  getBinArchive,
  PATHS,
  PLATFORM,
  VERSION,
} from "./defines.ts";
import type { BinArchive, Platform } from "./defines.ts";
import {
  loadRuntimeLock,
  type RuntimeArtifact,
  type RuntimeLock,
  type RuntimePlatform,
} from "./runtime_lock.ts";
import {
  assertSafeFilesystemTree,
  copyDirectoryTreeSafely,
  extractTarXzSafely,
  extractZipSafely,
  findSingleTopLevelAppDirectory,
} from "./runtime_archive.ts";
import { exists, Logger, runCommand, safeRemove } from "./utils.ts";

const logger = new Logger("initializer");

const RUNTIME_BASE_URL = "https://dev-assets.floorp.app/floorp-runtime-builds/";
const RUNTIME_INDEX_URL = `${RUNTIME_BASE_URL}.ftp-deploy-sync-state.json`;

export const LOCKED_RUNTIME_GITHUB_TOKEN_ENV = "FLOORP_RUNTIME_GITHUB_TOKEN";
export const LOCKED_RUNTIME_MARKER = ".floorp-runtime-lock.json";

export type LockedRuntimeFetch = (
  input: string | URL | Request,
  init?: RequestInit,
) => Promise<Response>;

export interface LockedRuntimeNetworkOptions {
  githubToken?: string;
  fetchImpl?: LockedRuntimeFetch;
}

export interface NativeRuntimeTarget {
  platform: RuntimePlatform;
  architecture: RuntimeArtifact["architecture"];
}

export interface LockedReleaseAssetMetadata {
  id: number;
  name: string;
  size: number;
  digest: string;
  browserDownloadUrl: string;
}

export interface LockedReleaseMetadata {
  id: number;
  tagName: string;
  immutable: boolean;
  assets: LockedReleaseAssetMetadata[];
}

export interface LockedRuntimeOperations {
  fetchRelease(lock: RuntimeLock): Promise<LockedReleaseMetadata>;
  download(url: string, destination: string): Promise<void>;
  sha256(filePath: string): Promise<string>;
  extractMain(
    artifact: RuntimeArtifact,
    archivePath: string,
    destinationRoot: string,
  ): Promise<void>;
  extractCompanion(
    archivePath: string,
    destinationRoot: string,
  ): Promise<void>;
  rename(from: string, to: string): Promise<void>;
  remove(filePath: string): Promise<void>;
  nonce(): string;
}

export interface LockedRuntimeInstallOptions
  extends LockedRuntimeNetworkOptions {
  lock: RuntimeLock;
  target?: NativeRuntimeTarget;
  binRootDir?: string;
  profileDir?: string;
  operations?: Partial<LockedRuntimeOperations>;
}

export interface LockedRuntimeInstallResult {
  artifact: RuntimeArtifact;
  reused: boolean;
  backupPath?: string;
  controlBackupPath?: string;
}

export interface InitializerRunDependencies {
  loadLock(): Promise<RuntimeLock>;
  installLock(lock: RuntimeLock): Promise<void>;
  installDebug?(): Promise<void>;
  savePrefs(): void;
}

export type RuntimeDistribution = "release" | "debug";

export interface InitializerRunOptions
  extends Partial<InitializerRunDependencies> {
  distribution?: RuntimeDistribution;
}

interface LockedRuntimeMarker {
  schemaVersion: 1;
  sourceRef: string;
  sourceCommit: string;
  sourceTree: string;
  releaseId: number;
  artifactId: number;
  artifactName: string;
  artifactSha256: string;
  applicationIniAssetId: number;
  applicationIniAssetName: string;
  applicationIniAssetSha256: string;
  platform: RuntimePlatform;
  architecture: RuntimeArtifact["architecture"];
  version: string;
  buildId: string;
}

interface RuntimeLayout {
  applicationIni: string;
  executable: string;
  legacyVersionFile: string;
}

export const DEBUG_RUNTIME_MARKER = ".floorp-runtime-debug.json";

interface DebugRuntimeMarker {
  schemaVersion: 1;
  distribution: "debug";
  platform: RuntimePlatform;
  architecture: RuntimeArtifact["architecture"];
  artifactName: string;
  artifactSize: number;
  artifactSha256: string;
  version: string;
  buildId: string;
}

export const isEnvironmentPermissionError = (error: unknown): boolean =>
  error instanceof Deno.errors.PermissionDenied ||
  error instanceof Deno.errors.NotCapable;

export function resolveNativeRuntimeTarget(
  os: string,
  architecture: string,
): NativeRuntimeTarget {
  if (os === "windows" && architecture === "x86_64") {
    return { platform: "windows", architecture: "x86_64" };
  }
  if (
    os === "linux" &&
    (architecture === "x86_64" || architecture === "aarch64")
  ) {
    return { platform: "linux", architecture };
  }
  if (os === "darwin") {
    return { platform: "macos", architecture: "universal" };
  }
  throw new Error(
    `Locked Runtime is not available for ${os}/${architecture}.`,
  );
}

export function selectLockedRuntimeArtifact(
  lock: RuntimeLock,
  target: NativeRuntimeTarget,
): RuntimeArtifact {
  const matches = lock.artifacts.filter(
    (artifact) =>
      artifact.platform === target.platform &&
      artifact.architecture === target.architecture,
  );
  if (matches.length !== 1) {
    throw new Error(
      `Expected exactly one locked Runtime artifact for ${target.platform}/${target.architecture}; found ${matches.length}.`,
    );
  }
  return matches[0];
}

export function runtimeLayoutFor(
  binRootDir: string,
  artifact: RuntimeArtifact,
): RuntimeLayout {
  const floorpRoot = path.join(binRootDir, BRANDING.base_name);
  switch (artifact.platform) {
    case "windows":
      return {
        applicationIni: path.join(floorpRoot, "application.ini"),
        executable: path.join(floorpRoot, `${BRANDING.base_name}.exe`),
        legacyVersionFile: path.join(floorpRoot, "nora.version.txt"),
      };
    case "linux":
      return {
        applicationIni: path.join(floorpRoot, "application.ini"),
        executable: path.join(floorpRoot, `${BRANDING.base_name}-bin`),
        legacyVersionFile: path.join(floorpRoot, "nora.version.txt"),
      };
    case "macos": {
      const appRoot = path.join(
        floorpRoot,
        `${BRANDING.display_name}.app`,
        "Contents",
      );
      return {
        applicationIni: path.join(appRoot, "Resources", "application.ini"),
        executable: path.join(appRoot, "MacOS", BRANDING.base_name),
        legacyVersionFile: path.join(
          appRoot,
          "Resources",
          "nora.version.txt",
        ),
      };
    }
  }
}

export function parseApplicationIni(
  content: string,
): { name?: string; version: string; buildId: string } {
  let section = "";
  let name: string | undefined;
  let version: string | undefined;
  let buildId: string | undefined;
  for (const rawLine of content.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith(";") || line.startsWith("#")) continue;
    const sectionMatch = line.match(/^\[([^\]]+)\]$/);
    if (sectionMatch) {
      section = sectionMatch[1];
      continue;
    }
    if (section !== "App") continue;
    const separator = line.indexOf("=");
    if (separator < 1) continue;
    const key = line.slice(0, separator).trim();
    const value = line.slice(separator + 1).trim();
    if (key === "Name") name = value;
    if (key === "Version") version = value;
    if (key === "BuildID") buildId = value;
  }
  if (!version || !buildId) {
    throw new Error(
      "application.ini is missing [App] Version or BuildID.",
    );
  }
  return { name, version, buildId };
}

const LOCKED_MAC_DEVELOPER_KEYS = [
  "MozillaDeveloperRepoPath",
  "MozillaDeveloperObjPath",
] as const;

const escapeRegExp = (value: string): string =>
  value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

const escapeXmlText = (value: string): string =>
  value
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;");

function plistKeyPattern(key: string): RegExp {
  return new RegExp(`<key>\\s*${escapeRegExp(key)}\\s*</key>`, "g");
}

function plistStringPairPattern(key: string, captureValue = false): RegExp {
  return new RegExp(
    `<key>\\s*${escapeRegExp(key)}\\s*</key>\\s*<string>${
      captureValue ? "([^<]*)" : "[^<]*"
    }</string>`,
    "g",
  );
}

function setSinglePlistString(
  content: string,
  key: string,
  escapedValue: string,
): string {
  const keyMatches = content.match(plistKeyPattern(key)) ?? [];
  if (keyMatches.length > 1) {
    throw new Error(`macOS Info.plist contains duplicate ${key} keys.`);
  }
  const replacement = `<key>${key}</key>\n\t<string>${escapedValue}</string>`;
  if (keyMatches.length === 1) {
    const pairMatches = content.match(plistStringPairPattern(key)) ?? [];
    if (pairMatches.length !== 1) {
      throw new Error(
        `macOS Info.plist ${key} is not represented by exactly one string value.`,
      );
    }
    return content.replace(plistStringPairPattern(key), () => replacement);
  }

  const insertIndex = content.lastIndexOf("</dict>");
  if (insertIndex === -1) {
    throw new Error("macOS Info.plist is missing its closing </dict> element.");
  }
  return content.slice(0, insertIndex) + `\n\t${replacement}\n` +
    content.slice(insertIndex);
}

async function assertLockedMacInfoPlist(
  treeRoot: string,
  binRootDir: string,
  artifact: RuntimeArtifact,
  forbiddenTreeRoot?: string,
): Promise<void> {
  if (artifact.platform !== "macos") return;
  if (artifact.extractionPolicy !== "dmg-floorp-app") {
    throw new Error(
      `Locked macOS Runtime has unexpected extraction policy ${artifact.extractionPolicy}.`,
    );
  }

  const contentsRoot = path.dirname(
    path.dirname(runtimeLayoutFor(treeRoot, artifact).applicationIni),
  );
  const infoPlist = path.join(contentsRoot, "Info.plist");
  const info = await Deno.lstat(infoPlist);
  if (!info.isFile || info.isSymlink) {
    throw new Error(
      `Locked macOS Info.plist is not a regular file: ${infoPlist}`,
    );
  }

  const finalDeveloperPath = path.join(binRootDir, BRANDING.base_name);
  const escapedFinalPath = escapeXmlText(finalDeveloperPath);
  const verifiedContent = await Deno.readTextFile(infoPlist);
  for (const key of LOCKED_MAC_DEVELOPER_KEYS) {
    const values = [
      ...verifiedContent.matchAll(plistStringPairPattern(key, true)),
    ]
      .map((match) => match[1]);
    if (values.length !== 1 || values[0] !== escapedFinalPath) {
      throw new Error(
        `Locked macOS Info.plist ${key} did not verify as the final Runtime path ${finalDeveloperPath}.`,
      );
    }
  }
  if (forbiddenTreeRoot) {
    const forbiddenDeveloperPath = path.join(
      forbiddenTreeRoot,
      BRANDING.base_name,
    );
    if (
      verifiedContent.includes(forbiddenDeveloperPath) ||
      verifiedContent.includes(escapeXmlText(forbiddenDeveloperPath))
    ) {
      throw new Error(
        `Locked macOS Info.plist still contains staging path ${forbiddenDeveloperPath}.`,
      );
    }
  }
}

async function prepareLockedMacInfoPlist(
  stageRoot: string,
  binRootDir: string,
  artifact: RuntimeArtifact,
): Promise<void> {
  if (artifact.platform !== "macos") return;
  const contentsRoot = path.dirname(
    path.dirname(runtimeLayoutFor(stageRoot, artifact).applicationIni),
  );
  const infoPlist = path.join(contentsRoot, "Info.plist");
  const info = await Deno.lstat(infoPlist);
  if (!info.isFile || info.isSymlink) {
    throw new Error(
      `Locked macOS Info.plist is not a regular file: ${infoPlist}`,
    );
  }
  const finalDeveloperPath = path.join(binRootDir, BRANDING.base_name);
  const escapedFinalPath = escapeXmlText(finalDeveloperPath);
  let content = await Deno.readTextFile(infoPlist);
  for (const key of LOCKED_MAC_DEVELOPER_KEYS) {
    content = setSinglePlistString(content, key, escapedFinalPath);
  }
  await Deno.writeTextFile(infoPlist, content);
  await assertLockedMacInfoPlist(
    stageRoot,
    binRootDir,
    artifact,
    stageRoot,
  );
}

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === "object" && value !== null && !Array.isArray(value);

const normalizeSha256 = (value: string): string =>
  value.toLowerCase().replace(/^sha256:/, "");

function githubRepositoryApiPath(repository: string): string {
  const repositoryParts = repository.split("/");
  if (repositoryParts.length !== 2 || repositoryParts.some((part) => !part)) {
    throw new Error(`Invalid locked Runtime repository: ${repository}`);
  }
  return repositoryParts.map(encodeURIComponent).join("/");
}

function isPlainLockedRuntimeAssetName(assetName: string): boolean {
  return assetName.length > 0 && assetName !== "." && assetName !== ".." &&
    !assetName.includes("/") && !assetName.includes("\\") &&
    !path.isAbsolute(assetName);
}

export function lockedReleasePublicDownloadUrl(
  repository: string,
  ref: string,
  assetName: string,
): string {
  const repositoryPath = githubRepositoryApiPath(repository);
  if (!ref || ref === "." || ref === "..") {
    throw new Error("Invalid locked Runtime release ref.");
  }
  if (!isPlainLockedRuntimeAssetName(assetName)) {
    throw new Error("Locked Runtime asset names must be plain file names.");
  }
  return `https://github.com/${repositoryPath}/releases/download/${
    encodeURIComponent(ref)
  }/${encodeURIComponent(assetName)}`;
}

function parseReleaseMetadata(value: unknown): LockedReleaseMetadata {
  if (!isRecord(value) || !Array.isArray(value.assets)) {
    throw new Error("GitHub release metadata has an invalid shape.");
  }
  const assets = value.assets.map((entry, index) => {
    if (
      !isRecord(entry) ||
      typeof entry.id !== "number" ||
      typeof entry.name !== "string" ||
      typeof entry.size !== "number" ||
      typeof entry.digest !== "string" ||
      typeof entry.browser_download_url !== "string"
    ) {
      throw new Error(`GitHub release asset ${index} has an invalid shape.`);
    }
    return {
      id: entry.id,
      name: entry.name,
      size: entry.size,
      digest: entry.digest,
      browserDownloadUrl: entry.browser_download_url,
    };
  });
  if (
    typeof value.id !== "number" ||
    typeof value.tag_name !== "string" ||
    typeof value.immutable !== "boolean"
  ) {
    throw new Error("GitHub release identity metadata is incomplete.");
  }
  return {
    id: value.id,
    tagName: value.tag_name,
    immutable: value.immutable,
    assets,
  };
}

const REDIRECT_STATUSES = new Set([301, 302, 303, 307, 308]);
const MAX_LOCKED_RUNTIME_REDIRECTS = 5;

function normalizedGitHubToken(value: string | undefined): string | undefined {
  const token = value?.trim();
  if (!token) return undefined;
  const hasInvalidCharacter = [...token].some((character) => {
    const codePoint = character.codePointAt(0) ?? 0;
    return codePoint <= 0x20 || codePoint === 0x7f ||
      character.trim().length === 0;
  });
  if (hasInvalidCharacter) {
    throw new Error("Locked Runtime GitHub token has an invalid format.");
  }
  return token;
}

function githubApiHeaders(
  accept: string,
  githubToken: string | undefined,
): Headers {
  const headers = new Headers({
    Accept: accept,
    "X-GitHub-Api-Version": "2022-11-28",
  });
  const token = normalizedGitHubToken(githubToken);
  if (token) headers.set("Authorization", `Bearer ${token}`);
  return headers;
}

async function fetchLockedReleaseMetadata(
  lock: RuntimeLock,
  options: LockedRuntimeNetworkOptions = {},
): Promise<LockedReleaseMetadata> {
  const repository = githubRepositoryApiPath(lock.source.repository);
  const url =
    `https://api.github.com/repos/${repository}/releases/${lock.source.release.id}`;
  let response: Response;
  try {
    response = await (options.fetchImpl ?? globalThis.fetch)(url, {
      headers: githubApiHeaders(
        "application/vnd.github+json",
        options.githubToken,
      ),
      redirect: "manual",
    });
  } catch {
    throw new Error("Locked Runtime release metadata request failed.");
  }
  if (REDIRECT_STATUSES.has(response.status)) {
    throw new Error(
      "Unexpected redirect while fetching locked Runtime release metadata.",
    );
  }
  if (!response.ok) {
    throw new Error(
      `HTTP ${response.status} while fetching locked Runtime release metadata.`,
    );
  }
  return parseReleaseMetadata(await response.json());
}

async function downloadLockedFile(
  url: string,
  destination: string,
  fetchImpl: LockedRuntimeFetch = globalThis.fetch,
): Promise<void> {
  let currentUrl: URL;
  try {
    currentUrl = new URL(url);
  } catch {
    throw new Error("Locked Runtime asset URL is invalid.");
  }
  if (currentUrl.protocol !== "https:") {
    throw new Error("Locked Runtime asset URL must use HTTPS.");
  }

  let response: Response | undefined;
  for (
    let redirectCount = 0;
    redirectCount <= MAX_LOCKED_RUNTIME_REDIRECTS;
    redirectCount += 1
  ) {
    try {
      response = await fetchImpl(currentUrl, {
        headers: { Accept: "application/octet-stream" },
        redirect: "manual",
      });
    } catch {
      throw new Error("Locked Runtime asset download request failed.");
    }
    if (!REDIRECT_STATUSES.has(response.status)) break;
    if (redirectCount === MAX_LOCKED_RUNTIME_REDIRECTS) {
      throw new Error("Locked Runtime asset download exceeded redirect limit.");
    }
    const location = response.headers.get("location");
    if (!location) {
      throw new Error("Locked Runtime asset redirect has no location.");
    }
    let nextUrl: URL;
    try {
      nextUrl = new URL(location, currentUrl);
    } catch {
      throw new Error("Locked Runtime asset redirect location is invalid.");
    }
    if (nextUrl.protocol !== "https:") {
      throw new Error("Locked Runtime asset redirect must use HTTPS.");
    }
    currentUrl = nextUrl;
  }
  if (!response) {
    throw new Error("Locked Runtime asset download produced no response.");
  }
  if (!response.ok) {
    throw new Error(
      `HTTP ${response.status} while downloading locked Runtime asset.`,
    );
  }
  await Deno.mkdir(path.dirname(destination), { recursive: true });
  const output = await Deno.open(destination, {
    createNew: true,
    write: true,
  });
  try {
    if (!response.body) {
      throw new Error("Locked Runtime asset download has no body.");
    }
    await response.body.pipeTo(output.writable);
  } catch {
    try {
      output.close();
    } catch {
      // pipeTo closes the writable in the normal and error paths.
    }
    throw new Error("Failed while writing locked Runtime asset.");
  }
}

async function sha256File(filePath: string): Promise<string> {
  const hash = createHash("sha256");
  const file = await Deno.open(filePath, { read: true });
  try {
    for await (const chunk of file.readable) hash.update(chunk);
  } finally {
    try {
      file.close();
    } catch {
      // Iterating the readable closes the file.
    }
  }
  return hash.digest("hex");
}

async function extractLockedMainArchive(
  artifact: RuntimeArtifact,
  archivePath: string,
  destinationRoot: string,
): Promise<void> {
  switch (artifact.extractionPolicy) {
    case "zip-direct-floorp":
      await extractZipSafely(
        archivePath,
        destinationRoot,
        BRANDING.base_name,
      );
      return;
    case "tar-xz-floorp":
      await extractTarXzSafely(
        archivePath,
        destinationRoot,
        BRANDING.base_name,
      );
      return;
    case "dmg-floorp-app": {
      const mountPoint = await Deno.makeTempDir({
        dir: path.dirname(destinationRoot),
        prefix: "runtime-dmg-mount-",
      });
      let attached = false;
      try {
        runCommand("hdiutil", [
          "attach",
          "-readonly",
          "-nobrowse",
          "-quiet",
          "-mountpoint",
          mountPoint,
          archivePath,
        ]);
        attached = true;
        const floorpRoot = path.join(destinationRoot, BRANDING.base_name);
        await Deno.mkdir(floorpRoot, { recursive: true });
        const expectedAppName = `${BRANDING.display_name}.app`;
        const sourceApp = await findSingleTopLevelAppDirectory(
          mountPoint,
          expectedAppName,
        );
        await copyDirectoryTreeSafely(
          sourceApp,
          path.join(floorpRoot, expectedAppName),
        );
        try {
          runCommand("xattr", ["-rc", destinationRoot]);
        } catch {
          // xattr is advisory for local development artifacts.
        }
      } finally {
        try {
          if (attached) {
            runCommand("hdiutil", ["detach", "-quiet", mountPoint]);
          }
        } finally {
          await Deno.remove(mountPoint, { recursive: true }).catch(() => {});
        }
      }
      return;
    }
    default:
      throw new Error(
        `Unsupported locked Runtime extraction policy: ${artifact.extractionPolicy}`,
      );
  }
}

const defaultLockedRuntimeOperations = (
  networkOptions: LockedRuntimeNetworkOptions = {},
): LockedRuntimeOperations => ({
  fetchRelease: (lock) => fetchLockedReleaseMetadata(lock, networkOptions),
  download: (url, destination) =>
    downloadLockedFile(
      url,
      destination,
      networkOptions.fetchImpl ?? globalThis.fetch,
    ),
  sha256: sha256File,
  extractMain: extractLockedMainArchive,
  extractCompanion: extractZipSafely,
  rename: (from, to) => Deno.rename(from, to),
  remove: (filePath) => Deno.remove(filePath),
  nonce: () => `${Date.now()}-${crypto.randomUUID()}`,
});

const mergeLockedRuntimeOperations = (
  overrides: Partial<LockedRuntimeOperations> | undefined,
  networkOptions: LockedRuntimeNetworkOptions = {},
): LockedRuntimeOperations => ({
  ...defaultLockedRuntimeOperations(networkOptions),
  ...overrides,
});

const pathExists = async (filePath: string): Promise<boolean> => {
  try {
    await Deno.lstat(filePath);
    return true;
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return false;
    throw error;
  }
};

interface NamedParallelOperation {
  name: string;
  run: () => Promise<unknown>;
}

async function awaitParallelOperations(
  context: string,
  operations: NamedParallelOperation[],
): Promise<void> {
  const results = await Promise.allSettled(
    operations.map((operation) => Promise.resolve().then(operation.run)),
  );
  const failures: Error[] = [];
  for (let index = 0; index < results.length; index++) {
    const result = results[index];
    if (result.status === "fulfilled") continue;
    const reason = result.reason instanceof Error
      ? result.reason.message
      : String(result.reason);
    failures.push(
      new Error(`${operations[index].name}: ${reason}`, {
        cause: result.reason,
      }),
    );
  }
  if (failures.length > 0) {
    throw new AggregateError(
      failures,
      `${context} failed after all parallel operations settled: ${
        failures.map((failure) => failure.message).join("; ")
      }`,
    );
  }
}

async function removeIfPresent(filePath: string): Promise<void> {
  try {
    await Deno.remove(filePath, { recursive: true });
  } catch (error) {
    if (!(error instanceof Deno.errors.NotFound)) throw error;
  }
}

function lockedRuntimeMarker(
  lock: RuntimeLock,
  artifact: RuntimeArtifact,
): LockedRuntimeMarker {
  return {
    schemaVersion: 1,
    sourceRef: lock.source.ref,
    sourceCommit: lock.source.commit,
    sourceTree: lock.source.tree,
    releaseId: lock.source.release.id,
    artifactId: artifact.asset.id,
    artifactName: artifact.asset.name,
    artifactSha256: normalizeSha256(artifact.asset.sha256),
    applicationIniAssetId: artifact.applicationIniAsset.id,
    applicationIniAssetName: artifact.applicationIniAsset.name,
    applicationIniAssetSha256: normalizeSha256(
      artifact.applicationIniAsset.sha256,
    ),
    platform: artifact.platform,
    architecture: artifact.architecture,
    version: artifact.version,
    buildId: artifact.buildId,
  };
}

function markerMatches(
  value: unknown,
  expected: LockedRuntimeMarker,
): boolean {
  if (!isRecord(value)) return false;
  return Object.entries(expected).every(([key, expectedValue]) =>
    value[key] === expectedValue
  );
}

async function assertAssetFile(
  filePath: string,
  asset: RuntimeArtifact["asset"],
  sha256: (filePath: string) => Promise<string>,
): Promise<void> {
  const info = await Deno.stat(filePath);
  if (!info.isFile) {
    throw new Error(`Downloaded asset is not a regular file: ${filePath}`);
  }
  if (info.size !== asset.size) {
    throw new Error(
      `Locked Runtime asset size mismatch for ${asset.name}: ${info.size} !== ${asset.size}.`,
    );
  }
  const actualSha256 = normalizeSha256(await sha256(filePath));
  const expectedSha256 = normalizeSha256(asset.sha256);
  if (actualSha256 !== expectedSha256) {
    throw new Error(
      `Locked Runtime SHA-256 mismatch for ${asset.name}: ${actualSha256} !== ${expectedSha256}.`,
    );
  }
}

function findReleaseAsset(
  release: LockedReleaseMetadata,
  expected: RuntimeArtifact["asset"],
): LockedReleaseAssetMetadata {
  const matches = release.assets.filter((asset) => asset.id === expected.id);
  if (matches.length !== 1) {
    throw new Error(
      `Expected exactly one GitHub release asset with id ${expected.id}; found ${matches.length}.`,
    );
  }
  const actual = matches[0];
  if (
    actual.name !== expected.name ||
    actual.size !== expected.size ||
    normalizeSha256(actual.digest) !== normalizeSha256(expected.sha256)
  ) {
    throw new Error(
      `GitHub release metadata does not match the lock for asset ${expected.id}.`,
    );
  }
  return actual;
}

function resolveLockedReleaseAssets(
  lock: RuntimeLock,
  artifact: RuntimeArtifact,
  release: LockedReleaseMetadata,
): {
  main: LockedReleaseAssetMetadata;
  companion: LockedReleaseAssetMetadata;
} {
  if (
    release.id !== lock.source.release.id ||
    release.tagName !== lock.source.ref ||
    release.immutable !== lock.source.release.immutable
  ) {
    throw new Error("GitHub release identity does not match the Runtime lock.");
  }
  return {
    main: findReleaseAsset(release, artifact.asset),
    companion: findReleaseAsset(release, artifact.applicationIniAsset),
  };
}

export interface LockedRuntimeReleaseMetadataValidationOptions
  extends LockedRuntimeNetworkOptions {
  lock: RuntimeLock;
  operations?: Partial<LockedRuntimeOperations>;
}

/**
 * Validate the live GitHub release identity and asset metadata against the
 * canonical lock. This network gate is intentionally separate from native
 * artifact validation so fork PRs can use deterministic public downloads.
 */
export async function validateLockedRuntimeReleaseMetadata(
  options: LockedRuntimeReleaseMetadataValidationOptions,
): Promise<LockedReleaseMetadata> {
  const operations = mergeLockedRuntimeOperations(options.operations, {
    githubToken: options.githubToken,
    fetchImpl: options.fetchImpl,
  });
  const release = await operations.fetchRelease(options.lock);
  for (const artifact of options.lock.artifacts) {
    resolveLockedReleaseAssets(options.lock, artifact, release);
  }
  return release;
}

async function assertRuntimeTree(
  root: string,
  artifact: RuntimeArtifact,
): Promise<void> {
  const layout = runtimeLayoutFor(root, artifact);
  const applicationInfo = await Deno.lstat(layout.applicationIni);
  if (!applicationInfo.isFile) {
    throw new Error(
      `Locked Runtime application.ini is not a regular file: ${layout.applicationIni}`,
    );
  }
  const parsed = parseApplicationIni(
    await Deno.readTextFile(layout.applicationIni),
  );
  if (parsed.name && parsed.name !== BRANDING.display_name) {
    throw new Error(
      `Locked Runtime application name mismatch: ${parsed.name} !== ${BRANDING.display_name}.`,
    );
  }
  if (parsed.version !== artifact.version) {
    throw new Error(
      `Locked Runtime Version mismatch: ${parsed.version} !== ${artifact.version}.`,
    );
  }
  if (parsed.buildId !== artifact.buildId) {
    throw new Error(
      `Locked Runtime BuildID mismatch: ${parsed.buildId} !== ${artifact.buildId}.`,
    );
  }
  const executableInfo = await Deno.lstat(layout.executable);
  if (!executableInfo.isFile || executableInfo.size <= 0) {
    throw new Error(
      `Locked Runtime executable is missing or empty: ${layout.executable}`,
    );
  }
  if (
    artifact.platform !== "windows" &&
    // Windows filesystems do not expose portable POSIX execute bits, even
    // when validating an explicitly selected non-Windows fixture.
    Deno.build.os !== "windows" &&
    executableInfo.mode !== null &&
    (executableInfo.mode & 0o111) === 0
  ) {
    throw new Error(
      `Locked Runtime executable has no execute bit: ${layout.executable}`,
    );
  }
}

async function findSingleApplicationIni(root: string): Promise<string> {
  const matches: string[] = [];
  const pending = [root];
  while (pending.length > 0) {
    const directory = pending.pop();
    if (!directory) break;
    for await (const entry of Deno.readDir(directory)) {
      const entryPath = path.join(directory, entry.name);
      if (entry.isDirectory) {
        pending.push(entryPath);
      } else if (
        entry.isFile && entry.name.toLowerCase().endsWith("application.ini")
      ) {
        matches.push(entryPath);
      }
    }
  }
  if (matches.length !== 1) {
    throw new Error(
      `Expected exactly one companion application.ini; found ${matches.length}.`,
    );
  }
  return matches[0];
}

function debugRuntimeVersionPath(binRootDir: string): string {
  if (PLATFORM !== "darwin") {
    return path.join(binRootDir, BRANDING.base_name, "nora.version.txt");
  }
  return path.join(
    binRootDir,
    BRANDING.base_name,
    `${BRANDING.display_name}.app`,
    "Contents",
    "Resources",
    "nora.version.txt",
  );
}

function debugRuntimeExecutablePath(binRootDir: string): string {
  return path.join(binRootDir, path.relative(BIN_ROOT_DIR, BIN_PATH_EXE));
}

async function assertDebugRuntimeTree(binRootDir: string): Promise<{
  version: string;
  buildId: string;
}> {
  const executable = await Deno.lstat(debugRuntimeExecutablePath(binRootDir));
  if (!executable.isFile || executable.size <= 0) {
    throw new Error(
      `Debug Runtime executable is missing or empty under ${binRootDir}.`,
    );
  }

  const applicationIni = await findSingleApplicationIni(binRootDir);
  const parsed = parseApplicationIni(await Deno.readTextFile(applicationIni));
  if (parsed.name && parsed.name !== BRANDING.display_name) {
    throw new Error(
      `Debug Runtime application name mismatch: ${parsed.name} !== ${BRANDING.display_name}.`,
    );
  }
  return parsed;
}

function debugRuntimeMarkerFields(
  marker: DebugRuntimeMarker,
): Record<string, unknown> {
  return {
    schemaVersion: marker.schemaVersion,
    distribution: marker.distribution,
    platform: marker.platform,
    architecture: marker.architecture,
    artifactName: marker.artifactName,
    artifactSize: marker.artifactSize,
    artifactSha256: marker.artifactSha256,
  };
}

async function isMatchingDebugRuntime(
  binRootDir: string,
  entry: RuntimeDeployEntry,
  target: NativeRuntimeTarget,
): Promise<boolean> {
  try {
    const marker = JSON.parse(
      await Deno.readTextFile(path.join(binRootDir, DEBUG_RUNTIME_MARKER)),
    ) as unknown;
    const expected: DebugRuntimeMarker = {
      schemaVersion: 1,
      distribution: "debug",
      platform: target.platform,
      architecture: target.architecture,
      artifactName: entry.name,
      artifactSize: entry.size!,
      artifactSha256: normalizeSha256(entry.hash!),
      version: "",
      buildId: "",
    };
    if (!isRecord(marker)) return false;
    for (
      const [key, value] of Object.entries(debugRuntimeMarkerFields(expected))
    ) {
      if (marker[key] !== value) return false;
    }
    await assertDebugRuntimeTree(binRootDir);
    return true;
  } catch (error) {
    if (!(error instanceof Deno.errors.NotFound)) {
      const message = error instanceof Error ? error.message : String(error);
      logger.warn(`Existing Debug Runtime needs replacement: ${message}`);
    }
    return false;
  }
}

async function assertRuntimeDeployEntry(
  filePath: string,
  entry: RuntimeDeployEntry,
): Promise<void> {
  const size = entry.size;
  if (
    typeof size !== "number" ||
    !Number.isSafeInteger(size) ||
    size <= 0 ||
    typeof entry.hash !== "string" ||
    !/^[0-9a-f]{64}$/i.test(entry.hash)
  ) {
    throw new Error(
      `Debug Runtime FTP asset ${entry.name} is missing a valid size or SHA-256 hash.`,
    );
  }
  const info = await Deno.stat(filePath);
  if (!info.isFile) {
    throw new Error(
      `Debug Runtime FTP asset is not a regular file: ${filePath}`,
    );
  }
  if (info.size !== size) {
    throw new Error(
      `Debug Runtime FTP asset size mismatch for ${entry.name}: ${info.size} !== ${size}.`,
    );
  }
  const actualSha256 = normalizeSha256(await sha256File(filePath));
  const expectedSha256 = normalizeSha256(entry.hash);
  if (actualSha256 !== expectedSha256) {
    throw new Error(
      `Debug Runtime FTP SHA-256 mismatch for ${entry.name}: ${actualSha256} !== ${expectedSha256}.`,
    );
  }
}

async function assertCompanionApplicationIni(
  root: string,
  artifact: RuntimeArtifact,
): Promise<void> {
  const parsed = parseApplicationIni(
    await Deno.readTextFile(await findSingleApplicationIni(root)),
  );
  if (
    parsed.version !== artifact.version ||
    parsed.buildId !== artifact.buildId
  ) {
    throw new Error(
      `Companion application.ini does not match ${artifact.version}/${artifact.buildId}.`,
    );
  }
}

export interface LockedRuntimeValidationOptions
  extends LockedRuntimeNetworkOptions {
  lock: RuntimeLock;
  destinationRoot: string;
  target?: NativeRuntimeTarget;
  operations?: Partial<LockedRuntimeOperations>;
}

/**
 * Download, authenticate, extract, and validate a native Runtime without
 * changing the live `_dist/bin` tree. The destination must not already exist.
 */
export async function validateLockedRuntimeArtifact(
  options: LockedRuntimeValidationOptions,
): Promise<RuntimeArtifact> {
  const target = options.target ??
    resolveNativeRuntimeTarget(Deno.build.os, Deno.build.arch);
  const artifact = selectLockedRuntimeArtifact(options.lock, target);
  const operations = mergeLockedRuntimeOperations(options.operations, {
    fetchImpl: options.fetchImpl,
  });
  const destinationRoot = path.resolve(options.destinationRoot);
  if (await pathExists(destinationRoot)) {
    throw new Error(
      `Locked Runtime validation destination already exists: ${destinationRoot}`,
    );
  }
  if (
    !isPlainLockedRuntimeAssetName(artifact.asset.name) ||
    !isPlainLockedRuntimeAssetName(artifact.applicationIniAsset.name)
  ) {
    throw new Error("Locked Runtime asset names must be plain file names.");
  }

  const nonce = operations.nonce();
  const scratchRoot = path.dirname(destinationRoot);
  const archivePath = path.join(
    scratchRoot,
    `.runtime-download-${nonce}-${artifact.asset.name}`,
  );
  const companionArchivePath = path.join(
    scratchRoot,
    `.runtime-download-${nonce}-${artifact.applicationIniAsset.name}`,
  );
  const companionRoot = path.join(
    scratchRoot,
    `.runtime-companion-${nonce}`,
  );
  let succeeded = false;

  try {
    await Deno.mkdir(scratchRoot, { recursive: true });
    await Deno.mkdir(destinationRoot);
    await Deno.mkdir(companionRoot);

    await awaitParallelOperations("Locked Runtime downloads", [
      {
        name: artifact.asset.name,
        run: () =>
          operations.download(
            lockedReleasePublicDownloadUrl(
              options.lock.source.repository,
              options.lock.source.ref,
              artifact.asset.name,
            ),
            archivePath,
          ),
      },
      {
        name: artifact.applicationIniAsset.name,
        run: () =>
          operations.download(
            lockedReleasePublicDownloadUrl(
              options.lock.source.repository,
              options.lock.source.ref,
              artifact.applicationIniAsset.name,
            ),
            companionArchivePath,
          ),
      },
    ]);
    await awaitParallelOperations("Locked Runtime integrity checks", [
      {
        name: artifact.asset.name,
        run: () =>
          assertAssetFile(
            archivePath,
            artifact.asset,
            operations.sha256,
          ),
      },
      {
        name: artifact.applicationIniAsset.name,
        run: () =>
          assertAssetFile(
            companionArchivePath,
            artifact.applicationIniAsset,
            operations.sha256,
          ),
      },
    ]);

    await operations.extractMain(artifact, archivePath, destinationRoot);
    await operations.extractCompanion(companionArchivePath, companionRoot);
    await assertSafeFilesystemTree(destinationRoot);
    await assertSafeFilesystemTree(companionRoot);
    await assertRuntimeTree(destinationRoot, artifact);
    await assertCompanionApplicationIni(companionRoot, artifact);

    const forbiddenStatePaths = [
      path.join(destinationRoot, "applied_patches"),
      path.join(destinationRoot, BRANDING.base_name, ".git"),
    ];
    const presentForbiddenState = [];
    for (const forbiddenPath of forbiddenStatePaths) {
      if (await pathExists(forbiddenPath)) {
        presentForbiddenState.push(forbiddenPath);
      }
    }
    if (presentForbiddenState.length > 0) {
      throw new Error(
        `Locked Runtime archive unexpectedly contains local control state: ${
          presentForbiddenState.join(", ")
        }.`,
      );
    }
    succeeded = true;
    return artifact;
  } finally {
    for (
      const temporaryPath of [
        archivePath,
        companionArchivePath,
        companionRoot,
      ]
    ) {
      try {
        await removeIfPresent(temporaryPath);
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.warn(
          `Could not clean Runtime temporary path ${temporaryPath}: ${message}`,
        );
      }
    }
    if (!succeeded) {
      try {
        await removeIfPresent(destinationRoot);
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.warn(
          `Could not clean failed Runtime validation tree ${destinationRoot}: ${message}`,
        );
      }
    }
  }
}

async function isMatchingLockedRuntime(
  binRootDir: string,
  lock: RuntimeLock,
  artifact: RuntimeArtifact,
): Promise<boolean> {
  const markerPath = path.join(binRootDir, LOCKED_RUNTIME_MARKER);
  try {
    const marker = JSON.parse(await Deno.readTextFile(markerPath)) as unknown;
    if (!markerMatches(marker, lockedRuntimeMarker(lock, artifact))) {
      return false;
    }
    await assertRuntimeTree(binRootDir, artifact);
    await assertLockedMacInfoPlist(
      binRootDir,
      binRootDir,
      artifact,
    );
    return true;
  } catch (error) {
    if (!(error instanceof Deno.errors.NotFound)) {
      const message = error instanceof Error ? error.message : String(error);
      logger.warn(`Existing locked Runtime needs replacement: ${message}`);
    }
    return false;
  }
}

async function isolateLockedRuntimeControlState(
  binRootDir: string,
  nonce: string,
  rename: LockedRuntimeOperations["rename"],
): Promise<string | undefined> {
  const distRoot = path.dirname(binRootDir);
  const source = path.join(distRoot, "marionette-port.txt");
  if (!(await pathExists(source))) return undefined;
  const backupRoot = path.join(distRoot, `runtime-control-backup-${nonce}`);
  const backup = path.join(backupRoot, "marionette-port.txt");
  let backupRootCreated = false;
  try {
    await Deno.mkdir(backupRoot);
    backupRootCreated = true;
    await rename(source, backup);
    return backupRoot;
  } catch (error) {
    if (!backupRootCreated) throw error;
    if (await pathExists(backup)) {
      try {
        await rename(backup, source);
      } catch (rollbackError) {
        throw new Error(
          `Failed to isolate Marionette control state and failed to restore it. Recovery data remains at ${backupRoot}. Original error: ${
            String(error)
          }. Rollback error: ${
            rollbackError instanceof Error
              ? rollbackError.message
              : String(rollbackError)
          }`,
        );
      }
    }
    await removeIfPresent(backupRoot);
    throw error;
  }
}

async function restoreLockedRuntimeControlState(
  binRootDir: string,
  backupRoot: string,
  rename: LockedRuntimeOperations["rename"],
): Promise<void> {
  const source = path.join(backupRoot, "marionette-port.txt");
  const destination = path.join(
    path.dirname(binRootDir),
    "marionette-port.txt",
  );
  if (!(await pathExists(source))) {
    throw new Error(`Marionette recovery file is missing from ${backupRoot}.`);
  }
  if (await pathExists(destination)) {
    throw new Error(
      `Refusing to overwrite Marionette control state while restoring ${backupRoot}.`,
    );
  }
  await rename(source, destination);
  await removeIfPresent(backupRoot);
}

interface FailedTestControlDisposition {
  quarantinePath?: string;
}

async function invalidateTestControlAfterReplacementFailure(
  profileDir: string,
  nonce: string,
  operations: LockedRuntimeOperations,
): Promise<FailedTestControlDisposition> {
  const livePath = path.join(profileDir, "nora-tests-control.json");
  const quarantinePath = path.join(
    profileDir,
    `nora-tests-control.runtime-quarantine-${nonce}.json`,
  );
  if (!(await pathExists(livePath))) return {};

  let quarantineError: unknown;
  try {
    if (await pathExists(quarantinePath)) {
      throw new Error(
        `Refusing to overwrite existing browser-test control quarantine ${quarantinePath}.`,
      );
    }
    await operations.rename(livePath, quarantinePath);
    if (!(await pathExists(livePath))) return { quarantinePath };
    quarantineError = new Error(
      `Browser-test control rename returned without removing ${livePath}.`,
    );
  } catch (error) {
    quarantineError = error;
  }

  let removalError: unknown;
  try {
    await operations.remove(livePath);
    if (!(await pathExists(livePath))) {
      return (await pathExists(quarantinePath)) ? { quarantinePath } : {};
    }
    removalError = new Error(
      `Browser-test control removal returned without removing ${livePath}.`,
    );
  } catch (error) {
    removalError = error;
  }

  throw new Error(
    `Could not invalidate stale browser-test control. Browser startup must remain stopped. ` +
      `Live control may remain at ${livePath}; attempted quarantine path is ${quarantinePath}. ` +
      `Quarantine error: ${
        quarantineError instanceof Error
          ? quarantineError.message
          : String(quarantineError)
      }. Removal error: ${
        removalError instanceof Error
          ? removalError.message
          : String(removalError)
      }`,
  );
}

async function swapLockedRuntime(
  stageRoot: string,
  binRootDir: string,
  backupRoot: string,
  failedRoot: string,
  rename: LockedRuntimeOperations["rename"],
): Promise<string | undefined> {
  const hadLiveRuntime = await pathExists(binRootDir);
  if (hadLiveRuntime) await rename(binRootDir, backupRoot);

  try {
    await rename(stageRoot, binRootDir);
  } catch (swapError) {
    let displacedCandidate: string | undefined;
    const rollbackErrors: string[] = [];
    if (await pathExists(binRootDir)) {
      try {
        await rename(binRootDir, failedRoot);
        displacedCandidate = failedRoot;
      } catch (error) {
        rollbackErrors.push(
          `could not preserve failed candidate: ${
            error instanceof Error ? error.message : String(error)
          }`,
        );
      }
    }
    if (hadLiveRuntime) {
      try {
        await rename(backupRoot, binRootDir);
      } catch (error) {
        rollbackErrors.push(
          `could not restore backup: ${
            error instanceof Error ? error.message : String(error)
          }`,
        );
      }
    }
    if (rollbackErrors.length > 0) {
      throw new Error(
        `Locked Runtime swap failed and rollback was incomplete. Recover the previous Runtime from ${backupRoot}${
          displacedCandidate
            ? `; failed candidate is at ${displacedCandidate}`
            : ""
        }. Swap error: ${
          swapError instanceof Error ? swapError.message : String(swapError)
        }. ${rollbackErrors.join("; ")}`,
      );
    }
    throw swapError;
  }
  return hadLiveRuntime ? backupRoot : undefined;
}

/** Install the exact native Runtime described by the canonical lock. */
export async function installLockedRuntime(
  options: LockedRuntimeInstallOptions,
): Promise<LockedRuntimeInstallResult> {
  const target = options.target ??
    resolveNativeRuntimeTarget(Deno.build.os, Deno.build.arch);
  const artifact = selectLockedRuntimeArtifact(options.lock, target);
  const binRootDir = path.resolve(options.binRootDir ?? BIN_ROOT_DIR);
  if (await isMatchingLockedRuntime(binRootDir, options.lock, artifact)) {
    logger.info(
      `Locked Runtime ${artifact.version}/${artifact.buildId} is already installed.`,
    );
    return { artifact, reused: true };
  }

  const operations = mergeLockedRuntimeOperations(options.operations, {
    fetchImpl: options.fetchImpl,
  });
  const nonce = operations.nonce();
  const profileDir = path.resolve(options.profileDir ?? PATHS.profile_test);
  const stageRoot = `${binRootDir}.runtime-staging-${nonce}`;
  const backupRoot = `${binRootDir}.runtime-backup-${nonce}`;
  const failedRoot = `${binRootDir}.runtime-failed-${nonce}`;
  let transactionStarted = false;

  try {
    if (
      await pathExists(stageRoot) ||
      await pathExists(backupRoot) ||
      await pathExists(failedRoot)
    ) {
      throw new Error(
        `Refusing to reuse an existing locked Runtime transaction path for nonce ${nonce}.`,
      );
    }
    transactionStarted = true;
    await validateLockedRuntimeArtifact({
      lock: options.lock,
      destinationRoot: stageRoot,
      target,
      operations,
    });
    const marker = lockedRuntimeMarker(options.lock, artifact);
    await Deno.writeTextFile(
      path.join(stageRoot, LOCKED_RUNTIME_MARKER),
      `${JSON.stringify(marker, null, 2)}\n`,
      { createNew: true },
    );
    const layout = runtimeLayoutFor(stageRoot, artifact);
    await Deno.writeTextFile(layout.legacyVersionFile, VERSION, {
      createNew: true,
    });
    await prepareLockedMacInfoPlist(stageRoot, binRootDir, artifact);
    await assertSafeFilesystemTree(stageRoot);

    const controlBackupPath = await isolateLockedRuntimeControlState(
      binRootDir,
      nonce,
      operations.rename,
    );
    let backupPath: string | undefined;
    try {
      backupPath = await swapLockedRuntime(
        stageRoot,
        binRootDir,
        backupRoot,
        failedRoot,
        operations.rename,
      );
    } catch (swapError) {
      if (controlBackupPath) {
        try {
          await restoreLockedRuntimeControlState(
            binRootDir,
            controlBackupPath,
            operations.rename,
          );
        } catch (restoreError) {
          throw new Error(
            `Locked Runtime swap failed and Marionette control state restoration failed. Recovery data remains at ${controlBackupPath}. Swap error: ${
              swapError instanceof Error ? swapError.message : String(swapError)
            }. Control restore error: ${
              restoreError instanceof Error
                ? restoreError.message
                : String(restoreError)
            }`,
          );
        }
      }
      throw swapError;
    }
    logger.success(
      `Installed locked Runtime ${artifact.version}/${artifact.buildId}.`,
    );
    if (backupPath) {
      logger.info(
        `Previous Runtime retained for recovery at ${backupPath}.`,
      );
    }
    return {
      artifact,
      reused: false,
      backupPath,
      controlBackupPath,
    };
  } catch (installError) {
    let disposition: FailedTestControlDisposition;
    try {
      disposition = await invalidateTestControlAfterReplacementFailure(
        profileDir,
        nonce,
        operations,
      );
    } catch (invalidationError) {
      throw new Error(
        `Locked Runtime replacement failed and browser-test control invalidation also failed. ` +
          `Runtime failure: ${
            installError instanceof Error
              ? installError.message
              : String(installError)
          }. Invalidation failure: ${
            invalidationError instanceof Error
              ? invalidationError.message
              : String(invalidationError)
          }`,
        { cause: installError },
      );
    }
    if (disposition.quarantinePath) {
      throw new Error(
        `${
          installError instanceof Error
            ? installError.message
            : String(installError)
        } Stale browser-test control was quarantined at ${disposition.quarantinePath}.`,
        { cause: installError },
      );
    }
    throw installError;
  } finally {
    if (transactionStarted && await pathExists(stageRoot)) {
      try {
        await removeIfPresent(stageRoot);
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.warn(
          `Could not clean Runtime staging tree ${stageRoot}: ${message}`,
        );
      }
    }
  }
}

export interface RuntimeDeployEntry {
  type: string;
  name: string;
  size?: number;
  hash?: string;
}

interface RuntimeDeployIndex {
  description?: string;
  version?: string;
  generatedTime?: number;
  data?: RuntimeDeployEntry[];
}

const PLATFORM_KEYWORDS: Record<Platform, string[]> = {
  windows: ["windows", "win"],
  darwin: ["macos", "mac", "darwin"],
  linux: ["linux"],
};

const ARCH_KEYWORDS: Record<BinArchive["architecture"], string[]> = {
  x86_64: ["x86_64", "x64", "amd64"],
  aarch64: ["aarch64", "arm64"],
  universal: ["universal"],
};

export function debugRuntimeArchiveName(binArchive: BinArchive): string {
  if (binArchive.platform === "windows") {
    return "windows-x86_64-artifacts.zip";
  }
  if (binArchive.platform === "linux") {
    return `${
      binArchive.architecture === "aarch64" ? "linux-aarch64" : "linux-x86_64"
    }-artifacts.zip`;
  }
  return "mac-universal-artifacts.zip";
}

export function selectDebugRuntimeEntry(
  entries: RuntimeDeployEntry[],
  binArchive: BinArchive,
): RuntimeDeployEntry {
  const expectedName = debugRuntimeArchiveName(binArchive);
  const matches = entries.filter((entry) => entry.name === expectedName);
  if (matches.length !== 1) {
    throw new Error(
      `Expected exactly one Debug Runtime FTP asset ${expectedName}; found ${matches.length}.`,
    );
  }
  const entry = matches[0];
  const size = entry.size;
  if (
    typeof size !== "number" ||
    !Number.isSafeInteger(size) ||
    size <= 0 ||
    typeof entry.hash !== "string" ||
    !/^[0-9a-f]{64}$/i.test(entry.hash)
  ) {
    throw new Error(
      `Debug Runtime FTP asset ${expectedName} is missing a valid size or SHA-256 hash.`,
    );
  }
  return entry;
}

const normalizeName = (value: string): string => value.toLowerCase();

export const filterRuntimeEntries = (
  index: RuntimeDeployIndex,
): RuntimeDeployEntry[] => {
  if (!index?.data || !Array.isArray(index.data)) return [];
  return index.data.filter(
    (entry): entry is RuntimeDeployEntry =>
      !!entry &&
      typeof entry.name === "string" &&
      entry.type === "file" &&
      entry.name.length > 0,
  );
};

export const scoreRuntimeEntry = (
  entryName: string,
  binArchive: BinArchive,
): number => {
  const name = normalizeName(entryName);
  let score = 0;

  PLATFORM_KEYWORDS[binArchive.platform].forEach((keyword) => {
    if (name.includes(keyword)) score += 50;
  });

  ARCH_KEYWORDS[binArchive.architecture].forEach((keyword) => {
    if (name.includes(keyword)) score += 40;
  });

  if (name.includes("artifact")) score += 5;
  if (name.endsWith(".zip")) score += 5;
  return score;
};

export const pickRuntimeEntry = (
  entries: RuntimeDeployEntry[],
  binArchive: BinArchive,
): RuntimeDeployEntry => {
  const scored = entries
    .map((entry) => ({
      entry,
      score: scoreRuntimeEntry(entry.name, binArchive),
    }))
    .sort((a, b) => b.score - a.score);

  const best = scored[0];
  if (!best) {
    throw new Error("No runtime artifacts available to download.");
  }
  if (best.score <= 0) {
    logger.warn(
      "No strong runtime artifact match found. Using the first available entry: %s",
      best.entry.name,
    );
  }
  return best.entry;
};

const buildRuntimeUrl = (entryName: string): string =>
  `${RUNTIME_BASE_URL}${entryName}`;

const fetchRuntimeIndex = async (): Promise<RuntimeDeployIndex> => {
  const maxAttempts = 5;
  let lastError: Error | undefined;

  for (let attempt = 1; attempt <= maxAttempts; attempt++) {
    try {
      const resp = await fetch(RUNTIME_INDEX_URL, {
        headers: { Accept: "application/json" },
        redirect: "follow",
      });
      if (!resp.ok) {
        throw new Error(`HTTP ${resp.status} while fetching runtime index`);
      }

      const index = (await resp.json()) as RuntimeDeployIndex;
      if (filterRuntimeEntries(index).length === 0) {
        throw new Error(
          "Runtime build index does not contain any downloadable file entries.",
        );
      }

      return index;
    } catch (error) {
      lastError = error instanceof Error ? error : new Error(String(error));
      if (attempt < maxAttempts) {
        const delayMs = 1000 * attempt;
        logger.warn(
          `Runtime index fetch failed (attempt ${attempt}/${maxAttempts}): ${lastError.message}. Retrying in ${delayMs}ms...`,
        );
        await new Promise((resolve) => setTimeout(resolve, delayMs));
      }
    }
  }

  throw lastError ?? new Error("Failed to fetch runtime index");
};

async function installDebugRuntime(): Promise<void> {
  const binArchive = getBinArchive();
  const target = resolveNativeRuntimeTarget(Deno.build.os, Deno.build.arch);
  const index = await fetchRuntimeIndex();
  const entry = selectDebugRuntimeEntry(
    filterRuntimeEntries(index),
    binArchive,
  );

  if (await isMatchingDebugRuntime(BIN_ROOT_DIR, entry, target)) {
    logger.info(
      `Debug Runtime ${entry.name} (${entry.hash}) is already installed.`,
    );
    return;
  }

  const nonce = crypto.randomUUID();
  const distRoot = path.dirname(BIN_ROOT_DIR);
  const stageRoot = path.join(distRoot, `.runtime-debug-staging-${nonce}`);
  const backupRoot = path.join(distRoot, `.runtime-debug-backup-${nonce}`);
  const failedRoot = path.join(distRoot, `.runtime-debug-failed-${nonce}`);
  const downloadRoot = path.join(
    distRoot,
    `.runtime-debug-download-${nonce}`,
  );
  const archivePath = path.join(downloadRoot, binArchive.filename);
  let transactionStarted = false;

  try {
    for (
      const transactionPath of [
        stageRoot,
        backupRoot,
        failedRoot,
        downloadRoot,
      ]
    ) {
      if (await pathExists(transactionPath)) {
        throw new Error(
          `Refusing to reuse an existing Debug Runtime transaction path: ${transactionPath}.`,
        );
      }
    }
    transactionStarted = true;
    await Deno.mkdir(stageRoot, { recursive: true });
    await Deno.mkdir(downloadRoot, { recursive: true });
    await downloadBin(binArchive.filename, entry, archivePath);
    await decompressBin(stageRoot, archivePath);
    await assertSafeFilesystemTree(stageRoot);
    const identity = await assertDebugRuntimeTree(stageRoot);
    const marker: DebugRuntimeMarker = {
      schemaVersion: 1,
      distribution: "debug",
      platform: target.platform,
      architecture: target.architecture,
      artifactName: entry.name,
      artifactSize: entry.size!,
      artifactSha256: normalizeSha256(entry.hash!),
      version: identity.version,
      buildId: identity.buildId,
    };
    await Deno.writeTextFile(
      path.join(stageRoot, DEBUG_RUNTIME_MARKER),
      `${JSON.stringify(marker, null, 2)}\n`,
      { createNew: true },
    );

    const backupPath = await swapLockedRuntime(
      stageRoot,
      BIN_ROOT_DIR,
      backupRoot,
      failedRoot,
      (from, to) => Deno.rename(from, to),
    );
    if (backupPath) await removeIfPresent(backupPath);
    logger.success(
      `Installed Debug Runtime ${identity.version}/${identity.buildId} from ${entry.name}.`,
    );
  } finally {
    if (transactionStarted) {
      for (const transactionPath of [stageRoot, downloadRoot]) {
        try {
          await removeIfPresent(transactionPath);
        } catch (error) {
          const message = error instanceof Error
            ? error.message
            : String(error);
          logger.warn(
            `Could not clean Debug Runtime transaction path ${transactionPath}: ${message}`,
          );
        }
      }
    }
  }
}

/**
 * Entry point: ensure the selected Runtime distribution is present and
 * preferences are saved. Release is the default and uses the canonical lock;
 * development explicitly selects the Debug FTP distribution.
 */
export async function run(
  overrides: InitializerRunOptions = {},
): Promise<void> {
  const dependencies: InitializerRunDependencies = {
    loadLock: overrides.loadLock ?? (() => loadRuntimeLock()),
    installLock: overrides.installLock ??
      (async (lock) => {
        await installLockedRuntime({ lock });
      }),
    installDebug: overrides.installDebug ?? installDebugRuntime,
    savePrefs: overrides.savePrefs ?? savePrefsForProfile,
  };
  if ((overrides.distribution ?? "release") === "debug") {
    await dependencies.installDebug!();
  } else {
    await dependencies.installLock(await dependencies.loadLock());
  }
  dependencies.savePrefs();
}

/** Explicit compatibility entry point for the retired CDN bootstrap. */
export async function runLegacyInitializerForCompatibility(): Promise<void> {
  const hasVersion = exists(BIN_VERSION);
  const hasBin = exists(BIN_PATH_EXE);
  let needInit = false;

  logger.info(BIN_VERSION);

  if (hasBin && hasVersion) {
    const version = Deno.readTextFileSync(BIN_VERSION).trim();
    if (VERSION !== version) {
      logger.warn(
        `Version mismatch: ${version} !== ${VERSION}. Re-extracting.`,
      );
      safeRemove(BIN_ROOT_DIR);
      needInit = true;
    } else {
      logger.info("Binary version matches. No initialization needed.");
    }
  } else if (hasBin && !hasVersion) {
    logger.info(
      `Binary exists but version file is missing. Writing ${VERSION}.`,
    );
    Deno.mkdirSync(BIN_DIR, { recursive: true });
    Deno.writeTextFileSync(BIN_VERSION, VERSION);
    logger.success("Initialization complete.");
  } else if (!hasBin && hasVersion) {
    logger.error(
      "Version file exists but binary is missing. Abnormal termination.",
    );
    throw new Error("Unreachable: !has_bin && has_version");
  } else {
    logger.info("Binary not found. Extracting.");
    needInit = true;
  }

  if (needInit) {
    Deno.mkdirSync(BIN_ROOT_DIR, { recursive: true });
    await decompressBin();
    logger.success("Initialization complete.");
  }
}

export function savePrefsForProfile(): void {
  const profileDir = PATHS.profile_test;
  const userJsPath = path.join(profileDir, "user.js");

  const userJsContent = `/**
 *! DO NOT EDIT THIS FILE.
 *
 ** This file is AUTOGENERATED
 ** Please modify the 'tools/lib/initializer.rb' in the repo.
 */
user_pref("devtools.debugger.prompt-connection", false);
user_pref("security.disallow_privileged_https_script_loads", false);
user_pref("security.allow_parent_unrestricted_js_loads", true);
user_pref("remote.active-protocols", 0);
user_pref("marionette.enabled", true);
user_pref("marionette.port", 2828);
user_pref("devtools.chrome.enabled", true);
user_pref("browser.newtabpage.enabled", true);
`;

  Deno.mkdirSync(profileDir, { recursive: true });
  Deno.writeTextFileSync(userJsPath, userJsContent);
  logger.info(`Wrote developer preferences to ${userJsPath}`);
}

async function extractNestedZip(
  outerZipPath: string,
  extractToDir: string,
): Promise<void> {
  const tempDir = await Deno.makeTempDir({ prefix: "nora-runtime-outer-" });

  try {
    logger.info("Extracting outer zip...");
    await extractZipSafely(outerZipPath, tempDir);

    // Find the inner zip file
    let innerZipPath: string | null = null;
    for (const entry of Deno.readDirSync(tempDir)) {
      if (entry.name.endsWith(".zip")) {
        innerZipPath = path.join(tempDir, entry.name);
        break;
      }
    }

    if (!innerZipPath) {
      throw new Error("No inner zip file found in the extracted archive");
    }

    logger.info("Extracting inner zip...");
    await extractZipSafely(innerZipPath, extractToDir, BRANDING.base_name);

    // Set proper permissions on Unix-like systems
    if (PLATFORM !== "windows") {
      try {
        runCommand("chmod", ["-R", "755", extractToDir]);
      } catch {
        // Ignore chmod errors
      }
    }
  } finally {
    // Clean up temp directory
    try {
      await Deno.remove(tempDir, { recursive: true });
    } catch {
      // Ignore cleanup errors
    }
  }
}

export async function decompressBin(
  destinationRoot = BIN_ROOT_DIR,
  archivePathOverride?: string,
): Promise<void> {
  const binArchive = getBinArchive();
  let archivePath = archivePathOverride
    ? path.resolve(archivePathOverride)
    : path.resolve(binArchive.filename);

  if (!archivePathOverride && !exists(archivePath)) {
    const cwd = Deno.cwd();
    const expectedExt = binArchive.format === "tar.xz"
      ? ".tar.xz"
      : path.extname(binArchive.filename);
    for (const entry of Deno.readDirSync(cwd)) {
      if (entry.isFile && entry.name.endsWith(expectedExt)) {
        logger.info(`Found alternative artifact: ${entry.name}`);
        archivePath = path.resolve(entry.name);
        break;
      }
    }
  }

  logger.info(`Binary extraction started: ${path.basename(archivePath)}`);

  if (!exists(archivePath)) {
    logger.warn(
      `${binArchive.filename} not found. Downloading from GitHub release.`,
    );
    await downloadBin(binArchive.filename);
    archivePath = path.resolve(binArchive.filename);
  }

  try {
    Deno.mkdirSync(destinationRoot, { recursive: true });
    // Handle nested zip extraction
    if (binArchive.filename.endsWith(".zip")) {
      await extractNestedZip(archivePath, destinationRoot);
    } else {
      // Handle other archive formats (DMG, tar.xz, etc.)
      switch (PLATFORM) {
        case "windows":
          throw new Error("Non-zip archives not supported on Windows");

        case "darwin": {
          logger.info("macOS extraction (hdiutil)");
          const mountPoint = await Deno.makeTempDir({
            prefix: "nora_dmg_mount_",
          });
          try {
            runCommand("hdiutil", [
              "attach",
              "-nobrowse",
              "-quiet",
              "-mountpoint",
              mountPoint,
              archivePath,
            ]);
            const subdir = path.join(destinationRoot, BRANDING.base_name);
            Deno.mkdirSync(subdir, { recursive: true });
            runCommand("cp", ["-a", `${mountPoint}/.`, subdir]);

            // Rename any .app to BRANDING.display_name.app
            for (const entry of Deno.readDirSync(subdir)) {
              if (entry.isDirectory && entry.name.endsWith(".app")) {
                const oldPath = path.join(subdir, entry.name);
                const newName = BRANDING.display_name + ".app";
                const newPath = path.join(subdir, newName);
                if (entry.name !== newName) {
                  logger.info(`Renaming ${entry.name} to ${newName}`);
                  Deno.renameSync(oldPath, newPath);
                }
              }
            }

            try {
              runCommand("xattr", ["-rc", destinationRoot]);
            } catch {
              // xattr might not be present; ignore
            }
            runCommand("chmod", ["-R", "755", destinationRoot]);
            // Patch Info.plist to inject developer repo/obj path equal to
            // the extracted directory so GetRepoDir can find it without
            // editing C++ sources.
            try {
              const macUtils = await import("./macos_utils.ts");
              await macUtils.patchAppInfoPlists(subdir, subdir, subdir);
            } catch (e) {
              const msg = e instanceof Error ? e.message : String(e);
              logger.warn(`macOS Info.plist patch skipped: ${msg}`);
            }
          } finally {
            try {
              runCommand("hdiutil", ["detach", "-quiet", mountPoint]);
            } catch {
              // ignore detach failures
            }
          }
          break;
        }

        case "linux": {
          await extractTarXzSafely(
            archivePath,
            destinationRoot,
            BRANDING.base_name,
          );
          break;
        }

        default:
          throw new Error(`Unsupported platform: ${PLATFORM}`);
      }
    }

    const versionPath = debugRuntimeVersionPath(destinationRoot);
    Deno.mkdirSync(path.dirname(versionPath), { recursive: true });
    Deno.writeTextFileSync(versionPath, VERSION);
    logger.success("Extraction complete!");
  } catch (e) {
    const msg = e instanceof Error ? e.message : String(e);
    logger.error(`Error during extraction: ${msg}`);
    throw e;
  }
}

export async function downloadBin(
  filename: string,
  selectedEntry?: RuntimeDeployEntry,
  outputPath?: string,
): Promise<RuntimeDeployEntry> {
  const binArchive = getBinArchive();
  let picked = selectedEntry;
  if (!picked) {
    const index = await fetchRuntimeIndex();
    const entries = filterRuntimeEntries(index);
    if (!entries.length) {
      throw new Error(
        "Runtime build index does not contain any downloadable file entries.",
      );
    }
    picked = pickRuntimeEntry(entries, binArchive);
  }
  if (!isPlainLockedRuntimeAssetName(picked.name)) {
    throw new Error(`Debug Runtime FTP asset name is unsafe: ${picked.name}`);
  }
  const downloadUrl = buildRuntimeUrl(picked.name);
  logger.info(
    `Downloading runtime artifact '${picked.name}' for ${binArchive.platform}/${binArchive.architecture}`,
  );

  const resp = await fetch(downloadUrl, { redirect: "follow" });
  if (!resp.ok) {
    throw new Error(`HTTP ${resp.status} while downloading ${downloadUrl}`);
  }

  // Helper: stream response to file with progress logs
  const streamToFile = async (
    response: Response,
    outPath: string,
    label: string,
    rewritePrevLogLine = false,
  ): Promise<void> => {
    const total = Number(response.headers.get("content-length") ?? "0");
    const encoder = new TextEncoder();
    const upOne = "\x1b[1A";
    const clearLine = "\x1b[2K";
    const logPrefix = "[initializer] INFO: ";
    const writeProgress = async (text: string) => {
      if (rewritePrevLogLine) {
        const line = `${upOne}${clearLine}${logPrefix}${text}\n`;
        await Deno.stdout.write(encoder.encode(line));
      } else {
        logger.info(text);
      }
    };
    try {
      const outDir = path.dirname(outPath);
      try {
        if (outDir && outDir !== ".") {
          await Deno.mkdir(outDir, { recursive: true });
        }
      } catch {
        // ignore
      }
      const file = await Deno.open(outPath, {
        create: true,
        write: true,
        truncate: true,
      });
      try {
        const reader = response.body?.getReader();
        if (!reader) {
          const buf = new Uint8Array(await response.arrayBuffer());
          await file.write(buf);
          return;
        }
        let downloaded = 0;
        let lastPct = -1;
        let lastTime = 0;
        while (true) {
          const { value, done } = await reader.read();
          if (done) break;
          if (value) {
            await file.write(value);
            downloaded += value.byteLength;
            const now = Date.now();
            if (total > 0) {
              const pct = Math.floor((downloaded / total) * 100);
              if (pct !== lastPct && (pct % 5 === 0 || now - lastTime > 1000)) {
                lastPct = pct;
                lastTime = now;
                await writeProgress(
                  `${label}: ${pct}% (${(downloaded / 1048576).toFixed(1)}MB/${
                    (total / 1048576).toFixed(1)
                  }MB)`,
                );
              }
            } else if (now - lastTime > 1000) {
              lastTime = now;
              await writeProgress(
                `${label}: ${(downloaded / 1048576).toFixed(1)}MB`,
              );
            }
          }
        }
      } finally {
        file.close();
      }
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e);
      throw new Error(`Failed to write ${outPath}: ${msg}`);
    }
  };

  // If the expected target is a .zip, save as-is (nested unzip is handled later).
  const isZipExpected = filename.toLowerCase().endsWith(".zip");
  const destinationPath = path.resolve(outputPath ?? filename);
  if (isZipExpected) {
    await streamToFile(
      resp,
      destinationPath,
      "Downloading artifact",
      true,
    );
    await assertRuntimeDeployEntry(destinationPath, picked);
    logger.success(`Downloaded artifact zip to ${filename}`);
    return picked;
  }

  // Non-zip expected (e.g., .tar.xz, .dmg): extract inner file from the downloaded zip.
  const tmpZipPath = path.resolve(
    path.join(
      path.dirname(destinationPath),
      `runtime_artifact_${Date.now()}_${path.basename(picked.name)}`,
    ),
  );
  const tmpExtractDir = path.join(
    path.dirname(destinationPath),
    `runtime_artifact_extract_${Date.now()}`,
  );
  try {
    await Deno.mkdir(path.dirname(tmpZipPath), { recursive: true });
  } catch {
    // ignore
  }
  await streamToFile(resp, tmpZipPath, "Downloading artifact", true);
  await assertRuntimeDeployEntry(tmpZipPath, picked);
  await Deno.mkdir(tmpExtractDir);

  try {
    // Extract downloaded zip
    switch (PLATFORM) {
      case "windows":
        try {
          runCommand("tar", ["-xf", tmpZipPath, "-C", tmpExtractDir]);
        } catch {
          runCommand("powershell", [
            "-NoProfile",
            "-NonInteractive",
            "-Command",
            `Expand-Archive -LiteralPath '${tmpZipPath}' -DestinationPath '${tmpExtractDir}' -Force`,
          ]);
        }
        break;
      case "darwin":
      case "linux":
        runCommand("unzip", ["-q", tmpZipPath, "-d", tmpExtractDir]);
        break;
    }

    // Recursively find the intended inner file
    const wantedExt = filename.endsWith(".tar.xz")
      ? ".tar.xz"
      : path.extname(filename);

    let pickedInner: string | null = null;
    const stack: string[] = [tmpExtractDir];
    while (stack.length) {
      const dir = stack.pop()!;
      for (const entry of Deno.readDirSync(dir)) {
        const p = path.join(dir, entry.name);
        if (entry.isDirectory) {
          stack.push(p);
          continue;
        }
        // Prefer exact filename
        if (entry.name === filename) {
          pickedInner = p;
          break;
        }
        // Next, prefer extension match
        if (wantedExt && entry.name.endsWith(wantedExt)) {
          pickedInner ??= p;
        }
      }
      if (pickedInner) break;
    }

    if (!pickedInner) {
      throw new Error(
        `Inner file with expected extension '${wantedExt}' not found in artifact '${picked.name}'.`,
      );
    }

    // Move/copy to requested filename at CWD
    try {
      Deno.copyFileSync(pickedInner, destinationPath);
    } catch {
      // fallback to read/write
      const data = Deno.readFileSync(pickedInner);
      Deno.writeFileSync(destinationPath, data);
    }
    logger.success(`Downloaded binary to ${filename}`);
  } finally {
    // Cleanup temp
    try {
      Deno.removeSync(tmpZipPath);
    } catch {
      // ignore
    }
    try {
      Deno.removeSync(tmpExtractDir, { recursive: true });
    } catch {
      // ignore
    }
  }
  return picked;
}
