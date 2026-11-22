// SPDX-License-Identifier: MPL-2.0
import type { QaCliResult } from "./fdh_qa_cli_types.ts";
import type { QaDiscoveryCliTestDependencies } from "./fdh_qa_discovery_cli_types.ts";
import type {
  QaDiscoveryOptions,
  QaDiscoveryReport,
} from "./fdh_qa_discovery_types.ts";

const HELP =
  "Usage: fdh qa discover --source-run-id N --source-attempt N --source-head SHA\n" +
  "  --windows-policy require-success|shadow-observation --max-age-ms N\n\n" +
  "All five options are required. Canonical positive safe integers, lowercase 40-hex head,\n" +
  "max age <= 86400000 ms. Fixed repository/workflow; bounded creation-window scan.\n" +
  "Source and QA probe heads are bound independently. Incomplete, ambiguous or uncertain\n" +
  "candidates fail closed. No source timestamp, URL, token, clock or transport override.\n" +
  "Exit: 0 matched, 1 not ready/discovery failed, 2 invalid arguments. Stdout only.\n" +
  "Matching is a read-only observation, not native QA, approval or publication authority.\n";
const KEYS = [
  "source-run-id",
  "source-attempt",
  "source-head",
  "windows-policy",
  "max-age-ms",
] as const;
const SCOPE = {
  scope: "read-only-point-in-time-qa-discovery",
  transportAuthenticityVerified: false,
  apiAuthenticityVerified: false,
  nativeQaVerified: false,
  liveApprovalVerified: false,
  publicationAuthorized: false,
} as const;
const STAGES = ["options", "source", "runList", "candidates", "finalRecheck"];
const CODES = [
  "invalidOptions",
  "sourceRunFailed",
  "runListFailed",
  "paginationIncomplete",
  "candidateCapExceeded",
  "pendingQaRun",
  "noValidRun",
  "ambiguous",
  "uncertainCandidate",
  "changed",
  "finalRecheckFailed",
];
function integer(
  value: unknown,
  max = Number.MAX_SAFE_INTEGER,
): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) &&
    value > 0 && value <= max;
}
function numberToken(value: string | undefined): number | undefined {
  if (value === undefined || !/^[1-9][0-9]{0,15}$/.test(value)) return;
  const parsed = Number(value);
  return integer(parsed) && String(parsed) === value ? parsed : undefined;
}
function parse(argv: readonly string[]): QaDiscoveryOptions | undefined {
  if (
    argv.length < 7 || argv.length > 12 || argv[0] !== "qa" ||
    argv[1] !== "discover"
  ) return;
  const values = new Map<string, string>();
  for (let i = 2; i < argv.length; i++) {
    const token = argv[i];
    if (
      typeof token !== "string" || token.length > 256 || !token.startsWith("--")
    ) return;
    const equal = token.indexOf("=");
    const key = token.slice(2, equal < 0 ? undefined : equal);
    if (!KEYS.some((known) => known === key) || values.has(key)) return;
    const value = equal < 0 ? argv[++i] : token.slice(equal + 1);
    if (
      typeof value !== "string" || !value || value.length > 256 ||
      value.startsWith("--")
    ) return;
    values.set(key, value);
  }
  if (values.size !== KEYS.length) return;
  const sourceRunId = numberToken(values.get("source-run-id"));
  const sourceAttempt = numberToken(values.get("source-attempt"));
  const maxAgeMs = numberToken(values.get("max-age-ms"));
  const sourceHead = values.get("source-head");
  const windowsPolicy = values.get("windows-policy");
  if (
    sourceRunId === undefined || sourceAttempt === undefined ||
    maxAgeMs === undefined ||
    maxAgeMs > 86_400_000 || typeof sourceHead !== "string" ||
    !/^[0-9a-f]{40}$/.test(sourceHead) ||
    (windowsPolicy !== "require-success" &&
      windowsPolicy !== "shadow-observation")
  ) return;
  return Object.freeze({
    sourceRunId,
    sourceAttempt,
    sourceHead,
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
function summarize(
  report: QaDiscoveryReport,
  options: QaDiscoveryOptions,
): QaCliResult {
  if (
    report === null || typeof report !== "object" ||
    report.scope !== SCOPE.scope ||
    report.transportAuthenticityVerified !== false ||
    report.apiAuthenticityVerified !== false ||
    report.nativeQaVerified !== false ||
    report.liveApprovalVerified !== false ||
    report.publicationAuthorized !== false
  ) return failure(1, "discovery", "discoveryFailed");
  if (report.matches === false) {
    const stage = report.stage, code = report.code;
    return STAGES.includes(stage) && CODES.includes(code)
      ? failure(1, stage, code)
      : failure(1, "discovery", "discoveryFailed");
  }
  if (
    report.matches !== true || report.archiveBytesVerified !== true ||
    report.sourceRunId !== options.sourceRunId ||
    report.sourceAttempt !== options.sourceAttempt ||
    report.sourceHead !== options.sourceHead
  ) return failure(1, "discovery", "discoveryFailed");
  const {
    qaRunId,
    qaAttempt,
    qaHead,
    checkedAt,
    runsListed,
    pagesFetched,
    candidatesChecked,
  } = report;
  const time = typeof checkedAt === "string" ? Date.parse(checkedAt) : NaN;
  const now = Date.now();
  if (
    !integer(qaRunId) || qaRunId === options.sourceRunId ||
    !integer(qaAttempt) ||
    typeof qaHead !== "string" || !/^[0-9a-f]{40}$/.test(qaHead) ||
    !Number.isFinite(time) ||
    new Date(time).toISOString() !== checkedAt || time > now ||
    now - time > options.maxAgeMs ||
    !integer(runsListed, 300) || !integer(pagesFetched, 3) ||
    !integer(candidatesChecked, 8) ||
    candidatesChecked > runsListed
  ) return failure(1, "discovery", "discoveryFailed");
  return {
    exitCode: 0,
    stdout: JSON.stringify({
      ...SCOPE,
      matches: true,
      sourceRunId: options.sourceRunId,
      sourceAttempt: options.sourceAttempt,
      sourceHead: options.sourceHead,
      qaRunId,
      qaAttempt,
      qaHead,
      checkedAt,
      archiveBytesVerified: true,
      runsListed,
      pagesFetched,
      candidatesChecked,
    }),
  };
}
/** Raw tokens retain duplicates; help/invalid input never imports discovery/transport. */
export async function runQaDiscoveryCli(
  argv: readonly string[],
  deps?: QaDiscoveryCliTestDependencies,
): Promise<QaCliResult> {
  if (
    argv.length === 3 && argv[0] === "qa" && argv[1] === "discover" &&
    (argv[2] === "--help" || argv[2] === "-h")
  ) return { exitCode: 0, stdout: HELP };
  const options = parse(argv);
  if (!options) return failure(2, "options", "invalidArguments");
  try {
    const discover = deps?.discover ??
      (await import("./fdh_qa_discovery.ts")).discoverAndCollectFinalPackageQa;
    const result = summarize(await discover(options), options);
    if (new TextEncoder().encode(result.stdout + "\n").length > 256 * 1024) {
      return failure(1, "discovery", "discoveryFailed");
    }
    return result;
  } catch {
    return failure(1, "discovery", "discoveryFailed");
  }
}
