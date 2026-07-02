// SPDX-License-Identifier: MPL-2.0

import { parseArgs } from "@std/cli";
import * as path from "@std/path";

type TriageClassification =
  | "direct"
  | "needs-small-shim"
  | "needs-runner-shim"
  | "unsupported"
  | "quarantined"
  | "already-allowed";

type RuleClassification = Exclude<
  TriageClassification,
  "direct" | "quarantined" | "already-allowed"
>;

type AllowlistEntry =
  | string
  | {
    path: string;
    name?: string;
    note?: string;
  };

interface FirefoxTestCollectionManifest {
  schemaVersion: number;
  source?: {
    repository?: string;
    ref?: string;
    commit?: string;
  };
  files?: Array<{
    path: string;
    outputPath: string;
    roles?: string[];
    harnesses?: string[];
  }>;
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

interface QuarantineEntry {
  path: string;
  classification: "quarantined";
  reason: string;
  requiredApis: string[];
  sourceRef: string;
  lastObserved: string;
}

interface TriageRule {
  api: string;
  classification: RuleClassification;
  pattern: RegExp;
  reason: string;
}

interface TriageFinding {
  api: string;
  classification: RuleClassification;
  reason: string;
}

interface TriageEntry {
  path: string;
  directory: string;
  classification: TriageClassification;
  detectedApis: string[];
  requiredApis: string[];
  reasons: string[];
  nearestManifest: string;
  hasHeadJs: boolean;
  supportFileCount: number;
  size: number;
  sha256: string;
  quarantine?: {
    reason: string;
    sourceRef: string;
    lastObserved: string;
  };
}

interface TriageManifest {
  schemaVersion: 1;
  generatedAt: string;
  source: {
    repository: string;
    ref: string;
    commit: string;
  };
  collectionDir: string;
  allowlistPath: string;
  quarantinePath: string;
  counts: {
    candidates: number;
    classifications: Record<TriageClassification, number>;
  };
  tests: TriageEntry[];
}

interface TriageFirefoxBrowserTestsOptions {
  collectionDir: string;
  allowlistPath: string;
  quarantinePath: string;
  outputDir: string;
}

const DEFAULT_COLLECTION_DIR = "_dist/firefox-tests";
const DEFAULT_ALLOWLIST_PATH =
  "browser-features/chrome/test/firefox-downloaded/allowlist.json";
const DEFAULT_QUARANTINE_PATH =
  "browser-features/chrome/test/firefox-downloaded/quarantine.json";

const CLASSIFICATION_ORDER: Record<TriageClassification, number> = {
  direct: 0,
  "needs-small-shim": 1,
  "needs-runner-shim": 2,
  unsupported: 3,
  quarantined: 4,
  "already-allowed": 5,
};

const RULE_PRIORITY: Record<RuleClassification, number> = {
  "needs-small-shim": 1,
  "needs-runner-shim": 2,
  unsupported: 3,
};

const SUPPORTED_BROWSER_TEST_UTILS = new Set([
  "addTab",
  "browserLoaded",
  "browserStopped",
  "openNewForegroundTab",
  "removeTab",
  "switchTab",
  "waitForCondition",
  "waitForEvent",
  "waitForMutationCondition",
  "waitForNotificationInNotificationBox",
  "withNewTab",
]);

const STATIC_RULES: TriageRule[] = [
  {
    api: "SpecialPowers",
    classification: "unsupported",
    pattern: /\bSpecialPowers\b/,
    reason: "requires mochitest SpecialPowers/content-process behavior",
  },
  {
    api: "ContentTask",
    classification: "unsupported",
    pattern: /\bContentTask\b/,
    reason: "requires Firefox ContentTask process harness",
  },
  {
    api: "OpenBrowserWindow",
    classification: "unsupported",
    pattern: /\bOpenBrowserWindow\b/,
    reason: "opens browser windows with semantics not covered by Floorp runner",
  },
  {
    api: "BrowserTestUtils.addContentEventListener",
    classification: "unsupported",
    pattern: /\bBrowserTestUtils\.addContentEventListener\b/,
    reason: "requires content event listener plumbing",
  },
  {
    api: "BrowserTestUtils.waitForContentEvent",
    classification: "unsupported",
    pattern: /\bBrowserTestUtils\.waitForContentEvent\b/,
    reason: "requires content event listener plumbing",
  },
  {
    api: "BrowserTestUtils.synthesizeKey",
    classification: "unsupported",
    pattern: /\bBrowserTestUtils\.synthesizeKey\b/,
    reason: "requires cross-process input synthesis",
  },
  {
    api: "BrowserTestUtils.synthesizeMouse",
    classification: "unsupported",
    pattern: /\bBrowserTestUtils\.synthesizeMouse\b/,
    reason: "requires cross-process input synthesis",
  },
  {
    api: "BrowserTestUtils.synthesizeMouseAtCenter",
    classification: "unsupported",
    pattern: /\bBrowserTestUtils\.synthesizeMouseAtCenter\b/,
    reason: "requires cross-process input synthesis",
  },
  {
    api: "BrowserTestUtils.synthesizeMouseAtPoint",
    classification: "unsupported",
    pattern: /\bBrowserTestUtils\.synthesizeMouseAtPoint\b/,
    reason: "requires cross-process input synthesis",
  },
  {
    api: "EventUtils",
    classification: "needs-runner-shim",
    pattern: /\bEventUtils\./,
    reason: "requires Mozilla EventUtils compatibility",
  },
  {
    api: "TestUtils.topicObserved",
    classification: "needs-runner-shim",
    pattern: /\bTestUtils\.topicObserved\b/,
    reason: "requires observer-topic waiting compatibility",
  },
  {
    api: "gURLBar",
    classification: "needs-small-shim",
    pattern: /\bgURLBar\b/,
    reason: "uses urlbar globals that need browser-window compatibility",
  },
  {
    api: "BrowserCommands",
    classification: "needs-small-shim",
    pattern: /\bBrowserCommands\b/,
    reason:
      "uses browser command globals that need browser-window compatibility",
  },
];

function normalizeUpstreamPath(value: string): string {
  return value.replaceAll("\\", "/").replace(/^\/+/, "");
}

function ensureString(value: unknown, name: string): string {
  if (typeof value !== "string" || value.trim() === "") {
    throw new Error(`${name} is required`);
  }
  return value;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === "object";
}

async function readJsonFile<T>(filePath: string): Promise<T> {
  const raw = await Deno.readTextFile(filePath);
  return JSON.parse(raw) as T;
}

function assertArray(value: unknown, name: string): unknown[] {
  if (!Array.isArray(value)) {
    throw new Error(`${name} must be an array`);
  }
  return value;
}

function normalizeAllowlistEntry(entry: unknown, index: number): string {
  if (typeof entry === "string") {
    return normalizeUpstreamPath(ensureString(entry, `allowlist[${index}]`));
  }
  if (!isRecord(entry)) {
    throw new Error(`allowlist[${index}] must be a string or object`);
  }
  return normalizeUpstreamPath(
    ensureString(entry.path, `allowlist[${index}].path`),
  );
}

function ensureStringArray(value: unknown, name: string): string[] {
  const entries = assertArray(value, name);
  return entries.map((entry, index) =>
    ensureString(entry, `${name}[${index}]`)
  );
}

function normalizeQuarantineEntry(
  entry: unknown,
  index: number,
): QuarantineEntry {
  if (!isRecord(entry)) {
    throw new Error(`quarantine[${index}] must be an object`);
  }

  const classification = ensureString(
    entry.classification,
    `quarantine[${index}].classification`,
  );
  if (classification !== "quarantined") {
    throw new Error(
      `quarantine[${index}].classification must be "quarantined"`,
    );
  }

  return {
    path: normalizeUpstreamPath(
      ensureString(entry.path, `quarantine[${index}].path`),
    ),
    classification,
    reason: ensureString(entry.reason, `quarantine[${index}].reason`),
    requiredApis: ensureStringArray(
      entry.requiredApis,
      `quarantine[${index}].requiredApis`,
    ),
    sourceRef: ensureString(entry.sourceRef, `quarantine[${index}].sourceRef`),
    lastObserved: ensureString(
      entry.lastObserved,
      `quarantine[${index}].lastObserved`,
    ),
  };
}

function duplicateValues(values: Iterable<string>): string[] {
  const seen = new Set<string>();
  const duplicates = new Set<string>();
  for (const value of values) {
    if (seen.has(value)) {
      duplicates.add(value);
    }
    seen.add(value);
  }
  return [...duplicates].sort();
}

function sortedUnique(values: Iterable<string>): string[] {
  return [...new Set(values)].sort();
}

function formatRelative(from: string, to: string): string {
  return path.relative(from, to).replaceAll("\\", "/");
}

function detectBrowserTestUtilsMethods(source: string): TriageFinding[] {
  const findings: TriageFinding[] = [];
  const methodPattern = /\bBrowserTestUtils\.([A-Za-z_$][\w$]*)\b/g;
  for (const match of source.matchAll(methodPattern)) {
    const method = match[1];
    const api = `BrowserTestUtils.${method}`;
    if (SUPPORTED_BROWSER_TEST_UTILS.has(method)) {
      findings.push({
        api,
        classification: "needs-runner-shim",
        reason: "uses BrowserTestUtils compatibility",
      });
    } else {
      findings.push({
        api,
        classification: "needs-runner-shim",
        reason: "uses an unsupported BrowserTestUtils method",
      });
    }
  }
  return findings;
}

function detectStaticRuleFindings(source: string): TriageFinding[] {
  return STATIC_RULES
    .filter((rule) => rule.pattern.test(source))
    .map((rule) => ({
      api: rule.api,
      classification: rule.classification,
      reason: rule.reason,
    }));
}

function highestRuleClassification(
  findings: TriageFinding[],
): RuleClassification | undefined {
  return findings.reduce<RuleClassification | undefined>(
    (highest, finding) => {
      if (!highest) {
        return finding.classification;
      }
      return RULE_PRIORITY[finding.classification] > RULE_PRIORITY[highest]
        ? finding.classification
        : highest;
    },
    undefined,
  );
}

function classifySource(source: string): {
  classification: Exclude<
    TriageClassification,
    "quarantined" | "already-allowed"
  >;
  findings: TriageFinding[];
} {
  const findings = [
    ...detectStaticRuleFindings(source),
    ...detectBrowserTestUtilsMethods(source),
  ];
  const highest = highestRuleClassification(findings);
  return {
    classification: highest ?? "direct",
    findings,
  };
}

function incrementCount(
  counts: Record<TriageClassification, number>,
  classification: TriageClassification,
): void {
  counts[classification] += 1;
}

function emptyClassificationCounts(): Record<TriageClassification, number> {
  return {
    direct: 0,
    "needs-small-shim": 0,
    "needs-runner-shim": 0,
    unsupported: 0,
    quarantined: 0,
    "already-allowed": 0,
  };
}

function compareTriageEntries(left: TriageEntry, right: TriageEntry): number {
  return CLASSIFICATION_ORDER[right.classification] -
      CLASSIFICATION_ORDER[left.classification] ||
    left.path.localeCompare(right.path);
}

function buildMarkdownReport(manifest: TriageManifest): string {
  const countRows = Object.entries(manifest.counts.classifications)
    .sort(([left], [right]) => left.localeCompare(right))
    .map(([classification, count]) => `| ${classification} | ${count} |`);

  const testRows = manifest.tests
    .slice()
    .sort(compareTriageEntries)
    .map((entry) =>
      `| \`${entry.classification}\` | \`${entry.path}\` | ${
        entry.requiredApis.map((api) => `\`${api}\``).join(", ") || "-"
      } | ${entry.reasons.join("; ") || "-"} |`
    );

  return [
    "# Firefox Browser Test Triage",
    "",
    `Source repository: \`${manifest.source.repository}\``,
    `Source ref: \`${manifest.source.ref}\``,
    `Source commit: \`${manifest.source.commit}\``,
    `Generated at: \`${manifest.generatedAt}\``,
    "",
    "## Counts",
    "",
    "| Classification | Candidates |",
    "| --- | ---: |",
    ...countRows,
    "",
    "## Candidates",
    "",
    "| Classification | Path | Required APIs | Reasons |",
    "| --- | --- | --- | --- |",
    ...testRows,
    "",
  ].join("\n");
}

async function readCandidateSource(
  collectionDir: string,
  collectionManifest: FirefoxTestCollectionManifest,
  candidate: BrowserChromeCandidate,
): Promise<string> {
  const file = collectionManifest.files?.find((entry) =>
    entry.path === candidate.path
  );
  if (!file) {
    throw new Error(
      `Browser chrome candidate is missing from collection manifest: ${candidate.path}`,
    );
  }
  const filePath = path.join(collectionDir, ...file.outputPath.split("/"));
  return await Deno.readTextFile(filePath);
}

function validateQuarantineEntries(
  entries: QuarantineEntry[],
  candidatePaths: Set<string>,
  allowlistPaths: Set<string>,
): Map<string, QuarantineEntry> {
  const duplicates = duplicateValues(entries.map((entry) => entry.path));
  if (duplicates.length > 0) {
    throw new Error(
      `Duplicate quarantined Firefox test path(s): ${duplicates.join(", ")}`,
    );
  }

  for (const entry of entries) {
    if (!candidatePaths.has(entry.path)) {
      throw new Error(
        `Quarantined Firefox test is missing from browser-chrome candidates: ${entry.path}`,
      );
    }
    if (allowlistPaths.has(entry.path)) {
      throw new Error(
        `Quarantined Firefox test also appears in allowlist: ${entry.path}`,
      );
    }
  }

  return new Map(entries.map((entry) => [entry.path, entry]));
}

export async function triageFirefoxBrowserTests(
  options: TriageFirefoxBrowserTestsOptions,
): Promise<TriageManifest> {
  const collectionDir = path.resolve(options.collectionDir);
  const allowlistPath = path.resolve(options.allowlistPath);
  const quarantinePath = path.resolve(options.quarantinePath);
  const outputDir = path.resolve(options.outputDir);

  const collectionManifest = await readJsonFile<FirefoxTestCollectionManifest>(
    path.join(collectionDir, "manifest.json"),
  );
  const candidates = await readJsonFile<BrowserChromeCandidate[]>(
    path.join(collectionDir, "browser-chrome-candidates.json"),
  );
  const allowlistRaw = await readJsonFile<AllowlistEntry[]>(allowlistPath);
  const quarantineRaw = await readJsonFile<QuarantineEntry[]>(quarantinePath);

  const candidatePaths = new Set(candidates.map((candidate) => candidate.path));
  const allowlistPaths = new Set(
    assertArray(allowlistRaw, "allowlist").map(normalizeAllowlistEntry),
  );
  const quarantineEntries = assertArray(quarantineRaw, "quarantine").map(
    normalizeQuarantineEntry,
  );
  const quarantineByPath = validateQuarantineEntries(
    quarantineEntries,
    candidatePaths,
    allowlistPaths,
  );

  const counts = emptyClassificationCounts();
  const tests: TriageEntry[] = [];

  for (const candidate of candidates) {
    const source = await readCandidateSource(
      collectionDir,
      collectionManifest,
      candidate,
    );
    const sourceClassification = classifySource(source);
    const quarantine = quarantineByPath.get(candidate.path);
    const classification: TriageClassification = allowlistPaths.has(
        candidate.path,
      )
      ? "already-allowed"
      : quarantine
      ? "quarantined"
      : sourceClassification.classification;
    const requiredApis = sortedUnique([
      ...sourceClassification.findings.map((finding) => finding.api),
      ...(quarantine?.requiredApis ?? []),
    ]);
    const reasons = sortedUnique([
      ...sourceClassification.findings.map((finding) => finding.reason),
      ...(quarantine ? [quarantine.reason] : []),
    ]);

    incrementCount(counts, classification);
    tests.push({
      path: candidate.path,
      directory: candidate.directory,
      classification,
      detectedApis: sortedUnique(
        sourceClassification.findings.map((finding) => finding.api),
      ),
      requiredApis,
      reasons,
      nearestManifest: candidate.nearestManifest,
      hasHeadJs: candidate.hasHeadJs,
      supportFileCount: candidate.supportFileCount,
      size: candidate.size,
      sha256: candidate.sha256,
      quarantine: quarantine
        ? {
          reason: quarantine.reason,
          sourceRef: quarantine.sourceRef,
          lastObserved: quarantine.lastObserved,
        }
        : undefined,
    });
  }

  const manifest: TriageManifest = {
    schemaVersion: 1,
    generatedAt: new Date().toISOString(),
    source: {
      repository: collectionManifest.source?.repository ?? "",
      ref: collectionManifest.source?.ref ?? "",
      commit: collectionManifest.source?.commit ?? "",
    },
    collectionDir: formatRelative(Deno.cwd(), collectionDir),
    allowlistPath: formatRelative(Deno.cwd(), allowlistPath),
    quarantinePath: formatRelative(Deno.cwd(), quarantinePath),
    counts: {
      candidates: candidates.length,
      classifications: counts,
    },
    tests,
  };

  await Deno.mkdir(outputDir, { recursive: true });
  await Deno.writeTextFile(
    path.join(outputDir, "triage.json"),
    `${JSON.stringify(manifest, null, 2)}\n`,
  );
  await Deno.writeTextFile(
    path.join(outputDir, "TRIAGE.md"),
    buildMarkdownReport(manifest),
  );

  return manifest;
}

function usage(): string {
  return [
    "Usage: deno task firefox-tests:triage-browser [options]",
    "",
    "Options:",
    `  --collection <path>   Collected Firefox test directory (default: ${DEFAULT_COLLECTION_DIR})`,
    `  --allowlist <path>    Allowlist JSON file (default: ${DEFAULT_ALLOWLIST_PATH})`,
    `  --quarantine <path>   Quarantine JSON file (default: ${DEFAULT_QUARANTINE_PATH})`,
    "  --out <path>          Triage output directory (default: --collection)",
    "  --help, -h            Show this help",
  ].join("\n");
}

export async function main(args: string[]): Promise<void> {
  const parsed = parseArgs(args, {
    string: ["collection", "allowlist", "quarantine", "out"],
    boolean: ["help"],
    alias: { h: "help" },
    default: {
      collection: DEFAULT_COLLECTION_DIR,
      allowlist: DEFAULT_ALLOWLIST_PATH,
      quarantine: DEFAULT_QUARANTINE_PATH,
    },
  });

  if (parsed.help) {
    console.log(usage());
    return;
  }
  if (parsed._.length > 0) {
    throw new Error(`Unexpected positional arguments: ${parsed._.join(" ")}`);
  }

  const collection = ensureString(parsed.collection, "--collection");
  const output = typeof parsed.out === "string" ? parsed.out : collection;
  const manifest = await triageFirefoxBrowserTests({
    collectionDir: collection,
    allowlistPath: ensureString(parsed.allowlist, "--allowlist"),
    quarantinePath: ensureString(parsed.quarantine, "--quarantine"),
    outputDir: output,
  });

  console.log(
    `Triaged ${manifest.counts.candidates} Firefox browser test candidate(s).`,
  );
  for (
    const [classification, count] of Object.entries(
      manifest.counts.classifications,
    )
  ) {
    console.log(`- ${classification}: ${count}`);
  }
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
