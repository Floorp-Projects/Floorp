// SPDX-License-Identifier: MPL-2.0
import type {
  QaCollectionReport,
  QaCollectorOptions,
} from "./fdh_final_package_qa_collector_types.ts";
import type { QaCliResult, QaCliTestDependencies } from "./fdh_qa_cli_types.ts";

const HELP =
  "Usage: fdh qa collect --source-run-id N --source-attempt N --source-head SHA\n" +
  "  --qa-run-id N --qa-attempt N --qa-head SHA\n" +
  "  --windows-policy require-success|shadow-observation --max-age-ms N\n\n" +
  "All eight options are required. IDs/attempts and max age are canonical positive\n" +
  "safe integers; heads are 40 lowercase hex characters. Run IDs must differ.\n" +
  "Max age is at most 86400000 ms (24 hours). No repository, host, URL, token,\n" +
  "executable, clock, or transport override is supported. Output is stdout only.\n" +
  "Exit: 0 matched, 1 not ready/collection failed, 2 invalid arguments.\n" +
  "Matching is nonauthorizing: native QA/live approval/publication remain unverified.\n";
const KEYS = [
  "source-run-id",
  "source-attempt",
  "source-head",
  "qa-run-id",
  "qa-attempt",
  "qa-head",
  "windows-policy",
  "max-age-ms",
] as const;
const SCOPE = {
  scope: "read-only-point-in-time-qa-collection",
  transportAuthenticityVerified: false,
  apiAuthenticityVerified: false,
  nativeQaVerified: false,
  liveApprovalVerified: false,
  publicationAuthorized: false,
} as const;
const STAGES = [
  "options",
  "identity",
  "inventory",
  "versions",
  "eligibility",
  "archives",
  "evidence",
  "recheck",
];
const CODES = [
  "invalidOptions",
  "collectionFailed",
  "sourceNotReady",
  "evidenceMismatch",
  "changed",
];

function positiveInteger(value: string | undefined): number | undefined {
  if (value === undefined || !/^[1-9][0-9]{0,15}$/.test(value)) return;
  const n = Number(value);
  return Number.isSafeInteger(n) && n > 0 && String(n) === value
    ? n
    : undefined;
}

/** Deliberately parses raw tokens, retaining duplicates even across =/space forms. */
function parseOptions(argv: readonly string[]): QaCollectorOptions | undefined {
  if (
    argv.length < 10 || argv.length > 18 || argv[0] !== "qa" ||
    argv[1] !== "collect"
  ) return;
  const values = new Map<string, string>();
  for (let i = 2; i < argv.length; i++) {
    const token = argv[i];
    if (
      typeof token !== "string" || token.length > 256 || !token.startsWith("--")
    ) return;
    const equal = token.indexOf("=");
    const key = token.slice(2, equal === -1 ? undefined : equal);
    if (!KEYS.some((known) => known === key) || values.has(key)) return;
    const value = equal === -1 ? argv[++i] : token.slice(equal + 1);
    if (
      typeof value !== "string" || value.length === 0 || value.length > 256 ||
      value.startsWith("--")
    ) return;
    values.set(key, value);
  }
  if (values.size !== KEYS.length) return;
  const sourceRunId = positiveInteger(values.get("source-run-id"));
  const sourceAttempt = positiveInteger(values.get("source-attempt"));
  const qaRunId = positiveInteger(values.get("qa-run-id"));
  const qaAttempt = positiveInteger(values.get("qa-attempt"));
  const maxAgeMs = positiveInteger(values.get("max-age-ms"));
  const sourceHead = values.get("source-head")!;
  const qaHead = values.get("qa-head")!;
  const windowsPolicy = values.get("windows-policy");
  if (
    sourceRunId === undefined || sourceAttempt === undefined ||
    qaRunId === undefined || qaAttempt === undefined ||
    maxAgeMs === undefined || maxAgeMs > 86_400_000 ||
    sourceRunId === qaRunId || sourceHead.length !== 40 ||
    qaHead.length !== 40 || !/^[0-9a-f]{40}$/.test(sourceHead) ||
    !/^[0-9a-f]{40}$/.test(qaHead) ||
    (windowsPolicy !== "require-success" &&
      windowsPolicy !== "shadow-observation")
  ) return;
  return Object.freeze({
    sourceRunId,
    sourceAttempt,
    sourceHead,
    qaRunId,
    qaAttempt,
    qaHead,
    windowsPolicy,
    maxAgeMs,
  });
}

function failure(exitCode: 1 | 2, stage: string, code: string): QaCliResult {
  return {
    exitCode,
    stdout: JSON.stringify({ ...SCOPE, matches: false, stage, code }),
  };
}

/** Project only fixed codes and independently validated identities, never API data. */
function summarize(
  report: QaCollectionReport,
  options: QaCollectorOptions,
): QaCliResult {
  if (
    report.scope !== SCOPE.scope ||
    report.transportAuthenticityVerified !== false ||
    report.apiAuthenticityVerified !== false ||
    report.nativeQaVerified !== false ||
    report.liveApprovalVerified !== false ||
    report.publicationAuthorized !== false
  ) return failure(1, "collection", "collectionFailed");
  if (report.matches === false) {
    // Snapshot once: a getter must not swap a validated code before serialization.
    const stage = report.stage;
    const code = report.code;
    if (!STAGES.includes(stage) || !CODES.includes(code)) {
      return failure(1, "collection", "collectionFailed");
    }
    return failure(1, stage, code);
  }
  if (
    report.matches !== true || report.archiveBytesVerified !== true ||
    report.sourceRunId !== options.sourceRunId ||
    report.sourceAttempt !== options.sourceAttempt ||
    report.qaRunId !== options.qaRunId || report.qaAttempt !== options.qaAttempt
  ) return failure(1, "collection", "collectionFailed");
  return {
    exitCode: 0,
    stdout: JSON.stringify({
      ...SCOPE,
      matches: true,
      sourceRunId: options.sourceRunId,
      sourceAttempt: options.sourceAttempt,
      qaRunId: options.qaRunId,
      qaAttempt: options.qaAttempt,
      archiveBytesVerified: true,
    }),
  };
}

/** Full raw argv beginning with qa. Help/invalid never import or invoke collector. */
export async function runQaCli(
  rawArgv: readonly string[],
  testDeps?: QaCliTestDependencies,
): Promise<QaCliResult> {
  if (
    (rawArgv.length === 2 && rawArgv[0] === "qa" &&
      (rawArgv[1] === "--help" || rawArgv[1] === "-h" ||
        rawArgv[1] === "help")) ||
    (rawArgv.length === 3 && rawArgv[0] === "qa" && rawArgv[1] === "collect" &&
      (rawArgv[2] === "--help" || rawArgv[2] === "-h"))
  ) return { exitCode: 0, stdout: HELP };
  const options = parseOptions(rawArgv);
  if (!options) return failure(2, "options", "invalidArguments");
  try {
    const collect = testDeps?.collect ??
      (await import("./fdh_final_package_qa_collector.ts"))
        .collectFinalPackageQa;
    const result = summarize(await collect(options), options);
    // The projection is constant-size today; retain a wire-byte guard for changes.
    if (new TextEncoder().encode(result.stdout + "\n").length > 256 * 1024) {
      return failure(1, "collection", "collectionFailed");
    }
    return result;
  } catch {
    // Never print exception messages: gh stderr/API bodies may contain credentials.
    return failure(1, "collection", "collectionFailed");
  }
}
