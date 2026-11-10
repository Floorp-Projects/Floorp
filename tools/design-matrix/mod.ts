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
import { captureMatrix } from "./capture.ts";
import {
  compareRuns,
  DEFAULT_COMPARE_OPTIONS,
  renderMarkdown,
} from "./compare.ts";
import {
  type CompareOptions,
  type Design,
  DESIGNS,
  type ThemeKey,
  THEMES,
} from "./types.ts";

const USAGE = `Usage:
  deno task design-matrix run --exe <path/to/floorp(.exe)> --out <dir> [--baseline <dir>] [options]
  deno task design-matrix compare --baseline <dir> --current <dir> [--report <dir>] [options]

Capture options:
  --designs a,b      subset of ${DESIGNS.join(",")} (default: all)
  --themes a,b       subset of ${THEMES.join(",")} (default: all)
  --port N           Marionette port (default 2828)
  --headed           show the window instead of -headless
  --width/--height   window size in CSS px (default 1280x800)
  --tmp-dir <dir>    parent for temporary profiles (default: OS temp)

Compare options:
  --tolerance N      per-channel difference ignored, 0-255 (default ${DEFAULT_COMPARE_OPTIONS.tolerance})
  --threshold R      changed-pixel ratio that fails an image (default ${DEFAULT_COMPARE_OPTIONS.threshold})
  --ignore-styles    report computed-style differences without failing
  --no-diff-images   do not write highlighted diff PNGs

Exit codes: 0 no regression, 1 differences above threshold, 2 usage or capture error.`;

function parseList<T extends string>(
  raw: string | undefined,
  allowed: readonly T[],
  label: string,
): T[] {
  if (!raw) return [...allowed];
  const items = raw.split(",").map((s) => s.trim()).filter(Boolean);
  for (const item of items) {
    if (!allowed.includes(item as T)) {
      throw new Error(
        `Unknown ${label} "${item}" (allowed: ${allowed.join(", ")})`,
      );
    }
  }
  return items as T[];
}

function num(raw: string | undefined, fallback: number, label: string): number {
  if (raw === undefined) return fallback;
  const n = Number(raw);
  if (!Number.isFinite(n) || n < 0) {
    throw new Error(`Invalid --${label}: ${raw}`);
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
    `[design-matrix] ${
      report.failed ? "FAIL" : "PASS"
    }: ${report.failingImages}/${report.images.length} images over ${
      (options.threshold * 100).toFixed(3)
    }% (max ${
      (report.maxRatio * 100).toFixed(3)
    }%), ${report.values.length} style values differ`,
  );
  console.log(`[design-matrix] report: ${path.join(reportDir, "report.md")}`);
  return report.failed ? 1 : 0;
}

export async function main(argv: string[]): Promise<number> {
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
    ],
    boolean: ["headed", "ignore-styles", "no-diff-images", "help"],
  });
  const command = String(args._[0] ?? "");
  if (args.help || !["run", "compare"].includes(command)) {
    console.log(USAGE);
    return args.help ? 0 : 2;
  }

  const options: CompareOptions = {
    tolerance: num(
      args.tolerance,
      DEFAULT_COMPARE_OPTIONS.tolerance,
      "tolerance",
    ),
    threshold: num(
      args.threshold,
      DEFAULT_COMPARE_OPTIONS.threshold,
      "threshold",
    ),
    failOnStyleDiff: !args["ignore-styles"],
    writeDiffImages: !args["no-diff-images"],
  };

  if (command === "compare") {
    if (!args.baseline || !args.current) {
      console.error("compare needs --baseline and --current\n\n" + USAGE);
      return 2;
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
    console.error("run needs --exe and --out\n\n" + USAGE);
    return 2;
  }
  const exe = path.resolve(args.exe);
  try {
    await Deno.stat(exe);
  } catch {
    console.error(`Build not found: ${exe}`);
    return 2;
  }
  const outDir = path.resolve(args.out);
  const manifest = await captureMatrix({
    exe,
    outDir,
    designs: parseList<Design>(args.designs, DESIGNS, "design"),
    themes: parseList<ThemeKey>(args.themes, THEMES, "theme"),
    port: num(args.port, 2828, "port"),
    headless: !args.headed,
    width: num(args.width, 1280, "width"),
    height: num(args.height, 800, "height"),
    tmpRoot: args["tmp-dir"] ? path.resolve(args["tmp-dir"]) : undefined,
    log: (msg) => console.error(msg),
  });
  const errors = manifest.combos.flatMap((c) =>
    c.errors.map((e) => `${c.design}/${c.theme}: ${e}`)
  );
  console.log(
    `[design-matrix] captured ${manifest.combos.length} combinations of ${manifest.appVersion} (Gecko ${manifest.platformVersion}) into ${outDir}`,
  );
  if (errors.length > 0) {
    console.error(`[design-matrix] ${errors.length} capture errors:`);
    for (const e of errors) console.error(`  ${e}`);
  }
  if (args.baseline) {
    const code = await runCompare(
      path.resolve(args.baseline),
      outDir,
      path.resolve(args.report ?? path.join(outDir, "report")),
      options,
    );
    return errors.length > 0 ? 2 : code;
  }
  return errors.length > 0 ? 2 : 0;
}

if (import.meta.main) {
  Deno.exit(await main(Deno.args));
}
