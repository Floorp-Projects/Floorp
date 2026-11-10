#!/usr/bin/env -S deno run -A
// SPDX-License-Identifier: MPL-2.0

/**
 * Design matrix: screenshots and computed styles of every Floorp design ×
 * theme, compared against a baseline run. Use it before and after a Runtime
 * (Gecko) bump. See README.md next to this file.
 *
 *   deno task design-matrix run --exe <floorp.exe> --out <dir> [--baseline <dir>]
 *   deno task design-matrix compare --baseline <dir> --current <dir> [--report <dir>]
 */

import { parseArgs } from "@std/cli/parse-args";
import * as path from "@std/path";
import { CaptureInterrupted, captureMatrix } from "./capture.ts";
import {
  collectProblems,
  compareRuns,
  DEFAULT_COMPARE_OPTIONS,
  readManifest,
  renderMarkdown,
} from "./compare.ts";
import {
  type CompareOptions,
  type Design,
  DESIGNS,
  type ThemeKey,
  THEMES,
} from "./types.ts";

/** Exit codes, as documented in README.md. */
export const EXIT_PASS = 0;
export const EXIT_REGRESSION = 1;
export const EXIT_ERROR = 2;
/** Ctrl+C during `run` (128 + SIGINT); the manifest is marked incomplete. */
export const EXIT_INTERRUPTED = 130;

const D = DEFAULT_COMPARE_OPTIONS;
const USAGE = `Usage:
  deno task design-matrix run --exe <path/to/floorp(.exe)> --out <dir> [--baseline <dir>] [options]
  deno task design-matrix compare --baseline <dir> --current <dir> [--report <dir>] [options]

Capture options:
  --designs a,b      subset of ${DESIGNS.join(",")} (default: all)
  --themes a,b       subset of ${THEMES.join(",")} (default: all)
  --port N           Marionette port (default 2828); the fixture proxy uses N+1
  --headed           show the window instead of -headless
  --width/--height   window size in CSS px (default 1280x800)
  --tmp-dir <dir>    parent for temporary profiles (default: OS temp)

Compare options:
  --tolerance N      per-channel difference ignored, 0-255 (default ${D.tolerance})
  --max-pixels N     changed pixels that fail an image (default ${D.maxPixels})
  --threshold R      changed-pixel ratio that fails an image (default ${D.threshold})
  --max-mask R       largest image share masks may cover (default ${D.maxMaskRatio})
  --ignore-styles    report computed-style differences without failing
  --no-diff-images   do not write highlighted diff PNGs

Exit codes: 0 no regression, 1 regressions found, 2 usage error, capture
problem, incomplete run or any other error, 130 interrupted (Ctrl+C).`;

class UsageError extends Error {}

function parseList<T extends string>(
  raw: string | undefined,
  allowed: readonly T[],
  label: string,
): T[] {
  if (!raw) return [...allowed];
  const items = raw.split(",").map((s) => s.trim()).filter(Boolean);
  for (const item of items) {
    if (!allowed.includes(item as T)) {
      throw new UsageError(
        `Unknown ${label} "${item}" (allowed: ${allowed.join(", ")})`,
      );
    }
  }
  return items as T[];
}

/** Plain decimal digits only: no sign, exponent, prefix or whitespace. */
const INT_RE = /^\d+$/;
/** Plain decimal, optionally with a fraction: "1", "0.002". */
const DECIMAL_RE = /^\d+(?:\.\d+)?$/;

/** A whole number in [min, max]; only plain decimal digits are accepted. */
export function intArg(
  raw: string | undefined,
  fallback: number,
  label: string,
  min: number,
  max: number,
): number {
  if (raw === undefined) return fallback;
  const n = INT_RE.test(raw) ? Number(raw) : NaN;
  if (!Number.isSafeInteger(n) || n < min || n > max) {
    throw new UsageError(
      `Invalid --${label} "${raw}": expected a whole number from ${min} to ${max}`,
    );
  }
  return n;
}

/** A ratio in [0, 1] written as a plain decimal, such as 0.002. */
export function ratioArg(
  raw: string | undefined,
  fallback: number,
  label: string,
): number {
  if (raw === undefined) return fallback;
  // The regex comes first: Number() also accepts "", " 0.5", "0x1", "1e-3".
  const n = DECIMAL_RE.test(raw) ? Number(raw) : NaN;
  if (!Number.isFinite(n) || n < 0 || n > 1) {
    throw new UsageError(
      `Invalid --${label} "${raw}": expected a ratio from 0 to 1 (e.g. 0.002)`,
    );
  }
  return n;
}

async function runCompare(
  baselineDir: string,
  currentDir: string,
  reportDir: string,
  options: CompareOptions,
): Promise<number> {
  const report = await compareRuns(baselineDir, currentDir, reportDir, options);
  await Deno.mkdir(reportDir, { recursive: true });
  await Deno.writeTextFile(
    path.join(reportDir, "report.json"),
    JSON.stringify(report, null, 2) + "\n",
  );
  await Deno.writeTextFile(
    path.join(reportDir, "report.md"),
    renderMarkdown(report),
  );
  console.log(
    `[design-matrix] ${report.status.toUpperCase()}: ${report.failingImages}/${report.images.length} images failing (max ${
      (report.maxRatio * 100).toFixed(3)
    }%), ${report.values.length} style values differ, ${report.problems.length} capture problems`,
  );
  console.log(`[design-matrix] report: ${path.join(reportDir, "report.md")}`);
  return report.status === "error"
    ? EXIT_ERROR
    : report.status === "fail"
    ? EXIT_REGRESSION
    : EXIT_PASS;
}

async function run(argv: string[]): Promise<number> {
  const args = parseArgs(argv, {
    string: [
      "exe",
      "out",
      "baseline",
      "current",
      "report",
      "designs",
      "themes",
      "port",
      "width",
      "height",
      "tmp-dir",
      "tolerance",
      "threshold",
      "max-pixels",
      "max-mask",
    ],
    boolean: ["headed", "ignore-styles", "no-diff-images", "help"],
  });
  const command = String(args._[0] ?? "");
  if (args.help) {
    console.log(USAGE);
    return EXIT_PASS;
  }
  if (!["run", "compare"].includes(command)) {
    throw new UsageError(`Unknown command "${command}"`);
  }

  const options: CompareOptions = {
    tolerance: intArg(args.tolerance, D.tolerance, "tolerance", 0, 255),
    threshold: ratioArg(args.threshold, D.threshold, "threshold"),
    maxPixels: intArg(
      args["max-pixels"],
      D.maxPixels,
      "max-pixels",
      0,
      Number.MAX_SAFE_INTEGER,
    ),
    maxMaskRatio: ratioArg(args["max-mask"], D.maxMaskRatio, "max-mask"),
    failOnStyleDiff: !args["ignore-styles"],
    writeDiffImages: !args["no-diff-images"],
  };

  if (command === "compare") {
    if (!args.baseline || !args.current) {
      throw new UsageError("compare needs --baseline and --current");
    }
    const current = path.resolve(args.current);
    return await runCompare(
      path.resolve(args.baseline),
      current,
      path.resolve(args.report ?? path.join(current, "report")),
      options,
    );
  }

  if (!args.exe || !args.out) {
    throw new UsageError("run needs --exe and --out");
  }
  const designs = parseList<Design>(args.designs, DESIGNS, "design");
  const themes = parseList<ThemeKey>(args.themes, THEMES, "theme");
  // The fixture proxy listens on port + 1, so the port stops at 65534.
  const port = intArg(args.port, 2828, "port", 1, 65534);
  const width = intArg(args.width, 1280, "width", 100, 16384);
  const height = intArg(args.height, 800, "height", 100, 16384);
  const exe = path.resolve(args.exe);
  try {
    await Deno.stat(exe);
  } catch {
    throw new UsageError(`Build not found: ${exe}`);
  }
  // Check the baseline before spending minutes on the capture.
  const baselineDir = args.baseline ? path.resolve(args.baseline) : null;
  if (baselineDir) await readManifest(baselineDir);

  const outDir = path.resolve(args.out);
  const manifest = await captureMatrix({
    exe,
    outDir,
    designs,
    themes,
    port,
    headless: !args.headed,
    width,
    height,
    tmpRoot: args["tmp-dir"] ? path.resolve(args["tmp-dir"]) : undefined,
    log: (msg) => console.error(msg),
  });
  const problems = collectProblems(manifest, "current");
  console.log(
    `[design-matrix] captured ${manifest.combos.length} combinations of ${manifest.appVersion} (Gecko ${manifest.platformVersion}) into ${outDir}`,
  );
  if (problems.length > 0) {
    console.error(`[design-matrix] ${problems.length} capture problems:`);
    for (const p of problems) console.error(`  ${p.key}: ${p.message}`);
  }
  if (baselineDir) {
    // The report lists the capture problems too, and exits 2 if there are any.
    return await runCompare(
      baselineDir,
      outDir,
      path.resolve(args.report ?? path.join(outDir, "report")),
      options,
    );
  }
  return problems.length > 0 ? EXIT_ERROR : EXIT_PASS;
}

/**
 * Runs the CLI; every error, expected or not, exits 2 (never 1). Ctrl+C
 * exits 130, as the signal handler in capture.ts does.
 */
export async function main(argv: string[]): Promise<number> {
  try {
    return await run(argv);
  } catch (e) {
    const msg = e instanceof Error ? e.message : String(e);
    console.error(`[design-matrix] error: ${msg}`);
    if (e instanceof CaptureInterrupted) return EXIT_INTERRUPTED;
    if (e instanceof UsageError) console.error("\n" + USAGE);
    return EXIT_ERROR;
  }
}

if (import.meta.main) {
  Deno.exit(await main(Deno.args));
}
