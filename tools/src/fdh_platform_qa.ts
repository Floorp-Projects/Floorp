// SPDX-License-Identifier: MPL-2.0

/**
 * Cross-platform Agentic QA step (KAN-145): one run of a packaged Floorp on a
 * QA target (the macOS VM or the Linux container, KAN-144), from preflight to
 * cleanup, as a single command. `fdh job start --step platform` runs it under
 * the durable executor (B-02, D-23). A lost attempt is rerun from the start:
 * the run first removes whatever this task left on the target, so a rerun is
 * safe and never resumes against half-known remote state.
 *
 * This is an early development check on one machine per platform. It is not
 * QA3, release qualification or a visual judgment.
 */

import { decodeBase64 } from "@std/encoding/base64";
import { encodeHex } from "@std/encoding/hex";
import * as path from "@std/path";
import {
  createSshTransport,
  MacosQaTarget,
  parseMacosQaTargetConfig,
} from "./fdh_qa_macos.ts";
import type { MacosPayloadKind } from "./fdh_qa_macos_types.ts";
import {
  createLinuxTransport,
  LinuxQaTarget,
  parseLinuxQaTargetConfig,
} from "./fdh_qa_linux.ts";
import type { LinuxPayloadKind } from "./fdh_qa_linux_types.ts";
import { QaTargetError } from "./fdh_qa_remote.ts";
import { taskStatePath, validateTaskId } from "./fdh_task.ts";
import {
  CLEAN_WORKTREE_STATE,
  isTerminal,
  listJobIds,
  readJob,
} from "./fdh_executor.ts";
import type {
  PlatformQaCliDeps,
  PlatformQaCliResult,
  PlatformQaIo,
  PlatformQaOutcome,
  PlatformQaPreflight,
  PlatformQaRecord,
  PlatformQaReport,
  PlatformQaRunOptions,
  PlatformQaStepRecord,
  PlatformQaTarget,
  QaPlatform,
} from "./fdh_platform_qa_types.ts";

export type {
  PlatformQaAttachment,
  PlatformQaCliDeps,
  PlatformQaCliResult,
  PlatformQaIo,
  PlatformQaOutcome,
  PlatformQaRecord,
  PlatformQaReport,
  PlatformQaRunOptions,
  PlatformQaTarget,
  QaPlatform,
} from "./fdh_platform_qa_types.ts";

export const QA_PLATFORMS: readonly QaPlatform[] = ["macos", "linux"];

const SHA256 = /^[0-9a-f]{64}$/;

/** Assertion recorded for a platform step job. */
export function platformAssertionName(platform: QaPlatform): string {
  return `platform-qa-${platform}`;
}

export function parsePlatform(value: string): QaPlatform {
  if (!(QA_PLATFORMS as readonly string[]).includes(value)) {
    throw new Error(
      `--platform must be ${QA_PLATFORMS.join(" or ")}, got ${
        JSON.stringify(value)
      }`,
    );
  }
  return value as QaPlatform;
}

/** Payload kind from the file name; each target takes its own formats. */
export function payloadKindFor(
  platform: QaPlatform,
  file: string,
): MacosPayloadKind | LinuxPayloadKind {
  const name = file.toLowerCase();
  const kinds: readonly string[] = platform === "macos"
    ? ["dmg", "tar.gz"]
    : ["tar.xz", "tar.bz2", "tar.gz"];
  const kind = kinds.find((k) => name.endsWith(`.${k}`));
  if (kind === undefined) {
    throw new Error(
      `--payload for ${platform} must end in ${
        kinds.map((k) => `.${k}`).join(", ")
      }`,
    );
  }
  return kind as MacosPayloadKind | LinuxPayloadKind;
}

/** What every run checks, in the browser chrome scope. */
export const PROBE_SCRIPT =
  "const w = Services.wm.getMostRecentWindow('navigator:browser');" +
  " return { app: Services.appinfo.name, version: Services.appinfo.version," +
  " buildId: Services.appinfo.appBuildID, os: Services.appinfo.OS," +
  " windows: [...Services.wm.getEnumerator('navigator:browser')].length," +
  " url: w?.gBrowser?.currentURI?.spec ?? null," +
  " contentSandboxLevel: Services.prefs.getIntPref('security.sandbox.content.level', -1) };";

const EXPECTED_OS: Record<QaPlatform, string> = {
  macos: "Darwin",
  linux: "Linux",
};

/** Probe problems; empty when the browser looks right for the platform. */
export function probeProblems(
  platform: QaPlatform,
  probe: unknown,
): string[] {
  if (probe === null || typeof probe !== "object") return ["probe-not-object"];
  const p = probe as Record<string, unknown>;
  const problems: string[] = [];
  if (p.os !== EXPECTED_OS[platform]) problems.push("probe-os-mismatch");
  if (!(typeof p.windows === "number" && p.windows >= 1)) {
    problems.push("probe-no-browser-window");
  }
  return problems;
}

/** The value as recorded and matched by --expect: strings as they are. */
export function evalText(value: unknown): string {
  return typeof value === "string" ? value : JSON.stringify(value) ?? "null";
}

/**
 * Replaces the given strings (host names, users, key paths) inside every
 * string value, as whole words: "10.0.0.5" does not match inside "10.0.0.50",
 * and an alias "mac" leaves "macos" alone. Object keys are kept. Longer words
 * go first, so "user@host" is replaced before "host".
 */
export function redact<T>(value: T, secrets: readonly string[]): T {
  const words = [...new Set(secrets.filter((s) => s.length >= 3))]
    .sort((a, b) => b.length - a.length);
  const patterns = words.map((word) =>
    new RegExp(
      `(?<![A-Za-z0-9_])${
        word.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")
      }(?![A-Za-z0-9_])`,
      "g",
    )
  );
  const walk = (v: unknown): unknown => {
    if (typeof v === "string") {
      return patterns.reduce((text, re) => text.replace(re, "<redacted>"), v)
        // ssh names the resolved address when hostSsh is an alias.
        .replace(IPV4, "<redacted>");
    }
    if (Array.isArray(v)) return v.map(walk);
    if (v !== null && typeof v === "object") {
      return Object.fromEntries(
        Object.entries(v as Record<string, unknown>).map((
          [k, x],
        ) => [k, walk(x)]),
      );
    }
    return v;
  };
  return walk(value) as T;
}

/** Any IPv4 address, not part of a longer dotted number. */
const IPV4 = /(?<![0-9.])(?:\d{1,3}\.){3}\d{1,3}(?![0-9.])/g;

/** Same rule as the targets' launch scripts, checked before a job starts. */
export function validateStartUrl(startUrl: string): string {
  if (!/^(about:[a-z-]+|https?:\/\/[A-Za-z0-9.:/_-]{1,200})$/.test(startUrl)) {
    throw new Error("--start-url must be about:<page> or a plain http(s) URL");
  }
  return startUrl;
}

/**
 * Platform jobs of a task that are not finished. The task id is the target's
 * only lock identity, so a second run of the same task and platform must not
 * start (its reset would remove the live run's target) while one is listed.
 */
export function activePlatformJobs(
  worktree: string,
  taskId: string,
  platform: QaPlatform,
): string[] {
  return listJobIds(worktree, taskId).filter((jobId) => {
    const job = readJob(worktree, taskId, jobId);
    return job.kind === "platform" && !isTerminal(job.state) &&
      (job.step?.commands ?? []).some((c) =>
        c.argv.includes(`--platform=${platform}`)
      );
  });
}

/** Report path of a platform step, relative to the worktree (KAN-146). */
export function platformReportRef(
  taskId: string,
  platform: QaPlatform,
): string {
  validateTaskId(taskId);
  return `_dist/fdh/platform/${taskId}/${platform}-report.json`;
}

async function sha256Bytes(bytes: Uint8Array): Promise<string> {
  const copy = new Uint8Array(bytes.length);
  copy.set(bytes);
  return encodeHex(
    new Uint8Array(await crypto.subtle.digest("SHA-256", copy)),
  );
}

function isRecordObject(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

/** The target as the report's preflight saw it, or null. */
function targetOf(report: Record<string, unknown>): string | null {
  const steps = Array.isArray(report.steps) ? report.steps : [];
  const preflight = steps.find((s) =>
    isRecordObject(s) && s.name === "preflight"
  );
  const result = isRecordObject(preflight) ? preflight.result : null;
  const details = isRecordObject(result) ? result.details : null;
  if (!isRecordObject(details)) return null;
  if (typeof details.image === "string") {
    return `Linux container image ${details.image}`;
  }
  if (typeof details.osVersion === "string") {
    return `macOS VM, macOS ${details.osVersion}`;
  }
  return null;
}

export interface PlatformRecordInput {
  worktree: string;
  taskId: string;
  platform: QaPlatform;
  jobId: string;
  /** `platformReportRef` of the step. */
  reportRef: string;
  /** The job's journal inputs. */
  head: string;
  worktreeState?: string;
}

/**
 * Read a settled platform job's report and screenshot and pin them (KAN-146).
 * Never throws: whatever does not hold up goes into `problems`, and the
 * caller records a failed assertion for it.
 */
export async function readPlatformQaRecord(
  input: PlatformRecordInput,
): Promise<PlatformQaRecord> {
  const record: PlatformQaRecord = {
    platform: input.platform,
    jobId: input.jobId,
    head: input.head.trim().toLowerCase(),
    cleanWorktree: input.worktreeState === CLEAN_WORKTREE_STATE,
    outcome: null,
    payload: null,
    target: null,
    attachments: [],
    problems: [],
  };
  if (input.reportRef !== platformReportRef(input.taskId, input.platform)) {
    record.problems.push(`unexpected report path ${input.reportRef}`);
    return record;
  }
  const read = (ref: string): Uint8Array | null => {
    try {
      return Deno.readFileSync(path.join(input.worktree, ...ref.split("/")));
    } catch {
      return null;
    }
  };
  const bytes = read(input.reportRef);
  if (bytes === null) {
    record.problems.push(`report ${input.reportRef} is not readable`);
    return record;
  }
  record.attachments.push({
    ref: input.reportRef,
    sha256: await sha256Bytes(bytes),
    bytes: bytes.length,
  });
  let report: unknown;
  try {
    report = JSON.parse(new TextDecoder().decode(bytes));
  } catch {
    record.problems.push("the report is not JSON");
    return record;
  }
  if (
    !isRecordObject(report) || report.schema !== "fdh-platform-qa-report/v1"
  ) {
    record.problems.push("the report is not an fdh-platform-qa-report/v1");
    return record;
  }
  if (report.taskId !== input.taskId || report.platform !== input.platform) {
    record.problems.push(
      `the report is for ${String(report.taskId)}/${String(report.platform)}`,
    );
    return record;
  }
  const outcome = report.outcome;
  if (outcome === "passed" || outcome === "failed" || outcome === "blocked") {
    record.outcome = outcome;
  } else {
    record.problems.push("the report has no outcome");
  }
  const payload = report.payload;
  if (
    isRecordObject(payload) && typeof payload.name === "string" &&
    typeof payload.sha256 === "string" && SHA256.test(payload.sha256)
  ) {
    record.payload = {
      name: payload.name,
      sha256: payload.sha256,
      version: typeof payload.version === "string" ? payload.version : null,
    };
  } else {
    record.problems.push("the report has no payload hash");
  }
  record.target = targetOf(report);
  const shot = report.screenshot;
  if (shot !== null && shot !== undefined) {
    const file = isRecordObject(shot) ? shot.file : undefined;
    const claimed = isRecordObject(shot) ? shot.sha256 : undefined;
    if (file !== `${input.platform}.png` || typeof claimed !== "string") {
      record.problems.push("the report's screenshot entry is malformed");
    } else {
      const ref = `${
        input.reportRef.slice(0, input.reportRef.lastIndexOf("/"))
      }/${file}`;
      const png = read(ref);
      if (png === null) {
        record.problems.push(`screenshot ${ref} is not readable`);
      } else {
        const sha256 = await sha256Bytes(png);
        record.attachments.push({ ref, sha256, bytes: png.length });
        if (sha256 !== claimed) {
          record.problems.push(
            `screenshot ${ref} does not match the report (sha256 ${sha256})`,
          );
        }
      }
    }
  } else if (record.outcome === "passed") {
    record.problems.push("a passed report has no screenshot");
  }
  return record;
}

/** The assertion text of a settled platform step. */
export function platformActual(
  exitCode: number,
  record: PlatformQaRecord,
): string {
  const report = record.attachments.find((a) => a.ref.endsWith("-report.json"));
  return [
    `exit code ${exitCode}`,
    `outcome ${record.outcome ?? "unknown"}`,
    `at ${record.head} (${
      record.cleanWorktree ? "clean worktree" : "uncommitted changes"
    })`,
    record.payload === null
      ? "payload unknown"
      : `payload ${record.payload.name} sha256 ${record.payload.sha256}`,
    `target ${record.target ?? "unknown"}`,
    report === undefined ? "no report" : `report sha256 ${report.sha256}`,
    ...(record.problems.length > 0
      ? [`problems: ${record.problems.join("; ")}`]
      : []),
  ].join("; ");
}

function userOf(sshDestination: string): string | null {
  const at = sshDestination.lastIndexOf("@");
  return at > 0 ? sshDestination.slice(0, at) : null;
}

function hostOf(sshDestination: string): string {
  const at = sshDestination.lastIndexOf("@");
  return at >= 0 ? sshDestination.slice(at + 1) : sshDestination;
}

function macosTarget(
  target: MacosQaTarget,
  kind: MacosPayloadKind,
): PlatformQaTarget {
  return {
    platform: "macos",
    async preflight(): Promise<PlatformQaPreflight> {
      const p = await target.preflight();
      return {
        ready: p.ready,
        blockers: p.blockers,
        holder: p.lockHolder,
        needsPrepare: false,
        details: { ...p },
      };
    },
    prepare: () => Promise.resolve({}),
    acquire: (taskId) => target.acquire(taskId),
    stage: (taskId, payload, sha256) =>
      target.stage(taskId, kind, payload, sha256),
    launch: (taskId, startUrl) => target.launch(taskId, startUrl),
    evaluate: (taskId, script) => target.evaluate(taskId, script),
    screenshot: (taskId) => target.screenshot(taskId),
    async cleanup(taskId) {
      return { ...(await target.cleanup(taskId)) };
    },
  };
}

function linuxTarget(
  target: LinuxQaTarget,
  kind: LinuxPayloadKind,
): PlatformQaTarget {
  return {
    platform: "linux",
    async preflight(): Promise<PlatformQaPreflight> {
      const p = await target.preflight();
      return {
        ready: p.ready,
        blockers: p.blockers,
        holder: p.containerHolder,
        needsPrepare: p.blockers.includes("image-missing"),
        details: { ...p, image: target.image },
      };
    },
    async prepare() {
      return { ...(await target.ensureImage()), image: target.image };
    },
    acquire: (taskId) => target.acquire(taskId),
    stage: (taskId, payload, sha256) =>
      target.stage(taskId, kind, payload, sha256),
    launch: (taskId, startUrl) => target.launch(taskId, startUrl),
    evaluate: (taskId, script) => target.evaluate(taskId, script),
    screenshot: (taskId) => target.screenshot(taskId),
    async cleanup(taskId) {
      return { ...(await target.cleanup(taskId)) };
    },
  };
}

/**
 * The real target for a platform from its config JSON. Also returns the
 * strings (host names) that must not appear in reports.
 */
export async function createPlatformTarget(
  platform: QaPlatform,
  config: unknown,
  payloadFile?: string,
): Promise<{ target: PlatformQaTarget; redactions: string[] }> {
  if (platform === "macos") {
    const c = parseMacosQaTargetConfig(config);
    return {
      target: macosTarget(
        new MacosQaTarget(c, createSshTransport(c)),
        payloadFile === undefined
          ? "dmg"
          : payloadKindFor("macos", payloadFile) as MacosPayloadKind,
      ),
      redactions: [
        c.hostSsh,
        hostOf(c.hostSsh),
        userOf(c.hostSsh),
        c.guestAddress,
        c.vmDir,
        c.guestKeyFile,
        c.guestKnownHostsFile,
        c.guestUser,
        c.testUser,
        c.qaRoot,
      ].filter((s): s is string => s !== null),
    };
  }
  const c = parseLinuxQaTargetConfig(config);
  return {
    target: linuxTarget(
      await LinuxQaTarget.create(c, createLinuxTransport(c)),
      payloadFile === undefined
        ? "tar.xz"
        : payloadKindFor("linux", payloadFile) as LinuxPayloadKind,
    ),
    redactions: [c.hostSsh, hostOf(c.hostSsh), userOf(c.hostSsh)].filter((
      s,
    ): s is string => s !== null),
  };
}

export const DENO_IO: PlatformQaIo = {
  async sha256File(file) {
    return encodeHex(
      await crypto.subtle.digest("SHA-256", await Deno.readFile(file)),
    );
  },
  async openFile(file) {
    return (await Deno.open(file)).readable;
  },
  writeFile: (file, data) => Deno.writeFile(file, data),
  async mkdir(dir) {
    await Deno.mkdir(dir, { recursive: true });
  },
  now: () => new Date().toISOString(),
};

class StepFailed extends Error {}

function failureOf(error: unknown): Record<string, unknown> {
  if (error instanceof QaTargetError) {
    return { error: String(error.result.error ?? "unknown"), ...error.result };
  }
  return { error: error instanceof Error ? error.message : String(error) };
}

/**
 * One QA run. Never throws for target problems: the outcome says what
 * happened. Cleanup runs whenever the target was acquired.
 */
export async function runPlatformQa(
  target: PlatformQaTarget,
  options: PlatformQaRunOptions,
  io: PlatformQaIo = DENO_IO,
  redactions: readonly string[] = [],
): Promise<PlatformQaReport> {
  const taskId = validateTaskId(options.taskId);
  if (!SHA256.test(options.payloadSha256)) {
    throw new Error("payload sha256 must be 64 lowercase hex");
  }
  if (options.expect !== undefined && options.evalScript === undefined) {
    throw new Error("--expect needs --eval");
  }
  const platform = target.platform;
  const steps: PlatformQaStepRecord[] = [];
  const startedAt = io.now();
  let reason: string | null = null;
  let blocked = false;

  const step = async <T>(name: string, fn: () => Promise<T>): Promise<T> => {
    const at = io.now();
    try {
      const result = await fn();
      steps.push({
        name,
        ok: true,
        startedAt: at,
        finishedAt: io.now(),
        result,
      });
      return result;
    } catch (error) {
      const failure = failureOf(error);
      steps.push({
        name,
        ok: false,
        startedAt: at,
        finishedAt: io.now(),
        result: failure,
      });
      reason ??= `${name}: ${String(failure.error)}`;
      throw new StepFailed(name);
    }
  };
  const fail = (why: string) => {
    reason ??= why;
    throw new StepFailed(why);
  };

  let version: string | null = null;
  let probe: Record<string, unknown> | null = null;
  let evalRecord: PlatformQaReport["eval"] = null;
  let screenshot: PlatformQaReport["screenshot"] = null;
  let cleanupClean: boolean | null = null;
  let acquired = false;

  try {
    await step("payload", async () => {
      const sha256 = await io.sha256File(options.payloadPath);
      if (sha256 !== options.payloadSha256) {
        throw new Error("payload-sha256-changed");
      }
      return { sha256 };
    });
    try {
      let pre = await step("preflight", () => target.preflight());
      if (pre.holder === taskId) {
        // Left by a lost attempt of this task: start again from clean.
        const reset = await step("reset", () => target.cleanup(taskId));
        if (!reset.clean) fail("reset: target not clean");
        pre = await step("preflight", () => target.preflight());
      }
      // Building the image needs disk and memory: only when nothing else
      // blocks the target.
      if (
        pre.needsPrepare && pre.blockers.every((b) => b === "image-missing")
      ) {
        await step("prepare", () => target.prepare());
        pre = await step("preflight", () => target.preflight());
      }
      if (!pre.ready) fail(`blocked: ${pre.blockers.join(", ")}`);
    } catch (error) {
      blocked = true;
      throw error;
    }
    try {
      await step("acquire", () => target.acquire(taskId));
    } catch (error) {
      const last = steps.at(-1)?.result as Record<string, unknown> | undefined;
      if (last?.error === "lock-held" || last?.error === "container-held") {
        // Another task took the target after preflight: nothing of ours is
        // on it, so there is nothing to clean up.
        blocked = true;
        reason = `blocked: ${String(last.error)}`;
      } else {
        acquired = true;
      }
      throw error;
    }
    acquired = true;
    const staged = await step(
      "stage",
      async () =>
        target.stage(
          taskId,
          await io.openFile(options.payloadPath),
          options.payloadSha256,
        ),
    );
    version = staged.version || null;
    await step("launch", () => target.launch(taskId, options.startUrl));
    const value = await step(
      "probe",
      () => target.evaluate(taskId, PROBE_SCRIPT),
    );
    probe = value !== null && typeof value === "object"
      ? value as Record<string, unknown>
      : { value };
    const problems = probeProblems(platform, value);
    if (problems.length > 0) fail(problems.join(", "));
    if (options.evalScript !== undefined) {
      const script = options.evalScript;
      const result = await step("eval", () => target.evaluate(taskId, script));
      evalRecord = {
        script,
        value: result,
        expect: options.expect ?? null,
      };
      if (
        options.expect !== undefined &&
        !evalText(result).includes(options.expect)
      ) {
        fail("eval-expect-not-found");
      }
    }
    screenshot = await step("screenshot", async () => {
      const png = decodeBase64(await target.screenshot(taskId));
      await io.mkdir(options.outDir);
      const file = path.join(options.outDir, `${platform}.png`);
      await io.writeFile(file, png);
      return {
        file: path.basename(file),
        sha256: encodeHex(await crypto.subtle.digest("SHA-256", png)),
      };
    });
  } catch (error) {
    if (!(error instanceof StepFailed)) {
      reason ??= `internal: ${error instanceof Error ? error.message : error}`;
    }
  } finally {
    if (acquired) {
      try {
        const cleaned = await step("cleanup", () => target.cleanup(taskId));
        cleanupClean = cleaned.clean;
        if (!cleaned.clean) reason ??= "cleanup: target not clean";
      } catch {
        cleanupClean = false;
      }
    }
  }

  const outcome: PlatformQaOutcome = blocked
    ? "blocked"
    : reason === null && cleanupClean === true
    ? "passed"
    : "failed";
  const report: PlatformQaReport = {
    schema: "fdh-platform-qa-report/v1",
    taskId,
    platform,
    outcome,
    reason,
    payload: {
      name: path.basename(options.payloadPath),
      sha256: options.payloadSha256,
      version,
    },
    probe,
    eval: evalRecord,
    screenshot,
    cleanupClean,
    steps,
    startedAt,
    finishedAt: io.now(),
  };
  const safe = redact(report, redactions);
  await io.mkdir(options.outDir);
  await io.writeFile(
    path.join(options.outDir, `${platform}-report.json`),
    new TextEncoder().encode(JSON.stringify(safe, null, 2) + "\n"),
  );
  return safe;
}

/** One-line summary printed last, so the job log tail always holds it. */
export function summaryLine(report: PlatformQaReport, reportFile: string) {
  return JSON.stringify({
    platform: report.platform,
    outcome: report.outcome,
    reason: report.reason,
    payloadSha256: report.payload.sha256,
    version: report.payload.version,
    probe: report.probe,
    eval: report.eval === null
      ? null
      : { value: evalText(report.eval.value), expect: report.eval.expect },
    screenshotSha256: report.screenshot?.sha256 ?? null,
    cleanupClean: report.cleanupClean,
    report: reportFile,
  });
}

const RUN_KEYS = [
  "platform",
  "task-id",
  "target",
  "target-sha256",
  "payload",
  "payload-sha256",
  "out",
  "eval",
  "expect",
  "start-url",
] as const;
const CLEANUP_KEYS = ["platform", "task-id", "target"] as const;

export const PLATFORM_QA_HELP =
  "Usage: fdh platform-qa run --platform <macos|linux> --task-id <id>\n" +
  "         --target <config.json> --target-sha256 <hex> --payload <file>\n" +
  "         --payload-sha256 <hex> --out <dir> [--eval <js> [--expect <text>]]\n" +
  "         [--start-url <about:page|http(s) URL>]\n" +
  "       fdh platform-qa cleanup --platform <macos|linux> --task-id <id> --target <config.json>\n\n" +
  "'run' is what 'fdh job start --step platform' executes; it does not touch the\n" +
  "task state. It writes <out>/<platform>-report.json (and <platform>.png) and\n" +
  "prints a one-line summary. Exit: 0 passed, 1 failed or blocked, 2 bad arguments.\n" +
  "'cleanup' removes what this task left on the target (after a job gave up).\n" +
  "Early development check only: not QA3, release qualification or visual judgment.\n";

function parseOptions(
  argv: string[],
  allowed: readonly string[],
): Map<string, string> {
  const options = new Map<string, string>();
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (!arg.startsWith("--")) throw new Error(`unexpected argument ${arg}`);
    const body = arg.slice(2);
    const eq = body.indexOf("=");
    const key = eq >= 0 ? body.slice(0, eq) : body;
    const value = eq >= 0 ? body.slice(eq + 1) : argv[++i];
    if (value === undefined) throw new Error(`--${key} requires a value`);
    if (!allowed.includes(key)) throw new Error(`unknown option --${key}`);
    if (options.has(key)) throw new Error(`--${key} given twice`);
    options.set(key, value);
  }
  return options;
}

function need(options: Map<string, string>, key: string): string {
  const value = options.get(key);
  if (value === undefined || value === "") {
    throw new Error(`--${key} is required`);
  }
  return value;
}

/** `fdh platform-qa <run|cleanup> ...` (argv without the leading word). */
export async function runPlatformQaCli(
  argv: string[],
  deps: PlatformQaCliDeps = {},
): Promise<PlatformQaCliResult> {
  const io = deps.io ?? DENO_IO;
  const readFile = deps.readFile ?? Deno.readFile;
  const taskStateExists = deps.taskStateExists ??
    ((id: string) => {
      try {
        return Deno.statSync(taskStatePath(Deno.cwd(), id)).isFile;
      } catch {
        return false;
      }
    });
  const createTarget = deps.createTarget ?? createPlatformTarget;
  const [sub, ...rest] = argv;
  if (sub === undefined || sub === "--help" || sub === "help") {
    return { exitCode: sub === undefined ? 2 : 0, stdout: PLATFORM_QA_HELP };
  }
  if (rest.includes("--help")) {
    return { exitCode: 0, stdout: PLATFORM_QA_HELP };
  }
  let options: Map<string, string>;
  try {
    if (sub !== "run" && sub !== "cleanup") {
      throw new Error(`unknown platform-qa command ${JSON.stringify(sub)}`);
    }
    options = parseOptions(rest, sub === "run" ? RUN_KEYS : CLEANUP_KEYS);
    parsePlatform(need(options, "platform"));
    validateTaskId(need(options, "task-id"));
    need(options, "target");
    if (sub === "run") {
      for (const key of ["target-sha256", "payload-sha256"]) {
        if (!SHA256.test(need(options, key))) {
          throw new Error(`--${key} must be 64 lowercase hex`);
        }
      }
      need(options, "payload");
      need(options, "out");
      if (options.has("expect") && !options.has("eval")) {
        throw new Error("--expect needs --eval");
      }
      for (const key of ["eval", "expect"]) {
        if (options.get(key) === "") {
          throw new Error(`--${key} must not be empty`);
        }
      }
      const startUrl = options.get("start-url");
      if (startUrl !== undefined) validateStartUrl(startUrl);
    }
  } catch (error) {
    return {
      exitCode: 2,
      stdout: `[fdh platform-qa] ${
        error instanceof Error ? error.message : error
      }\n${PLATFORM_QA_HELP}`,
    };
  }
  const platform = parsePlatform(need(options, "platform"));
  const taskId = need(options, "task-id");
  const targetFile = need(options, "target");
  const payloadFile = options.get("payload");
  // Local paths (they hold the user's profile name) never reach a report.
  let redactions: string[] = payloadFile === undefined
    ? []
    : [payloadFile, path.resolve(payloadFile)].flatMap((
      file,
    ) => [file, path.dirname(file)]);
  const failed = (why: unknown): PlatformQaCliResult => ({
    exitCode: 1,
    stdout: JSON.stringify(redact({
      platform,
      outcome: "failed",
      reason: `internal: ${why instanceof Error ? why.message : String(why)}`,
    }, redactions)),
  });
  try {
    return await runParsed();
  } catch (error) {
    return failed(error);
  }

  async function runParsed(): Promise<PlatformQaCliResult> {
    // Hash the bytes, as job start does; decoding drops a UTF-8 BOM.
    const configBytes = await readFile(targetFile);
    const configText = new TextDecoder().decode(configBytes);

    if (sub === "cleanup") {
      // The running-job check reads this worktree's journals, so it only
      // means something from the worktree that holds the task.
      if (!taskStateExists(taskId)) {
        return {
          exitCode: 1,
          stdout: JSON.stringify({
            platform,
            cleanup: null,
            refused:
              "no fdh task state for this task here; run cleanup from the task's worktree root",
          }),
        };
      }
      const active = (deps.activeJobs ?? activePlatformJobs)(
        Deno.cwd(),
        taskId,
        platform,
      );
      if (active.length > 0) {
        return {
          exitCode: 1,
          stdout: JSON.stringify({
            platform,
            cleanup: null,
            refused: `platform job ${
              active.join(", ")
            } of this task is not finished; cancel it first (fdh job cancel)`,
          }),
        };
      }
      const created = await createTarget(
        platform,
        JSON.parse(configText),
        payloadFile,
      );
      const target = created.target;
      redactions = [...redactions, ...created.redactions];
      try {
        const result = redact(await target.cleanup(taskId), redactions);
        return {
          exitCode: result.clean ? 0 : 1,
          stdout: JSON.stringify({ platform, cleanup: result }),
        };
      } catch (error) {
        return {
          exitCode: 1,
          stdout: JSON.stringify(
            redact({ platform, cleanup: failureOf(error) }, redactions),
          ),
        };
      }
    }

    const targetSha = encodeHex(
      await crypto.subtle.digest("SHA-256", new Uint8Array(configBytes)),
    );
    const outDir = need(options, "out");
    if (targetSha !== options.get("target-sha256")) {
      const at = io.now();
      const refused: PlatformQaReport = {
        schema: "fdh-platform-qa-report/v1",
        taskId,
        platform,
        outcome: "failed",
        reason: "target config changed since the job started",
        payload: {
          name: path.basename(need(options, "payload")),
          sha256: need(options, "payload-sha256"),
          version: null,
        },
        probe: null,
        eval: null,
        screenshot: null,
        cleanupClean: null,
        steps: [],
        startedAt: at,
        finishedAt: at,
      };
      await io.mkdir(outDir);
      await io.writeFile(
        path.join(outDir, `${platform}-report.json`),
        new TextEncoder().encode(JSON.stringify(refused, null, 2) + "\n"),
      );
      return {
        exitCode: 1,
        stdout: summaryLine(refused, `${platform}-report.json`),
      };
    }
    const created = await createTarget(
      platform,
      JSON.parse(configText),
      payloadFile,
    );
    const target = created.target;
    redactions = [...redactions, ...created.redactions];
    const report = await runPlatformQa(
      target,
      {
        taskId,
        payloadPath: need(options, "payload"),
        payloadSha256: need(options, "payload-sha256"),
        outDir,
        evalScript: options.get("eval"),
        expect: options.get("expect"),
        startUrl: options.get("start-url"),
      },
      io,
      redactions,
    );
    return {
      exitCode: report.outcome === "passed" ? 0 : 1,
      stdout: summaryLine(report, `${platform}-report.json`),
    };
  }
}
