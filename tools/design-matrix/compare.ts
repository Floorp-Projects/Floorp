// SPDX-License-Identifier: MPL-2.0

/**
 * Compares two design matrix runs: per-image changed-pixel ratios and
 * computed-style value differences, rendered as JSON and Markdown.
 */

import * as path from "@std/path";
import { decodePng, encodePng } from "./png.ts";
import type {
  CompareOptions,
  CompareReport,
  ImageDiff,
  Rect,
  RgbaImage,
  RunManifest,
  StateRecord,
  ValueDiff,
} from "./types.ts";

export const DEFAULT_COMPARE_OPTIONS: CompareOptions = {
  tolerance: 8,
  threshold: 0.002,
  failOnStyleDiff: true,
  writeDiffImages: true,
};

export interface PixelDiffResult {
  changedPixels: number;
  comparedPixels: number;
  bbox: Rect | null;
  /** Per-pixel changed flags (1 = changed), or null when not requested. */
  mask: Uint8Array | null;
}

function insideAny(x: number, y: number, rects: Rect[]): boolean {
  for (const r of rects) {
    if (x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) {
      return true;
    }
  }
  return false;
}

/**
 * Counts pixels whose largest RGBA channel difference exceeds `tolerance`.
 * Pixels inside `masks` are not compared. Images must be the same size.
 */
export function diffPixels(
  a: RgbaImage,
  b: RgbaImage,
  tolerance: number,
  masks: Rect[] = [],
  keepMask = false,
): PixelDiffResult {
  if (a.width !== b.width || a.height !== b.height) {
    throw new Error("diffPixels needs images of the same size");
  }
  const { width, height } = a;
  const flags = keepMask ? new Uint8Array(width * height) : null;
  let changed = 0;
  let compared = 0;
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -1;
  let maxY = -1;
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      if (masks.length > 0 && insideAny(x, y, masks)) continue;
      compared++;
      const o = (y * width + x) * 4;
      const d = Math.max(
        Math.abs(a.data[o] - b.data[o]),
        Math.abs(a.data[o + 1] - b.data[o + 1]),
        Math.abs(a.data[o + 2] - b.data[o + 2]),
        Math.abs(a.data[o + 3] - b.data[o + 3]),
      );
      if (d > tolerance) {
        changed++;
        if (flags) flags[y * width + x] = 1;
        if (x < minX) minX = x;
        if (y < minY) minY = y;
        if (x > maxX) maxX = x;
        if (y > maxY) maxY = y;
      }
    }
  }
  const bbox = changed > 0
    ? { x: minX, y: minY, width: maxX - minX + 1, height: maxY - minY + 1 }
    : null;
  return {
    changedPixels: changed,
    comparedPixels: compared,
    bbox,
    mask: flags,
  };
}

/** The current image dimmed, with changed pixels painted magenta. */
export function renderDiffImage(
  current: RgbaImage,
  flags: Uint8Array,
  masks: Rect[],
): RgbaImage {
  const out = new Uint8Array(current.data.length);
  for (let i = 0; i < current.width * current.height; i++) {
    const o = i * 4;
    if (flags[i]) {
      out[o] = 255;
      out[o + 1] = 0;
      out[o + 2] = 255;
    } else {
      const y = Math.floor(i / current.width);
      const x = i % current.width;
      const masked = masks.length > 0 && insideAny(x, y, masks);
      const gray = (current.data[o] + current.data[o + 1] +
        current.data[o + 2]) / 3;
      const v = masked ? 40 : Math.round(gray * 0.35 + 150);
      out[o] = out[o + 1] = out[o + 2] = v;
    }
    out[o + 3] = 255;
  }
  return { width: current.width, height: current.height, data: out };
}

/** Flattens a run's computed styles into `combo/state/surface/property` keys. */
export function flattenStyles(manifest: RunManifest): Map<string, string> {
  const out = new Map<string, string>();
  for (const combo of manifest.combos) {
    const prefix = `${combo.design}/${combo.theme}`;
    for (const [state, record] of Object.entries(combo.states)) {
      const styles = (record as StateRecord).styles;
      for (const [surface, values] of Object.entries(styles)) {
        if (values === null) {
          out.set(`${prefix}/${state}/${surface}`, "(missing)");
          continue;
        }
        for (const [prop, value] of Object.entries(values)) {
          out.set(`${prefix}/${state}/${surface}/${prop}`, value);
        }
      }
    }
  }
  return out;
}

export function diffValues(
  baseline: Map<string, string>,
  current: Map<string, string>,
): ValueDiff[] {
  const keys = new Set([...baseline.keys(), ...current.keys()]);
  const diffs: ValueDiff[] = [];
  for (const key of [...keys].sort()) {
    const a = baseline.get(key) ?? null;
    const b = current.get(key) ?? null;
    if (a !== b) diffs.push({ key, baseline: a, current: b });
  }
  return diffs;
}

interface ShotEntry {
  file: string;
  masks: Rect[];
}

function collectShots(manifest: RunManifest): Map<string, ShotEntry> {
  const out = new Map<string, ShotEntry>();
  for (const combo of manifest.combos) {
    for (const [state, record] of Object.entries(combo.states)) {
      const shot = (record as StateRecord).shot;
      if (shot) {
        out.set(`${combo.design}/${combo.theme}/${state}`, {
          file: shot.file,
          masks: shot.masks,
        });
      }
    }
  }
  return out;
}

export async function readManifest(dir: string): Promise<RunManifest> {
  const text = await Deno.readTextFile(path.join(dir, "manifest.json"));
  const manifest = JSON.parse(text) as RunManifest;
  if (manifest.tool !== "design-matrix") {
    throw new Error(`${dir} is not a design-matrix run directory`);
  }
  return manifest;
}

export async function compareRuns(
  baselineDir: string,
  currentDir: string,
  reportDir: string,
  options: CompareOptions,
): Promise<CompareReport> {
  const baseline = await readManifest(baselineDir);
  const current = await readManifest(currentDir);
  const baseShots = collectShots(baseline);
  const curShots = collectShots(current);
  const images: ImageDiff[] = [];
  const diffDir = path.join(reportDir, "diff");

  const keys = [...new Set([...baseShots.keys(), ...curShots.keys()])].sort();
  for (const key of keys) {
    const a = baseShots.get(key);
    const b = curShots.get(key);
    if (!a || !b) {
      images.push({
        key,
        file: (a ?? b)!.file,
        status: a ? "missing" : "added",
        changedPixels: 0,
        comparedPixels: 0,
        ratio: 1,
        bbox: null,
      });
      continue;
    }
    const imgA = await decodePng(
      await Deno.readFile(path.join(baselineDir, a.file)),
    );
    const imgB = await decodePng(
      await Deno.readFile(path.join(currentDir, b.file)),
    );
    if (imgA.width !== imgB.width || imgA.height !== imgB.height) {
      images.push({
        key,
        file: b.file,
        status: "size-mismatch",
        changedPixels: imgB.width * imgB.height,
        comparedPixels: imgB.width * imgB.height,
        ratio: 1,
        bbox: null,
        baselineSize: { width: imgA.width, height: imgA.height },
        currentSize: { width: imgB.width, height: imgB.height },
      });
      continue;
    }
    const masks = [...a.masks, ...b.masks];
    const result = diffPixels(
      imgA,
      imgB,
      options.tolerance,
      masks,
      options.writeDiffImages,
    );
    const ratio = result.comparedPixels > 0
      ? result.changedPixels / result.comparedPixels
      : 0;
    const entry: ImageDiff = {
      key,
      file: b.file,
      status: result.changedPixels > 0 ? "changed" : "same",
      changedPixels: result.changedPixels,
      comparedPixels: result.comparedPixels,
      ratio,
      bbox: result.bbox,
    };
    if (
      options.writeDiffImages && result.mask && ratio > options.threshold
    ) {
      await Deno.mkdir(diffDir, { recursive: true });
      const name = key.replaceAll("/", "__") + ".diff.png";
      await Deno.writeFile(
        path.join(diffDir, name),
        await encodePng(renderDiffImage(imgB, result.mask, masks)),
      );
      entry.diffImage = path.join("diff", name);
    }
    images.push(entry);
  }

  const values = diffValues(flattenStyles(baseline), flattenStyles(current));
  const failingImages = images.filter((i) => i.ratio > options.threshold)
    .length;
  const maxRatio = images.reduce((m, i) => Math.max(m, i.ratio), 0);
  const pick = (m: RunManifest) => ({
    appVersion: m.appVersion,
    platformVersion: m.platformVersion,
    buildID: m.buildID,
  });
  return {
    baselineDir,
    currentDir,
    baseline: pick(baseline),
    current: pick(current),
    options,
    images,
    values,
    maxRatio,
    failingImages,
    failed: failingImages > 0 ||
      (options.failOnStyleDiff && values.length > 0),
  };
}

function pct(r: number): string {
  return `${(r * 100).toFixed(3)}%`;
}

function cell(v: string | null): string {
  return v === null ? "_(none)_" : "`" + v.replaceAll("|", "\\|") + "`";
}

export function renderMarkdown(report: CompareReport): string {
  const o = report.options;
  const lines: string[] = [];
  lines.push("# Design matrix diff report", "");
  lines.push(
    `- Result: **${report.failed ? "FAIL" : "PASS"}**`,
    `- Baseline: ${report.baseline.appVersion} (Gecko ${report.baseline.platformVersion}, build ${report.baseline.buildID}) — \`${report.baselineDir}\``,
    `- Current: ${report.current.appVersion} (Gecko ${report.current.platformVersion}, build ${report.current.buildID}) — \`${report.currentDir}\``,
    `- Pixel tolerance: ${o.tolerance}/255 per channel; image threshold: ${
      pct(o.threshold)
    } changed pixels; style differences ${
      o.failOnStyleDiff ? "fail" : "are informational"
    }`,
    `- Images over threshold: ${report.failingImages} of ${report.images.length}; largest ratio ${
      pct(report.maxRatio)
    }`,
    `- Computed-style values that differ: ${report.values.length}`,
    "",
  );

  const over = report.images.filter((i) => i.ratio > o.threshold)
    .sort((a, b) => b.ratio - a.ratio);
  lines.push("## Images over threshold", "");
  if (over.length === 0) {
    lines.push("None.", "");
  } else {
    lines.push(
      "| design/theme/state | status | changed | bbox (x,y w×h) | diff |",
      "| --- | --- | ---: | --- | --- |",
    );
    for (const i of over) {
      const bbox = i.bbox
        ? `${i.bbox.x},${i.bbox.y} ${i.bbox.width}×${i.bbox.height}`
        : i.status === "size-mismatch"
        ? `${i.baselineSize?.width}×${i.baselineSize?.height} → ${i.currentSize?.width}×${i.currentSize?.height}`
        : "";
      const diff = i.diffImage
        ? `[png](${i.diffImage.replaceAll("\\", "/")})`
        : "";
      lines.push(
        `| ${i.key} | ${i.status} | ${pct(i.ratio)} | ${bbox} | ${diff} |`,
      );
    }
    lines.push("");
  }

  lines.push("## Computed-style differences", "");
  if (report.values.length === 0) {
    lines.push("None.", "");
  } else {
    lines.push(
      "| design/theme/state/surface/property | baseline | current |",
      "| --- | --- | --- |",
    );
    for (const v of report.values) {
      lines.push(`| ${v.key} | ${cell(v.baseline)} | ${cell(v.current)} |`);
    }
    lines.push("");
  }

  lines.push("## All images", "");
  lines.push("| design/theme/state | status | changed pixels | ratio |");
  lines.push("| --- | --- | ---: | ---: |");
  for (const i of report.images) {
    lines.push(
      `| ${i.key} | ${i.status} | ${i.changedPixels} | ${pct(i.ratio)} |`,
    );
  }
  lines.push("");
  return lines.join("\n");
}
