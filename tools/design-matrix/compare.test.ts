// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert";
import * as path from "@std/path";
import { writeManifest } from "./capture.ts";
import {
  compareRuns,
  DEFAULT_COMPARE_OPTIONS,
  diffPixels,
  diffValues,
  readManifest,
  renderMarkdown,
  SHOT_STATES,
} from "./compare.ts";
import { EXIT_ERROR, intArg, main, ratioArg } from "./mod.ts";
import { decodePng, encodePng } from "./png.ts";
import {
  type ComboRecord,
  type Rect,
  type RgbaImage,
  type RunManifest,
  type StateKey,
  type StateRecord,
  STATES,
} from "./types.ts";

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

interface FakeRun {
  img: RgbaImage;
  navBarColor: string;
  masks?: Rect[];
  /** States to leave out of the manifest. */
  skip?: StateKey[];
  problems?: string[];
  /** Written as the manifest's `complete` field; omitted when undefined. */
  complete?: boolean;
}

/** Writes a one-combination run (lepton/none) with every state recorded. */
async function writeRun(dir: string, run: FakeRun): Promise<void> {
  await Deno.mkdir(dir, { recursive: true });
  const png = await encodePng(run.img);
  const states: ComboRecord["states"] = {};
  for (const state of STATES) {
    if (run.skip?.includes(state)) continue;
    const rec: StateRecord = {
      shot: null,
      styles: { "nav-bar": { "background-color": run.navBarColor } },
      notes: [],
      problems: state === "main" ? (run.problems ?? []) : [],
    };
    if (SHOT_STATES.includes(state)) {
      const file = `lepton__none__${state}.png`;
      await Deno.writeFile(path.join(dir, file), png);
      rec.shot = { file, masks: run.masks ?? [] };
    }
    states[state] = rec;
  }
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
    windowSize: { width: run.img.width, height: run.img.height },
    combos: [{ design: "lepton", theme: "none", errors: [], states }],
  };
  if (run.complete !== undefined) manifest.complete = run.complete;
  await Deno.writeTextFile(
    path.join(dir, "manifest.json"),
    JSON.stringify(manifest),
  );
}

async function withTmp(fn: (tmp: string) => Promise<void>): Promise<void> {
  const tmp = await Deno.makeTempDir({ prefix: "design-matrix-test-" });
  try {
    await fn(tmp);
  } finally {
    await Deno.remove(tmp, { recursive: true });
  }
}

function compare(tmp: string, a: string, b: string) {
  return compareRuns(
    path.join(tmp, a),
    path.join(tmp, b),
    path.join(tmp, `report-${a}-${b}`),
    DEFAULT_COMPARE_OPTIONS,
  );
}

const WHITE: [number, number, number, number] = [255, 255, 255, 255];

Deno.test("compareRuns: identical runs pass, a changed toolbar fails", async () => {
  await withTmp(async (tmp) => {
    const base = solid(20, 20, WHITE);
    await writeRun(path.join(tmp, "a"), {
      img: base,
      navBarColor: "rgb(1, 2, 3)",
    });
    await writeRun(path.join(tmp, "b"), {
      img: base,
      navBarColor: "rgb(1, 2, 3)",
    });
    const same = await compare(tmp, "a", "b");
    assertEquals(same.status, "pass");
    assertEquals(same.maxRatio, 0);
    assertEquals(same.problems, []);

    const changed = solid(20, 20, WHITE);
    for (let x = 0; x < 20; x++) paint(changed, x, 0, [0, 0, 0, 255]);
    await writeRun(path.join(tmp, "c"), {
      img: changed,
      navBarColor: "rgb(9, 9, 9)",
    });
    const diff = await compare(tmp, "a", "c");
    assertEquals(diff.status, "fail");
    assertEquals(diff.failingImages, SHOT_STATES.length);
    assertEquals(diff.images[0].changedPixels, 20);
    assertEquals(diff.values.length, STATES.length);
    assert(diff.images[0].diffImage);
    const md = renderMarkdown(diff);
    assert(md.includes("**FAIL**"));
    assert(md.includes("lepton/none/main"));
    assert(md.includes("rgb(9, 9, 9)"));
  });
});

Deno.test("compareRuns: a few changed pixels fail even below the ratio threshold", async () => {
  await withTmp(async (tmp) => {
    const base = solid(100, 100, WHITE);
    const at = (n: number) => {
      const img = solid(100, 100, WHITE);
      for (let i = 0; i < n; i++) paint(img, i, 50, [0, 0, 0, 255]);
      return img;
    };
    await writeRun(path.join(tmp, "a"), { img: base, navBarColor: "x" });
    await writeRun(path.join(tmp, "ok"), { img: at(16), navBarColor: "x" });
    await writeRun(path.join(tmp, "bad"), { img: at(17), navBarColor: "x" });
    const ok = await compare(tmp, "a", "ok");
    assertEquals(ok.status, "pass");
    const bad = await compare(tmp, "a", "bad");
    // 17 / 10000 = 0.17% is under the 0.2% ratio, but over 16 pixels.
    assert(bad.images[0].ratio < DEFAULT_COMPARE_OPTIONS.threshold);
    assertEquals(bad.status, "fail");
  });
});

Deno.test("compareRuns: small masks hide noise and are counted", async () => {
  await withTmp(async (tmp) => {
    const base = solid(100, 100, WHITE);
    const noisy = solid(100, 100, WHITE);
    for (let i = 0; i < 40; i++) {
      paint(noisy, i % 10, Math.floor(i / 10), [0, 0, 0, 255]);
    }
    const mask = [{ x: 0, y: 0, width: 10, height: 10 }]; // 1% of the image
    await writeRun(path.join(tmp, "a"), {
      img: base,
      navBarColor: "x",
      masks: mask,
    });
    await writeRun(path.join(tmp, "b"), {
      img: noisy,
      navBarColor: "x",
      masks: mask,
    });
    const r = await compare(tmp, "a", "b");
    assertEquals(r.status, "pass");
    assertEquals(r.images[0].maskedPixels, 100);
    assertEquals(r.images[0].masksIgnored, false);
    assert(renderMarkdown(r).includes("Masked: 4 images, 400 pixels"));
  });
});

Deno.test("compareRuns: masks over the cap are ignored", async () => {
  await withTmp(async (tmp) => {
    const base = solid(100, 100, WHITE);
    const changed = solid(100, 100, WHITE);
    for (let x = 0; x < 50; x++) paint(changed, x, 5, [0, 0, 0, 255]);
    // A mask that grew to 10% of the image would hide the change.
    const mask = [{ x: 0, y: 0, width: 100, height: 10 }];
    await writeRun(path.join(tmp, "a"), { img: base, navBarColor: "x" });
    await writeRun(path.join(tmp, "b"), {
      img: changed,
      navBarColor: "x",
      masks: mask,
    });
    const r = await compare(tmp, "a", "b");
    assertEquals(r.images[0].masksIgnored, true);
    assertEquals(r.images[0].maskedPixels, 0);
    assertEquals(r.images[0].changedPixels, 50);
    assertEquals(r.status, "fail");
  });
});

Deno.test("compareRuns: capture problems and states missing in both runs are errors", async () => {
  await withTmp(async (tmp) => {
    const img = solid(10, 10, WHITE);
    await writeRun(path.join(tmp, "a"), {
      img,
      navBarColor: "x",
      skip: ["newtab"],
    });
    await writeRun(path.join(tmp, "b"), {
      img,
      navBarColor: "x",
      skip: ["newtab"],
    });
    const missing = await compare(tmp, "a", "b");
    assertEquals(missing.status, "error");
    const both = missing.images.find((i) => i.key === "lepton/none/newtab");
    assertEquals(both?.status, "missing-both");
    assert(missing.problems.some((p) => p.message === "state not recorded"));

    await writeRun(path.join(tmp, "c"), {
      img,
      navBarColor: "x",
      problems: ["context menu did not open"],
    });
    await writeRun(path.join(tmp, "d"), { img, navBarColor: "x" });
    const prob = await compare(tmp, "d", "c");
    assertEquals(prob.status, "error");
    assertEquals(prob.problems, [{
      run: "current",
      key: "lepton/none/main",
      message: "context menu did not open",
    }]);
    assert(renderMarkdown(prob).includes("context menu did not open"));
  });
});

Deno.test("main: errors exit 2, never 1", async () => {
  await withTmp(async (tmp) => {
    const quiet = silence();
    try {
      assertEquals(
        await main([
          "compare",
          "--baseline",
          path.join(tmp, "nope"),
          "--current",
          tmp,
        ]),
        EXIT_ERROR,
      );
      assertEquals(
        await main([
          "run",
          "--exe",
          Deno.execPath(),
          "--out",
          tmp,
          "--designs",
          "foo",
        ]),
        EXIT_ERROR,
      );
      assertEquals(
        await main([
          "run",
          "--exe",
          Deno.execPath(),
          "--out",
          tmp,
          "--baseline",
          path.join(tmp, "nope"),
        ]),
        EXIT_ERROR,
      );
      assertEquals(await main(["frobnicate"]), EXIT_ERROR);
    } finally {
      quiet();
    }
  });
});

Deno.test("compareRuns: an incomplete run is refused, an old manifest without the flag is not", async () => {
  await withTmp(async (tmp) => {
    const img = solid(10, 10, WHITE);
    await writeRun(path.join(tmp, "old"), { img, navBarColor: "x" });
    await writeRun(path.join(tmp, "done"), {
      img,
      navBarColor: "x",
      complete: true,
    });
    await writeRun(path.join(tmp, "partial"), {
      img,
      navBarColor: "x",
      complete: false,
    });
    assertEquals((await compare(tmp, "old", "done")).status, "pass");
    await assertRejects(
      () => compare(tmp, "partial", "done"),
      Error,
      "incomplete run",
    );
    await assertRejects(
      () => compare(tmp, "done", "partial"),
      Error,
      "incomplete run",
    );
    const quiet = silence();
    try {
      assertEquals(
        await main([
          "compare",
          "--baseline",
          path.join(tmp, "done"),
          "--current",
          path.join(tmp, "partial"),
        ]),
        EXIT_ERROR,
      );
      // `run --baseline` checks the baseline before starting any browser.
      assertEquals(
        await main([
          "run",
          "--exe",
          Deno.execPath(),
          "--out",
          path.join(tmp, "out"),
          "--baseline",
          path.join(tmp, "partial"),
        ]),
        EXIT_ERROR,
      );
    } finally {
      quiet();
    }
  });
});

Deno.test("writeManifest: replaces the manifest and leaves no temporary file", async () => {
  await withTmp(async (tmp) => {
    await writeRun(tmp, { img: solid(4, 4, WHITE), navBarColor: "x" });
    const manifest = await readManifest(tmp);
    await writeManifest(tmp, { ...manifest, complete: false });
    assertEquals(
      [...Deno.readDirSync(tmp)].filter((e) => e.name.endsWith(".tmp")),
      [],
    );
    await assertRejects(() => readManifest(tmp), Error, "incomplete run");
    await writeManifest(tmp, { ...manifest, complete: true });
    assertEquals((await readManifest(tmp)).complete, true);
  });
});

Deno.test("readManifest: a truncated manifest is a clear error", async () => {
  await withTmp(async (tmp) => {
    await Deno.writeTextFile(path.join(tmp, "manifest.json"), '{"tool": "de');
    await assertRejects(() => readManifest(tmp), Error, "not valid JSON");
  });
});

Deno.test("intArg / ratioArg: reject non-integers, negatives, NaN and out-of-range values", () => {
  assertEquals(intArg(undefined, 2828, "port", 1, 65534), 2828);
  assertEquals(intArg("2860", 2828, "port", 1, 65534), 2860);
  for (
    const bad of ["", " ", "abc", "1.5", "-1", "NaN", "Infinity", "1e3", "0x10"]
  ) {
    assertThrows(
      () => intArg(bad, 2828, "port", 1, 65534),
      Error,
      "whole number",
    );
  }
  assertThrows(() => intArg("65535", 2828, "port", 1, 65534), Error, "65534");
  assertThrows(() => intArg("0", 2828, "port", 1, 65534), Error, "from 1");
  assertEquals(ratioArg("0.002", 0.1, "threshold"), 0.002);
  assertEquals(ratioArg("1", 0.1, "threshold"), 1);
  for (const bad of ["", " ", "abc", "-0.1", "1.5", "NaN", "Infinity"]) {
    assertThrows(() => ratioArg(bad, 0.1, "threshold"), Error, "ratio");
  }
});

Deno.test("main: bad numeric flags exit 2 before anything starts", async () => {
  await withTmp(async (tmp) => {
    const out = path.join(tmp, "out");
    const quiet = silence();
    try {
      for (
        const flag of [
          "--port=abc",
          "--port=28.5",
          "--port=-1",
          "--port=NaN",
          "--port=65535",
          "--width=0",
          "--height=1e3",
          "--tolerance=256",
          "--max-pixels=1.5",
          "--threshold=",
          "--max-mask=2",
        ]
      ) {
        assertEquals(
          await main(["run", "--exe", Deno.execPath(), "--out", out, flag]),
          EXIT_ERROR,
          flag,
        );
      }
    } finally {
      quiet();
    }
    // Nothing was captured: the output directory was never created.
    await assertRejects(() => Deno.stat(out), Deno.errors.NotFound);
  });
});

const netGranted =
  (await Deno.permissions.query({ name: "net", host: "127.0.0.1" })).state ===
    "granted";

Deno.test({
  name: "main: a busy proxy port exits 2 and leaves no temp root",
  ignore: !netGranted,
  fn: async () => {
    await withTmp(async (tmp) => {
      const blocker = Deno.listen({ hostname: "127.0.0.1", port: 0 });
      const busy = (blocker.addr as Deno.NetAddr).port;
      const tmpRoot = path.join(tmp, "profiles");
      await Deno.mkdir(tmpRoot);
      const quiet = silence();
      try {
        const code = await main([
          "run",
          "--exe",
          Deno.execPath(), // never started: the proxy fails first
          "--out",
          path.join(tmp, "out"),
          "--port",
          String(busy - 1),
          "--tmp-dir",
          tmpRoot,
        ]);
        assertEquals(code, EXIT_ERROR);
        assertEquals([...Deno.readDirSync(tmpRoot)], []);
      } finally {
        quiet();
        blocker.close();
      }
    });
  },
});

/** Silences console output from main(); returns a restore function. */
function silence(): () => void {
  const { log, error } = console;
  console.log = () => {};
  console.error = () => {};
  return () => {
    console.log = log;
    console.error = error;
  };
}
