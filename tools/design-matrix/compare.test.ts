// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals } from "@std/assert";
import * as path from "@std/path";
import {
  compareRuns,
  DEFAULT_COMPARE_OPTIONS,
  diffPixels,
  diffValues,
  renderMarkdown,
} from "./compare.ts";
import { decodePng, encodePng } from "./png.ts";
import type { RgbaImage, RunManifest } from "./types.ts";

function solid(
  width: number,
  height: number,
  rgba: [number, number, number, number],
): RgbaImage {
  const data = new Uint8Array(width * height * 4);
  for (let i = 0; i < width * height; i++) data.set(rgba, i * 4);
  return { width, height, data };
}

function paint(img: RgbaImage, x: number, y: number, rgba: number[]): void {
  img.data.set(rgba, (y * img.width + x) * 4);
}

Deno.test("png: encode/decode round trip keeps pixels", async () => {
  const img = solid(7, 5, [10, 20, 30, 255]);
  paint(img, 3, 2, [200, 100, 50, 128]);
  const decoded = await decodePng(await encodePng(img));
  assertEquals(decoded.width, 7);
  assertEquals(decoded.height, 5);
  assertEquals(decoded.data, img.data);
});

Deno.test("diffPixels: ignores differences within tolerance", () => {
  const a = solid(4, 4, [100, 100, 100, 255]);
  const b = solid(4, 4, [104, 100, 96, 255]);
  const r = diffPixels(a, b, 8);
  assertEquals(r.changedPixels, 0);
  assertEquals(r.comparedPixels, 16);
  assertEquals(r.bbox, null);
});

Deno.test("diffPixels: counts changed pixels and their bounding box", () => {
  const a = solid(10, 10, [0, 0, 0, 255]);
  const b = solid(10, 10, [0, 0, 0, 255]);
  paint(b, 2, 3, [255, 0, 0, 255]);
  paint(b, 5, 7, [0, 255, 0, 255]);
  const r = diffPixels(a, b, 8);
  assertEquals(r.changedPixels, 2);
  assertEquals(r.bbox, { x: 2, y: 3, width: 4, height: 5 });
});

Deno.test("diffPixels: masked regions are not compared", () => {
  const a = solid(10, 10, [0, 0, 0, 255]);
  const b = solid(10, 10, [0, 0, 0, 255]);
  paint(b, 1, 1, [255, 255, 255, 255]);
  const r = diffPixels(a, b, 8, [{ x: 0, y: 0, width: 3, height: 3 }]);
  assertEquals(r.changedPixels, 0);
  assertEquals(r.comparedPixels, 100 - 9);
});

Deno.test("diffValues: lists changed, added and removed keys", () => {
  const a = new Map([["x", "1"], ["y", "2"], ["gone", "3"]]);
  const b = new Map([["x", "1"], ["y", "20"], ["new", "4"]]);
  assertEquals(diffValues(a, b), [
    { key: "gone", baseline: "3", current: null },
    { key: "new", baseline: null, current: "4" },
    { key: "y", baseline: "2", current: "20" },
  ]);
});

async function writeRun(
  dir: string,
  img: RgbaImage,
  navBarColor: string,
): Promise<void> {
  await Deno.mkdir(dir, { recursive: true });
  await Deno.writeFile(
    path.join(dir, "lepton__none__main.png"),
    await encodePng(img),
  );
  const manifest: RunManifest = {
    tool: "design-matrix",
    formatVersion: 1,
    startedAt: "",
    finishedAt: "",
    exe: "floorp",
    appVersion: "test",
    platformVersion: "159.0",
    buildID: "0",
    headless: true,
    windowSize: { width: img.width, height: img.height },
    combos: [{
      design: "lepton",
      theme: "none",
      errors: [],
      states: {
        main: {
          shot: { file: "lepton__none__main.png", masks: [] },
          styles: { "nav-bar": { "background-color": navBarColor } },
          notes: [],
        },
      },
    }],
  };
  await Deno.writeTextFile(
    path.join(dir, "manifest.json"),
    JSON.stringify(manifest),
  );
}

Deno.test("compareRuns: identical runs pass, a changed toolbar fails", async () => {
  const tmp = await Deno.makeTempDir({ prefix: "design-matrix-test-" });
  try {
    const base = solid(20, 20, [255, 255, 255, 255]);
    await writeRun(path.join(tmp, "a"), base, "rgb(1, 2, 3)");
    await writeRun(path.join(tmp, "b"), base, "rgb(1, 2, 3)");
    const same = await compareRuns(
      path.join(tmp, "a"),
      path.join(tmp, "b"),
      path.join(tmp, "report-same"),
      DEFAULT_COMPARE_OPTIONS,
    );
    assertEquals(same.failed, false);
    assertEquals(same.maxRatio, 0);

    const changed = solid(20, 20, [255, 255, 255, 255]);
    for (let x = 0; x < 20; x++) paint(changed, x, 0, [0, 0, 0, 255]);
    await writeRun(path.join(tmp, "c"), changed, "rgb(9, 9, 9)");
    const diff = await compareRuns(
      path.join(tmp, "a"),
      path.join(tmp, "c"),
      path.join(tmp, "report-diff"),
      DEFAULT_COMPARE_OPTIONS,
    );
    assert(diff.failed);
    assertEquals(diff.failingImages, 1);
    assertEquals(diff.images[0].changedPixels, 20);
    assertEquals(diff.values.length, 1);
    assert(diff.images[0].diffImage);
    const md = renderMarkdown(diff);
    assert(md.includes("**FAIL**"));
    assert(md.includes("lepton/none/main"));
    assert(md.includes("rgb(9, 9, 9)"));
  } finally {
    await Deno.remove(tmp, { recursive: true });
  }
});
