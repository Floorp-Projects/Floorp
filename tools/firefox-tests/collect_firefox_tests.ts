// SPDX-License-Identifier: MPL-2.0

import { parseArgs } from "@std/cli";
import * as path from "@std/path";
import {
  loadRuntimeLock,
  type RuntimeLock,
  type RuntimeMaterial,
} from "../src/runtime_lock.ts";

type Harness =
  | "a11y"
  | "browser-chrome"
  | "chrome"
  | "crashtest"
  | "eval"
  | "firefox-ui"
  | "generic"
  | "js"
  | "marionette"
  | "mochitest"
  | "performance"
  | "python"
  | "reftest"
  | "web-platform"
  | "xpcshell";

type FileRole = "candidate" | "head" | "manifest" | "support" | "test";

export interface CollectFirefoxTestsOptions {
  runtimeDir: string;
  outputDir: string;
  sourceRepo: string;
  sourceRef?: string;
  pathPrefix?: string;
  candidate?: boolean;
  runtimeLock?: RuntimeLock;
}

type CollectionMode = "candidate" | "locked";

interface GitTreeEntry {
  mode: string;
  type: string;
  object: string;
  size: number | null;
  path: string;
}

interface TestRoot {
  path: string;
  manifestPath: string;
  manifestName: string;
  harness: Harness;
}

interface CollectedFile {
  path: string;
  outputPath: string;
  size: number;
  sha256: string;
  mode: string;
  gitBlob: string;
  harnesses: Harness[];
  roles: FileRole[];
}

interface SelectedFile {
  entry: GitTreeEntry;
  harnesses: Harness[];
  roles: FileRole[];
}

interface WrittenBlob {
  outputPath: string;
  size: number;
  sha256: string;
}

interface BrowserChromeCandidate {
  path: string;
  directory: string;
  nearestManifest: string;
  hasHeadJs: boolean;
  supportFileCount: number;
  size: number;
  sha256: string;
}

interface CollectionManifest {
  schemaVersion: 1;
  mode: CollectionMode;
  source: {
    repository: string;
    ref: string;
    commit: string;
    tree: string;
  };
  filters: {
    pathPrefix: string | null;
  };
  scope: "browser-chrome" | "locked-closure";
  counts: {
    roots: number;
    files: number;
    candidates: number;
    totalBytes: number;
  };
  harnessCounts: Record<Harness, number>;
  extensionCounts: Record<string, number>;
  roots: TestRoot[];
  files: CollectedFile[];
}

const DEFAULT_OUTPUT_DIR = "_dist/firefox-tests";
const DEFAULT_SOURCE_REPO = "Floorp-Projects/Floorp-Runtime";

const WELL_KNOWN_TEST_ROOTS: readonly TestRoot[] = [
  {
    path: "testing/web-platform/tests",
    manifestPath: "testing/web-platform/tests",
    manifestName: "[wpt-tests-root]",
    harness: "web-platform",
  },
  {
    path: "testing/web-platform/meta",
    manifestPath: "testing/web-platform/meta",
    manifestName: "[wpt-meta-root]",
    harness: "web-platform",
  },
  {
    path: "testing/web-platform/mozilla/tests",
    manifestPath: "testing/web-platform/mozilla/tests",
    manifestName: "[wpt-mozilla-tests-root]",
    harness: "web-platform",
  },
  {
    path: "testing/web-platform/mozilla/meta",
    manifestPath: "testing/web-platform/mozilla/meta",
    manifestName: "[wpt-mozilla-meta-root]",
    harness: "web-platform",
  },
  {
    path: "testing/web-platform/outbound",
    manifestPath: "testing/web-platform/outbound",
    manifestName: "[wpt-outbound-root]",
    harness: "web-platform",
  },
];

function normalizeGitPath(value: string): string {
  return value.replaceAll("\\", "/").replace(/^\/+/, "");
}

function dirnameForGitPath(value: string): string {
  const normalized = normalizeGitPath(value);
  const index = normalized.lastIndexOf("/");
  return index === -1 ? "" : normalized.slice(0, index);
}

function basenameForGitPath(value: string): string {
  const normalized = normalizeGitPath(value);
  const index = normalized.lastIndexOf("/");
  return index === -1 ? normalized : normalized.slice(index + 1);
}

function extensionForGitPath(value: string): string {
  const base = basenameForGitPath(value);
  const index = base.lastIndexOf(".");
  return index === -1 ? "[none]" : base.slice(index).toLowerCase();
}

function isPathInsideRoot(filePath: string, rootPath: string): boolean {
  if (rootPath === "") {
    return true;
  }
  return filePath === rootPath || filePath.startsWith(`${rootPath}/`);
}

function isWithinPathPrefix(
  filePath: string,
  pathPrefix: string | null,
): boolean {
  return pathPrefix === null || isPathInsideRoot(filePath, pathPrefix);
}

function shouldIncludeRoot(root: TestRoot): boolean {
  return root.harness === "browser-chrome";
}

function isLikelyTestManifestPath(manifestPath: string): boolean {
  return /(^|\/)(test|tests|testing|marionette|webdriver|benchmarks)(\/|$)/
    .test(
      manifestPath,
    );
}

function detectHarnessForManifest(manifestPath: string): Harness | undefined {
  const manifestName = basenameForGitPath(manifestPath);
  const name = manifestName.toLowerCase();
  const normalizedPath = normalizeGitPath(manifestPath).toLowerCase();
  const isTomlOrIni = name.endsWith(".toml") || name.endsWith(".ini");

  if (isTomlOrIni) {
    if (name === "a11y.toml" || name === "a11y.ini") {
      return "a11y";
    }
    if (name.startsWith("browser") || name.startsWith("_browser")) {
      return "browser-chrome";
    }
    if (name.startsWith("chrome")) {
      return "chrome";
    }
    if (name.startsWith("mochitest") || name.includes("-mochitest")) {
      return "mochitest";
    }
    if (name.startsWith("xpcshell")) {
      return "xpcshell";
    }
    if (
      name === "wpt.ini" ||
      name === "web-platform-tests.ini" ||
      name === "web-platform-tests.toml"
    ) {
      return "web-platform";
    }
    if (name === "perftest.toml") {
      return "performance";
    }
    if (name === "eval.toml") {
      return "eval";
    }
    if (
      name === "python.toml" ||
      name === "unit-tests.toml" ||
      name === "integration-tests.toml"
    ) {
      return "python";
    }
    if (
      name.startsWith("manifest") && isLikelyTestManifestPath(normalizedPath)
    ) {
      if (normalizedPath.startsWith("testing/firefox-ui/")) {
        return "firefox-ui";
      }
      if (normalizedPath.includes("/marionette/")) {
        return "marionette";
      }
      if (normalizedPath.includes("/webdriver/")) {
        return "web-platform";
      }
      return "generic";
    }
  }

  if (name.endsWith("crashtest.list") || name.endsWith("crashtests.list")) {
    return "crashtest";
  }
  if (name.endsWith("reftest.list")) {
    return "reftest";
  }
  if (name === "jstests.list" && isLikelyTestManifestPath(normalizedPath)) {
    return "js";
  }

  return undefined;
}

function sortUniqueHarnesses(roots: TestRoot[]): Harness[] {
  return [...new Set(roots.map((root) => root.harness))].sort();
}

function rolesForLockedMaterial(material: RuntimeMaterial): FileRole[] {
  switch (material.role) {
    case "test":
      return ["candidate", "test"];
    case "manifest":
      return ["manifest"];
    case "head-support":
      return ["head"];
    case "support":
      return ["support"];
  }
}

function parseLsTreeLine(line: string): GitTreeEntry {
  const match = line.match(/^(\d+)\s+(\S+)\s+([0-9a-f]+)\s+(-|\d+)\t(.+)$/);
  if (!match) {
    throw new Error(`Unexpected git ls-tree output: ${line}`);
  }

  return {
    mode: match[1],
    type: match[2],
    object: match[3],
    size: match[4] === "-" ? null : Number(match[4]),
    path: normalizeGitPath(match[5]),
  };
}

function concatBytes(
  left: Uint8Array<ArrayBufferLike>,
  right: Uint8Array<ArrayBufferLike>,
): Uint8Array<ArrayBufferLike> {
  const output = new Uint8Array(left.byteLength + right.byteLength);
  output.set(left);
  output.set(right, left.byteLength);
  return output;
}

class ByteStreamReader {
  #buffer: Uint8Array<ArrayBufferLike> = new Uint8Array();
  #done = false;
  #reader: ReadableStreamDefaultReader<Uint8Array>;

  constructor(stream: ReadableStream<Uint8Array>) {
    this.#reader = stream.getReader();
  }

  async #fill(minLength: number): Promise<void> {
    while (!this.#done && this.#buffer.byteLength < minLength) {
      const next = await this.#reader.read();
      if (next.done) {
        this.#done = true;
        return;
      }
      this.#buffer = concatBytes(this.#buffer, next.value);
    }
  }

  async readLine(): Promise<string> {
    const decoder = new TextDecoder();

    while (true) {
      const newlineIndex = this.#buffer.indexOf(0x0a);
      if (newlineIndex !== -1) {
        const line = this.#buffer.slice(0, newlineIndex);
        this.#buffer = this.#buffer.slice(newlineIndex + 1);
        return decoder.decode(line);
      }

      if (this.#done) {
        if (this.#buffer.byteLength === 0) {
          throw new Error("Unexpected end of git cat-file output");
        }
        const line = this.#buffer;
        this.#buffer = new Uint8Array();
        return decoder.decode(line);
      }

      await this.#fill(this.#buffer.byteLength + 1);
    }
  }

  async readExact(length: number): Promise<Uint8Array> {
    await this.#fill(length);
    if (this.#buffer.byteLength < length) {
      throw new Error("Unexpected short read from git cat-file output");
    }

    const output = this.#buffer.slice(0, length);
    this.#buffer = this.#buffer.slice(length);
    return output;
  }

  async cancel(): Promise<void> {
    await this.#reader.cancel().catch(() => {});
    this.#reader.releaseLock();
  }
}

async function runGitBytes(
  runtimeDir: string,
  args: string[],
): Promise<Uint8Array> {
  const result = await new Deno.Command("git", {
    args,
    cwd: runtimeDir,
    stdout: "piped",
    stderr: "piped",
  }).output();

  if (!result.success) {
    const stderr = new TextDecoder().decode(result.stderr).trim();
    throw new Error(`git ${args.join(" ")} failed: ${stderr}`);
  }

  return result.stdout;
}

async function runGitText(runtimeDir: string, args: string[]): Promise<string> {
  return new TextDecoder().decode(await runGitBytes(runtimeDir, args));
}

async function listGitTree(
  runtimeDir: string,
  materials: RuntimeMaterial[],
): Promise<GitTreeEntry[]> {
  if (materials.length === 0) {
    return [];
  }
  const args = [
    "ls-tree",
    "-r",
    "-l",
    "--full-tree",
    "HEAD",
    "--",
    ...materials.map((entry) => entry.path),
  ];

  const text = await runGitText(runtimeDir, args);
  return text.trimEnd().split("\n")
    .filter((line) => line.trim() !== "")
    .map(parseLsTreeLine)
    .filter((entry) => entry.type === "blob")
    .sort((a, b) => a.path.localeCompare(b.path));
}

async function sha256Hex(bytes: Uint8Array): Promise<string> {
  const digestInput = new Uint8Array(bytes.byteLength);
  digestInput.set(bytes);
  const digest = await crypto.subtle.digest("SHA-256", digestInput);
  return [...new Uint8Array(digest)]
    .map((byte) => byte.toString(16).padStart(2, "0"))
    .join("");
}

async function writeJsonFile(filePath: string, value: unknown): Promise<void> {
  await Deno.mkdir(path.dirname(filePath), { recursive: true });
  await Deno.writeTextFile(filePath, `${JSON.stringify(value, null, 2)}\n`);
}

async function writeCollectedBlobBytes(
  outputRoot: string,
  stdin: WritableStreamDefaultWriter<Uint8Array>,
  stdout: ByteStreamReader,
  entry: GitTreeEntry,
): Promise<WrittenBlob> {
  const encoder = new TextEncoder();
  await stdin.write(encoder.encode(`${entry.object}\n`));

  const header = await stdout.readLine();
  const match = header.match(/^([0-9a-f]+) (\S+) (\d+)$/);
  if (!match) {
    throw new Error(
      `Unexpected git cat-file header for ${entry.path}: ${header}`,
    );
  }
  if (match[1] !== entry.object || match[2] !== "blob") {
    throw new Error(
      `Unexpected git cat-file object for ${entry.path}: ${header}`,
    );
  }

  const size = Number(match[3]);
  const bytes = await stdout.readExact(size);
  const trailingNewline = await stdout.readExact(1);
  if (trailingNewline[0] !== 0x0a) {
    throw new Error(`Unexpected git cat-file framing for ${entry.path}`);
  }

  const outputPath = path.join(outputRoot, "files", ...entry.path.split("/"));
  await Deno.mkdir(path.dirname(outputPath), { recursive: true });
  await Deno.writeFile(outputPath, bytes);
  return {
    outputPath: path.relative(outputRoot, outputPath).replaceAll("\\", "/"),
    size: bytes.byteLength,
    sha256: await sha256Hex(bytes),
  };
}

async function writeCollectedBlobs(
  runtimeDir: string,
  outputRoot: string,
  entries: GitTreeEntry[],
): Promise<Map<string, WrittenBlob>> {
  const child = new Deno.Command("git", {
    args: ["cat-file", "--batch"],
    cwd: runtimeDir,
    stdin: "piped",
    stdout: "piped",
    stderr: "piped",
  }).spawn();
  const stdin = child.stdin.getWriter();
  const stdout = new ByteStreamReader(child.stdout);
  const stderrText = new Response(child.stderr).text();
  const statusPromise = child.status;
  const written = new Map<string, WrittenBlob>();

  try {
    for (const entry of entries) {
      written.set(
        entry.path,
        await writeCollectedBlobBytes(
          outputRoot,
          stdin,
          stdout,
          entry,
        ),
      );
    }
    await stdin.close();

    const [status, stderr] = await Promise.all([statusPromise, stderrText]);
    await stdout.cancel();
    if (!status.success) {
      throw new Error(`git cat-file --batch failed: ${stderr.trim()}`);
    }

    return written;
  } catch (error) {
    const abortInput = stdin.abort(error).catch(() => {});
    const cancelOutput = stdout.cancel().catch(() => {});
    try {
      child.kill("SIGTERM");
    } catch {
      // The child may already have exited. Cleanup must preserve the first error.
    }
    await Promise.allSettled([
      abortInput,
      cancelOutput,
      statusPromise,
      stderrText,
    ]);
    throw error;
  }
}

export const collectFirefoxTestsTestInternals = {
  writeCollectedBlobs,
};

function discoverTestRoots(
  entries: GitTreeEntry[],
  pathPrefix: string | null,
): TestRoot[] {
  const manifestRoots = entries
    .map((entry): TestRoot | undefined => {
      if (!isWithinPathPrefix(entry.path, pathPrefix)) {
        return undefined;
      }

      const manifestName = basenameForGitPath(entry.path);
      const harness = detectHarnessForManifest(entry.path);
      if (!harness) {
        return undefined;
      }

      const root = {
        path: dirnameForGitPath(entry.path),
        manifestPath: entry.path,
        manifestName,
        harness,
      };
      return shouldIncludeRoot(root) ? root : undefined;
    })
    .filter((entry): entry is TestRoot => entry !== undefined);

  const wellKnownRoots = WELL_KNOWN_TEST_ROOTS.filter((root) =>
    shouldIncludeRoot(root) &&
    entries.some((entry) =>
      isWithinPathPrefix(entry.path, pathPrefix) &&
      isPathInsideRoot(entry.path, root.path)
    )
  );

  return [...manifestRoots, ...wellKnownRoots]
    .sort((a, b) =>
      a.path.localeCompare(b.path) ||
      a.manifestPath.localeCompare(b.manifestPath)
    );
}

function rootsForEntry(entry: GitTreeEntry, roots: TestRoot[]): TestRoot[] {
  const matchingRoots = roots.filter((root) =>
    isPathInsideRoot(entry.path, root.path)
  );
  const deepestPathLength = Math.max(
    -1,
    ...matchingRoots.map((root) => root.path.length),
  );
  return matchingRoots.filter((root) => root.path.length === deepestPathLength);
}

function lockedRoots(lock: RuntimeLock): TestRoot[] {
  return lock.source.tests.manifests.map((manifest) => ({
    path: dirnameForGitPath(manifest.path),
    manifestPath: manifest.path,
    manifestName: basenameForGitPath(manifest.path),
    harness: "browser-chrome",
  } satisfies TestRoot));
}

function selectLockedFiles(
  entries: GitTreeEntry[],
  lock: RuntimeLock,
): SelectedFile[] {
  const entriesByPath = new Map(entries.map((entry) => [entry.path, entry]));
  const selected: SelectedFile[] = [];

  for (const material of lock.source.materials.entries) {
    const entry = entriesByPath.get(material.path);
    if (!entry) {
      throw new Error(`Locked Runtime material is missing: ${material.path}`);
    }
    if (entry.type !== "blob") {
      throw new Error(
        `Locked Runtime material is not a blob: ${material.path} (${entry.type})`,
      );
    }
    if (entry.mode !== material.mode) {
      throw new Error(
        `Locked Runtime material mode mismatch for ${material.path}: expected ${material.mode}, got ${entry.mode}`,
      );
    }
    if (entry.object !== material.gitBlob) {
      throw new Error(
        `Locked Runtime material Git blob mismatch for ${material.path}: expected ${material.gitBlob}, got ${entry.object}`,
      );
    }
    if (entry.size !== material.bytes) {
      throw new Error(
        `Locked Runtime material size mismatch for ${material.path}: expected ${material.bytes}, got ${
          entry.size ?? "unknown"
        }`,
      );
    }
    selected.push({
      entry,
      harnesses: ["browser-chrome"],
      roles: rolesForLockedMaterial(material),
    });
  }

  if (selected.length !== lock.source.materials.count) {
    throw new Error(
      `Locked Runtime material count mismatch: expected ${lock.source.materials.count}, got ${selected.length}`,
    );
  }
  return selected;
}

function buildLockedBrowserChromeCandidates(
  files: CollectedFile[],
  lock: RuntimeLock,
  requireComplete: boolean,
): BrowserChromeCandidate[] {
  const filesByPath = new Map(files.map((file) => [file.path, file]));
  const manifestsByPath = new Map(
    lock.source.tests.manifests.map((manifest) => [manifest.path, manifest]),
  );

  return lock.source.tests.entries.flatMap((test) => {
    const file = filesByPath.get(test.path);
    const manifest = manifestsByPath.get(test.manifest);
    if (!file || !manifest) {
      if (requireComplete) {
        throw new Error(`Locked test closure is incomplete: ${test.path}`);
      }
      return [];
    }
    return [{
      path: test.path,
      directory: dirnameForGitPath(test.path),
      nearestManifest: test.manifest,
      hasHeadJs: manifest.supportPaths.some((supportPath) =>
        basenameForGitPath(supportPath) === "head.js"
      ),
      supportFileCount: manifest.supportPaths.length,
      size: file.size,
      sha256: file.sha256,
    }];
  }).sort((left, right) => left.path.localeCompare(right.path));
}

function incrementRecord<T extends string>(
  record: Record<T, number>,
  key: T,
  by = 1,
): void {
  record[key] = (record[key] ?? 0) + by;
}

function buildSummary(manifest: CollectionManifest): string {
  const harnessLines = Object.entries(manifest.harnessCounts)
    .sort(([a], [b]) => a.localeCompare(b))
    .map(([harness, count]) => `| ${harness} | ${count} |`);
  const extensionLines = Object.entries(manifest.extensionCounts)
    .sort(([a], [b]) => a.localeCompare(b))
    .map(([extension, count]) => `| ${extension} | ${count} |`);

  return [
    "# Firefox Test Collection",
    "",
    `Source repository: \`${manifest.source.repository}\``,
    `Source ref: \`${manifest.source.ref}\``,
    `Source commit: \`${manifest.source.commit}\``,
    `Source tree: \`${manifest.source.tree}\``,
    `Collection mode: \`${manifest.mode}\``,
    `Path prefix: \`${manifest.filters.pathPrefix ?? "[none]"}\``,
    `Scope: \`${manifest.scope}\``,
    "",
    "## Counts",
    "",
    `- Test roots: ${manifest.counts.roots}`,
    `- Files: ${manifest.counts.files}`,
    `- Browser chrome candidates: ${manifest.counts.candidates}`,
    `- Total bytes: ${manifest.counts.totalBytes}`,
    "",
    "## Harnesses",
    "",
    "| Harness | Files |",
    "| --- | ---: |",
    ...harnessLines,
    "",
    "## Extensions",
    "",
    "| Extension | Files |",
    "| --- | ---: |",
    ...extensionLines,
    "",
    "Raw files are stored under `files/` using their upstream repository paths.",
    "Use `manifest.json` for exact provenance and `browser-chrome-candidates.json` for Floorp import triage.",
    "",
  ].join("\n");
}

function ensureString(value: unknown, name: string): string {
  if (typeof value !== "string" || value.trim() === "") {
    throw new Error(`${name} is required`);
  }
  return value;
}

function normalizeOptionalPathPrefix(value: string | undefined): string | null {
  if (value === undefined || value.trim() === "") {
    return null;
  }
  return normalizeGitPath(value).replace(/\/+$/, "");
}

export async function collectFirefoxTests(
  options: CollectFirefoxTestsOptions,
): Promise<CollectionManifest> {
  const runtimeDir = path.resolve(options.runtimeDir);
  const outputDir = path.resolve(options.outputDir);
  const pathPrefix = normalizeOptionalPathPrefix(options.pathPrefix);
  const lock = options.runtimeLock ?? await loadRuntimeLock();
  const mode: CollectionMode = options.candidate ? "candidate" : "locked";
  const sourceCommit = (await runGitText(runtimeDir, ["rev-parse", "HEAD"]))
    .trim();
  const sourceTree = (await runGitText(runtimeDir, [
    "rev-parse",
    "HEAD^{tree}",
  ])).trim();
  let sourceRef: string;

  if (options.sourceRepo !== lock.source.repository) {
    throw new Error(
      `Runtime repository mismatch: expected ${lock.source.repository}, got ${options.sourceRepo}`,
    );
  }
  if (mode === "candidate") {
    if (options.sourceRef === undefined) {
      throw new Error(
        `Candidate collection requires --source-ref ${lock.source.trackingRef}`,
      );
    }
    sourceRef = options.sourceRef;
    if (sourceRef !== lock.source.trackingRef) {
      throw new Error(
        `Candidate collection must use tracking ref ${lock.source.trackingRef}, got ${sourceRef}`,
      );
    }
    const sourceRefCommit = (await runGitText(runtimeDir, [
      "rev-parse",
      "--verify",
      `${sourceRef}^{commit}`,
    ])).trim();
    if (sourceCommit !== sourceRefCommit) {
      throw new Error(
        `Candidate Runtime checkout HEAD mismatch for ${sourceRef}: expected ${sourceRefCommit}, got ${sourceCommit}`,
      );
    }
  } else {
    sourceRef = options.sourceRef ?? lock.source.ref;
    if (pathPrefix !== null) {
      throw new Error("Locked collection does not permit --path-prefix");
    }
    if (sourceRef !== lock.source.ref) {
      throw new Error(
        `Locked Runtime ref mismatch: expected ${lock.source.ref}, got ${sourceRef}`,
      );
    }
    if (sourceCommit !== lock.source.commit) {
      throw new Error(
        `Locked Runtime commit mismatch: expected ${lock.source.commit}, got ${sourceCommit}`,
      );
    }
    if (sourceTree !== lock.source.tree) {
      throw new Error(
        `Locked Runtime tree mismatch: expected ${lock.source.tree}, got ${sourceTree}`,
      );
    }
  }

  const effectivePathPrefix = mode === "candidate" ? pathPrefix : null;
  const projectedMaterials = mode === "candidate"
    ? lock.source.materials.entries.filter((material) =>
      isWithinPathPrefix(material.path, effectivePathPrefix)
    )
    : lock.source.materials.entries;
  const entries = await listGitTree(runtimeDir, projectedMaterials);
  const roots = mode === "candidate"
    ? discoverTestRoots(entries, effectivePathPrefix)
    : lockedRoots(lock);

  const selectedFiles: SelectedFile[] = mode === "locked"
    ? selectLockedFiles(entries, lock)
    : [];
  const harnessCounts = {} as Record<Harness, number>;
  const extensionCounts: Record<string, number> = {};

  if (mode === "candidate") {
    const projectedMaterialsByPath = new Map(
      projectedMaterials.map((material) => [material.path, material]),
    );
    for (const entry of entries) {
      const material = projectedMaterialsByPath.get(entry.path);
      if (!material) {
        continue;
      }

      const matchingRoots = rootsForEntry(entry, roots);
      selectedFiles.push({
        entry,
        harnesses: matchingRoots.length > 0
          ? sortUniqueHarnesses(matchingRoots)
          : ["browser-chrome"],
        roles: rolesForLockedMaterial(material),
      });
    }
  }

  await Deno.remove(outputDir, { recursive: true }).catch((error) => {
    if (!(error instanceof Deno.errors.NotFound)) {
      throw error;
    }
  });
  await Deno.mkdir(outputDir, { recursive: true });

  const writtenBlobs = await writeCollectedBlobs(
    runtimeDir,
    outputDir,
    selectedFiles.map((file) => file.entry),
  );
  const collectedFiles: CollectedFile[] = [];

  for (const selectedFile of selectedFiles) {
    const written = writtenBlobs.get(selectedFile.entry.path);
    if (!written) {
      throw new Error(`Missing copied blob for ${selectedFile.entry.path}`);
    }

    const { entry, harnesses, roles } = selectedFile;
    const expectedMaterial = mode === "locked"
      ? lock.source.materials.entries.find((material) =>
        material.path === entry.path
      )
      : undefined;
    if (
      expectedMaterial &&
      (written.size !== expectedMaterial.bytes ||
        written.sha256 !== expectedMaterial.sha256)
    ) {
      throw new Error(
        `Locked Runtime material content mismatch for ${entry.path}: expected ${expectedMaterial.bytes} bytes/${expectedMaterial.sha256}, got ${written.size} bytes/${written.sha256}`,
      );
    }
    for (const harness of harnesses) {
      incrementRecord(harnessCounts, harness);
    }
    incrementRecord(extensionCounts, extensionForGitPath(entry.path));
    collectedFiles.push({
      path: entry.path,
      outputPath: written.outputPath,
      size: written.size,
      sha256: written.sha256,
      mode: entry.mode,
      gitBlob: entry.object,
      harnesses,
      roles,
    });
  }

  const candidates = mode === "locked"
    ? buildLockedBrowserChromeCandidates(collectedFiles, lock, true)
    : buildLockedBrowserChromeCandidates(collectedFiles, lock, false);
  const manifest: CollectionManifest = {
    schemaVersion: 1,
    mode,
    source: {
      repository: options.sourceRepo,
      ref: sourceRef,
      commit: sourceCommit,
      tree: sourceTree,
    },
    filters: {
      pathPrefix: effectivePathPrefix,
    },
    scope: mode === "candidate" ? "locked-closure" : "browser-chrome",
    counts: {
      roots: roots.length,
      files: collectedFiles.length,
      candidates: candidates.length,
      totalBytes: collectedFiles.reduce((sum, file) => sum + file.size, 0),
    },
    harnessCounts,
    extensionCounts,
    roots,
    files: collectedFiles,
  };

  await writeJsonFile(path.join(outputDir, "manifest.json"), manifest);
  await writeJsonFile(
    path.join(outputDir, "browser-chrome-candidates.json"),
    candidates,
  );
  await Deno.writeTextFile(
    path.join(outputDir, "SUMMARY.md"),
    buildSummary(manifest),
  );

  return manifest;
}

function usage(): string {
  return [
    "Usage: deno task firefox-tests:collect --runtime-dir <path> [options]",
    "",
    "Options:",
    "  --runtime-dir <path>   Floorp-Runtime checkout to read",
    `  --out <path>           Output directory (default: ${DEFAULT_OUTPUT_DIR})`,
    `  --source-repo <repo>   Source repository label (default: ${DEFAULT_SOURCE_REPO})`,
    "  --source-ref <ref>     Source ref label recorded in manifest",
    "  --path-prefix <path>   Optional prefix within the locked candidate projection",
    "  --candidate            Static moving-ref projection of the locked closure",
    "  --help, -h             Show this help",
  ].join("\n");
}

export async function main(args: string[]): Promise<void> {
  const parsed = parseArgs(args, {
    string: [
      "runtime-dir",
      "out",
      "source-repo",
      "source-ref",
      "path-prefix",
    ],
    boolean: ["candidate", "help"],
    alias: { h: "help" },
    unknown: (arg, key) => {
      if (key !== undefined) {
        throw new Error(`Unknown option: ${arg}`);
      }
      return true;
    },
    default: {
      out: DEFAULT_OUTPUT_DIR,
      "source-repo": DEFAULT_SOURCE_REPO,
    },
  });

  if (parsed.help) {
    console.log(usage());
    return;
  }
  if (parsed._.length > 0) {
    throw new Error(`Unexpected positional arguments: ${parsed._.join(" ")}`);
  }

  const manifest = await collectFirefoxTests({
    runtimeDir: ensureString(parsed["runtime-dir"], "--runtime-dir"),
    outputDir: ensureString(parsed.out, "--out"),
    sourceRepo: ensureString(parsed["source-repo"], "--source-repo"),
    sourceRef: typeof parsed["source-ref"] === "string"
      ? parsed["source-ref"]
      : undefined,
    pathPrefix: typeof parsed["path-prefix"] === "string"
      ? parsed["path-prefix"]
      : undefined,
    candidate: parsed.candidate,
  });

  console.log(
    `Collected ${manifest.counts.files} Firefox test file(s) from ${manifest.counts.roots} root(s).`,
  );
  console.log(`Browser chrome candidates: ${manifest.counts.candidates}`);
}

if (import.meta.main) {
  try {
    await main(Deno.args);
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    console.error(`[firefox-tests] ${message}`);
    Deno.exit(1);
  }
}
