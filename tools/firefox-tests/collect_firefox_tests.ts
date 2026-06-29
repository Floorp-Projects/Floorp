// SPDX-License-Identifier: MPL-2.0

import { parseArgs } from "@std/cli";
import * as path from "@std/path";

export type CollectionScope = "all" | "browser-chrome";

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
  scope: CollectionScope;
  sourceRepo: string;
  sourceRef?: string;
  pathPrefix?: string;
}

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
  generatedAt: string;
  source: {
    repository: string;
    ref: string;
    commit: string;
  };
  filters: {
    pathPrefix: string | null;
  };
  scope: CollectionScope;
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

const TEST_FILE_PATTERN =
  /^(browser|test|chrome|a11y|crashtest|reftest|xpcshell)[_-].+\.(?:html|js|mjs|xhtml)$/;
const BROWSER_CHROME_CANDIDATE_PATTERN = /^browser_.+\.(?:js|mjs)$/;
const PYTHON_TEST_PATTERN = /(?:^test_.+\.py$|.+_test\.py$)/;
const WEB_PLATFORM_TEST_PATTERN =
  /(?:\.(?:any|window|worker|sharedworker|serviceworker)\.js|\.https?\.html|\.tentative\.html|\.html|\.xhtml)$/;

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

function shouldIncludeRoot(root: TestRoot, scope: CollectionScope): boolean {
  if (scope === "all") {
    return true;
  }
  return root.harness === "browser-chrome" || root.harness === "chrome";
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

function rolesForFile(entry: GitTreeEntry, roots: TestRoot[]): FileRole[] {
  const base = basenameForGitPath(entry.path);
  const roles = new Set<FileRole>();

  if (roots.some((root) => root.manifestPath === entry.path)) {
    roles.add("manifest");
  }
  if (base === "head.js" || base === "head.mjs") {
    roles.add("head");
  }
  if (TEST_FILE_PATTERN.test(base)) {
    roles.add("test");
  }
  if (PYTHON_TEST_PATTERN.test(base)) {
    roles.add("test");
  }
  if (
    roots.some((root) => root.harness === "web-platform") &&
    !entry.path.includes("/resources/") &&
    WEB_PLATFORM_TEST_PATTERN.test(base)
  ) {
    roles.add("test");
  }
  if (
    roots.some((root) =>
      root.harness === "browser-chrome" || root.harness === "chrome"
    ) && BROWSER_CHROME_CANDIDATE_PATTERN.test(base)
  ) {
    roles.add("candidate");
  }
  if (roles.size === 0) {
    roles.add("support");
  }

  return [...roles].sort();
}

function sortUniqueHarnesses(roots: TestRoot[]): Harness[] {
  return [...new Set(roots.map((root) => root.harness))].sort();
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
  pathPrefix: string | null,
): Promise<GitTreeEntry[]> {
  const args = [
    "ls-tree",
    "-r",
    "-l",
    "--full-tree",
    "HEAD",
  ];
  if (pathPrefix !== null) {
    args.push("--", pathPrefix);
  }

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
  stderrText: Promise<string>,
  entry: GitTreeEntry,
): Promise<WrittenBlob> {
  const encoder = new TextEncoder();
  await stdin.write(encoder.encode(`${entry.object}\n`));

  const header = await stdout.readLine();
  const match = header.match(/^([0-9a-f]+) (\S+) (\d+)$/);
  if (!match) {
    const stderr = (await stderrText).trim();
    throw new Error(
      `Unexpected git cat-file header for ${entry.path}: ${header}${
        stderr === "" ? "" : `\n${stderr}`
      }`,
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
  const written = new Map<string, WrittenBlob>();

  try {
    for (const entry of entries) {
      written.set(
        entry.path,
        await writeCollectedBlobBytes(
          outputRoot,
          stdin,
          stdout,
          stderrText,
          entry,
        ),
      );
    }
    await stdin.close();
  } catch (error) {
    await stdin.close().catch(() => {});
    await stdout.cancel();
    child.kill("SIGTERM");
    throw error;
  }

  const status = await child.status;
  const stderr = (await stderrText).trim();
  await stdout.cancel();
  if (!status.success) {
    throw new Error(`git cat-file --batch failed: ${stderr}`);
  }

  return written;
}

function discoverTestRoots(
  entries: GitTreeEntry[],
  scope: CollectionScope,
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
      return shouldIncludeRoot(root, scope) ? root : undefined;
    })
    .filter((entry): entry is TestRoot => entry !== undefined);

  const wellKnownRoots = WELL_KNOWN_TEST_ROOTS.filter((root) =>
    shouldIncludeRoot(root, scope) &&
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

function buildBrowserChromeCandidates(
  files: CollectedFile[],
  roots: TestRoot[],
): BrowserChromeCandidate[] {
  const filesByDirectory = new Map<string, CollectedFile[]>();
  for (const file of files) {
    const directory = dirnameForGitPath(file.path);
    const existing = filesByDirectory.get(directory) ?? [];
    existing.push(file);
    filesByDirectory.set(directory, existing);
  }

  return files
    .filter((file) => file.roles.includes("candidate"))
    .map((file) => {
      const directory = dirnameForGitPath(file.path);
      const directoryFiles = filesByDirectory.get(directory) ?? [];
      const nearestRoot = roots
        .filter((root) =>
          isPathInsideRoot(file.path, root.path) &&
          (root.harness === "browser-chrome" || root.harness === "chrome")
        )
        .sort((a, b) => b.path.length - a.path.length)[0];
      return {
        path: file.path,
        directory,
        nearestManifest: nearestRoot?.manifestPath ?? "",
        hasHeadJs: directoryFiles.some((entry) => entry.roles.includes("head")),
        supportFileCount: directoryFiles.filter((entry) =>
          entry.roles.includes("support")
        ).length,
        size: file.size,
        sha256: file.sha256,
      };
    })
    .sort((a, b) => a.path.localeCompare(b.path));
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
    `Path prefix: \`${manifest.filters.pathPrefix ?? "[none]"}\``,
    `Scope: \`${manifest.scope}\``,
    `Generated at: \`${manifest.generatedAt}\``,
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

function parseScope(value: string): CollectionScope {
  if (value === "all" || value === "browser-chrome") {
    return value;
  }
  throw new Error(`Invalid --scope value: ${value}`);
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

  await Deno.remove(outputDir, { recursive: true }).catch((error) => {
    if (!(error instanceof Deno.errors.NotFound)) {
      throw error;
    }
  });
  await Deno.mkdir(outputDir, { recursive: true });

  const sourceCommit = (await runGitText(runtimeDir, ["rev-parse", "HEAD"]))
    .trim();
  const sourceRef = options.sourceRef ??
    (await runGitText(runtimeDir, ["rev-parse", "--abbrev-ref", "HEAD"]))
      .trim();
  const entries = await listGitTree(runtimeDir, pathPrefix);
  const roots = discoverTestRoots(entries, options.scope, pathPrefix);

  const selectedFiles: SelectedFile[] = [];
  const harnessCounts = {} as Record<Harness, number>;
  const extensionCounts: Record<string, number> = {};

  for (const entry of entries) {
    if (!isWithinPathPrefix(entry.path, pathPrefix)) {
      continue;
    }

    const matchingRoots = rootsForEntry(entry, roots);
    if (matchingRoots.length === 0) {
      continue;
    }

    selectedFiles.push({
      entry,
      harnesses: sortUniqueHarnesses(matchingRoots),
      roles: rolesForFile(entry, matchingRoots),
    });
  }

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
    for (const harness of harnesses) {
      incrementRecord(harnessCounts, harness);
    }
    incrementRecord(extensionCounts, extensionForGitPath(entry.path));
    collectedFiles.push({
      path: entry.path,
      outputPath: written.outputPath,
      size: written.size,
      sha256: written.sha256,
      harnesses,
      roles,
    });
  }

  const candidates = buildBrowserChromeCandidates(collectedFiles, roots);
  const manifest: CollectionManifest = {
    schemaVersion: 1,
    generatedAt: new Date().toISOString(),
    source: {
      repository: options.sourceRepo,
      ref: sourceRef,
      commit: sourceCommit,
    },
    filters: {
      pathPrefix,
    },
    scope: options.scope,
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
    "  --scope <scope>        all | browser-chrome (default: all)",
    `  --source-repo <repo>   Source repository label (default: ${DEFAULT_SOURCE_REPO})`,
    "  --source-ref <ref>     Source ref label recorded in manifest",
    "  --path-prefix <path>   Optional upstream path prefix filter",
    "  --help, -h             Show this help",
  ].join("\n");
}

export async function main(args: string[]): Promise<void> {
  const parsed = parseArgs(args, {
    string: [
      "runtime-dir",
      "out",
      "scope",
      "source-repo",
      "source-ref",
      "path-prefix",
    ],
    boolean: ["help"],
    alias: { h: "help" },
    default: {
      out: DEFAULT_OUTPUT_DIR,
      scope: "all",
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
    scope: parseScope(ensureString(parsed.scope, "--scope")),
    sourceRepo: ensureString(parsed["source-repo"], "--source-repo"),
    sourceRef: typeof parsed["source-ref"] === "string"
      ? parsed["source-ref"]
      : undefined,
    pathPrefix: typeof parsed["path-prefix"] === "string"
      ? parsed["path-prefix"]
      : undefined,
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
