// SPDX-License-Identifier: MPL-2.0

/** Types shared by the design matrix capture and compare steps. */

export interface RgbaImage {
  width: number;
  height: number;
  /** RGBA, 4 bytes per pixel, row-major. */
  data: Uint8Array;
}

/** A rectangle in screenshot pixels. */
export interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

export const DESIGNS = [
  "lepton",
  "photon",
  "protonfix",
  "proton",
  "fluerial",
] as const;
export type Design = (typeof DESIGNS)[number];

export const THEMES = [
  "none",
  "light",
  "dark",
  "lwt-header",
  "lwt-bottom",
] as const;
export type ThemeKey = (typeof THEMES)[number];

export const STATES = [
  "main",
  "newtab",
  "contextmenu",
  "fullscreen",
  "clear-history",
] as const;
export type StateKey = (typeof STATES)[number];

/** Computed values of one element, keyed by CSS property. */
export type SurfaceStyle = Record<string, string> | null;

/** Computed values of every recorded surface in one state. */
export type StateStyles = Record<string, SurfaceStyle>;

export interface ShotRecord {
  /** File name relative to the run directory. */
  file: string;
  /** Regions excluded from pixel comparison (screenshot pixels). */
  masks: Rect[];
}

export interface StateRecord {
  shot: ShotRecord | null;
  styles: StateStyles;
  /** Informational notes (e.g. pref values seen). */
  notes: string[];
  /**
   * Things that went wrong in this state (it was not reached, the context
   * menu did not open, the window kept changing). Any problem makes a
   * compare exit 2. (Manifests written before this field existed lack it;
   * the compare step treats it as empty.)
   */
  problems: string[];
}

export interface ComboRecord {
  design: Design;
  theme: ThemeKey;
  states: Partial<Record<StateKey, StateRecord>>;
  errors: string[];
}

export interface RunManifest {
  tool: "design-matrix";
  formatVersion: 1;
  /**
   * False while the run is in progress and after an interruption (Ctrl+C, a
   * killed process): the combinations it lacks were not captured, so the
   * compare step refuses it. True once every requested combination was
   * attempted. (Manifests written before this field existed lack it and are
   * treated as complete.)
   */
  complete?: boolean;
  startedAt: string;
  finishedAt: string;
  exe: string;
  appVersion: string;
  platformVersion: string;
  buildID: string;
  headless: boolean;
  windowSize: { width: number; height: number };
  combos: ComboRecord[];
}

export interface ImageDiff {
  key: string;
  file: string;
  status:
    | "same"
    | "changed"
    | "size-mismatch"
    | "missing"
    | "added"
    | "missing-both";
  changedPixels: number;
  comparedPixels: number;
  /** Pixels excluded by masks (0 when masks were ignored). */
  maskedPixels: number;
  /** Masks covered more than `maxMaskRatio` and were not applied. */
  masksIgnored: boolean;
  ratio: number;
  /** Whether this image counts as a regression. */
  failing: boolean;
  /** Bounding box of changed pixels, if any. */
  bbox: Rect | null;
  baselineSize?: { width: number; height: number };
  currentSize?: { width: number; height: number };
  diffImage?: string;
}

export interface ValueDiff {
  key: string;
  baseline: string | null;
  current: string | null;
}

export interface CompareOptions {
  /** Per-channel difference (0-255) a pixel may have and still count as equal. */
  tolerance: number;
  /** Changed-pixel ratio above which an image counts as a regression. */
  threshold: number;
  /**
   * Changed-pixel count above which an image counts as a regression,
   * whatever the ratio (catches icon and 1px border changes).
   */
  maxPixels: number;
  /**
   * Largest share of an image masks may cover. Above it the masks are not
   * applied (and the report says so), so a layout change that grows a masked
   * region cannot hide other changes.
   */
  maxMaskRatio: number;
  /** Fail when computed style values differ. */
  failOnStyleDiff: boolean;
  /** Write highlighted diff PNGs for changed images. */
  writeDiffImages: boolean;
}

export interface CompareReport {
  baselineDir: string;
  currentDir: string;
  baseline: Pick<RunManifest, "appVersion" | "platformVersion" | "buildID">;
  current: Pick<RunManifest, "appVersion" | "platformVersion" | "buildID">;
  options: CompareOptions;
  images: ImageDiff[];
  values: ValueDiff[];
  /** Capture errors and state problems from either run. */
  problems: CaptureProblem[];
  /** Baseline design/theme combinations absent from the current run. */
  skippedCombos: string[];
  maxRatio: number;
  failingImages: number;
  /** "error": capture problems (exit 2); "fail": regressions (exit 1). */
  status: "pass" | "fail" | "error";
  failed: boolean;
}

export interface CaptureProblem {
  run: "baseline" | "current" | "both";
  key: string;
  message: string;
}
