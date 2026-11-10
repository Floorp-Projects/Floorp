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
  notes: string[];
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
  status: "same" | "changed" | "size-mismatch" | "missing" | "added";
  changedPixels: number;
  comparedPixels: number;
  ratio: number;
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
  maxRatio: number;
  failingImages: number;
  failed: boolean;
}
