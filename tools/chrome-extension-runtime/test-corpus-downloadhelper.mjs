#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
// Drive the actual frozen VDH v10 UI. Its vendor removed the native companion.
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { snapshotUI } from "./corpus-sidepanel-ui.sys.mjs";
import { unzipSync } from "fflate";
import { createHash } from "node:crypto";
import { execFileSync } from "node:child_process";
import { createServer } from "node:http";
import {
  mkdirSync,
  mkdtempSync,
  readdirSync,
  readFileSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const id = "lmjnegcaeklhafolokijcfjliaokphfk";
const tools = join(root, "tools/chrome-extension-runtime");
const corpus =
  /** @type {import("./corpus-types.ts").FrozenCorpus} */ (JSON.parse(
    readFileSync(join(tools, "corpus-lock.json"), "utf8"),
  ));
const record = corpus.extensions.find((e) => e.id === id);
if (!record) throw Error("Frozen Video DownloadHelper record missing");
const reference = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json"), "utf8"),
);
/** @param {Uint8Array|string} bytes */
const sha = (bytes) => createHash("sha256").update(bytes).digest("hex");
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome",
);
if (sha(readFileSync(chrome)) !== reference.binary.sha256) {
  throw Error("Reference binary hash mismatch");
}
const archive = readFileSync(
  join(root, "_dist/chrome-extension-corpus/packages", id + ".crx"),
);
if (sha(archive) !== record.packageSha256) {
  throw Error("Frozen CRX hash mismatch");
}
if (
  archive.toString("ascii", 0, 4) !== "Cr24" || archive.readUInt32LE(4) !== 3
) throw Error("Expected the frozen authenticated CRX3 package");
const originalFiles = unzipSync(archive.subarray(12 + archive.readUInt32LE(8)));
const originalScriptHashes = [];
for (const [name, original] of Object.entries(originalFiles)) {
  if (!/\.(?:js|mjs|cjs)$/i.test(name)) continue;
  const unpacked = readFileSync(
    join(root, "_dist/chrome-extension-corpus/chrome-unpacked", id, name),
  );
  if (!Buffer.from(original).equals(unpacked)) {
    throw Error(
      "Frozen vendor script changed before Chrome installation: " + name,
    );
  }
  originalScriptHashes.push(name + ":" + sha(original));
}
const output = mkdtempSync(
  join(root, "_dist/chrome-runtime-tests/corpus-downloadhelper-reference-"),
);
const downloads = join(output, "downloads");
mkdirSync(downloads);
const mediaPath = join(output, "authored-video.mp4");
execFileSync("ffmpeg", [
  "-nostdin",
  "-loglevel",
  "error",
  "-f",
  "lavfi",
  "-i",
  "testsrc2=size=640x360:rate=24",
  "-f",
  "lavfi",
  "-i",
  "sine=frequency=440:sample_rate=48000",
  "-t",
  "4",
  "-c:v",
  "libx264",
  "-preset",
  "ultrafast",
  "-crf",
  "18",
  "-threads",
  "1",
  "-pix_fmt",
  "yuv420p",
  "-c:a",
  "aac",
  "-movflags",
  "+faststart",
  mediaPath,
]);
const media = readFileSync(mediaPath);
const pageBytes = readFileSync(join(tools, "fixtures/corpus-media.html"));
/** @type {import("./crx-test-types.ts").MediaReport & {fixture:{requests:{url?:string,range:string|null}[]}}} */
const report = {
  stage: "actual-package-primary-operation",
  browser: "Chrome",
  reference,
  output,
  extension: {
    id,
    name: record.name,
    version: record.version,
    packageSha256: record.packageSha256,
    primaryOperationAsSelected: record.primaryOperation,
    actualAdvertisedOperation:
      "Discover media and save a playable file through the vendor v10 download engine",
    nativeCompanionApplicability: "not-applicable-v10",
    nativeCompanionSource:
      "https://github.com/aclap-dev/vdhcoapp/blob/f3736cee7013d4f9003884d9f8077c4c16d2ee4f/README.md",
    primaryOperationResult: "not-run",
    snapshots: [],
  },
  fixture: {
    pageSha256: sha(pageBytes),
    mediaSha256: sha(media),
    mediaBytes: media.length,
    requests: [],
  },
};
Object.assign(report.extension, {
  unchangedScriptCount: originalScriptHashes.length,
  unchangedScriptsSha256: sha(originalScriptHashes.sort().join("\n")),
});
const save = () =>
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
const server = createServer((req, res) => {
  report.fixture.requests.push({
    url: req.url,
    range: req.headers.range || null,
  });
  if (req.url === "/authored-video.mp4") {
    const match = /^bytes=(\d+)-(\d*)$/.exec(req.headers.range || "");
    const start = match ? Number(match[1]) : 0;
    const end = match?.[2]
      ? Math.min(Number(match[2]), media.length - 1)
      : media.length - 1;
    if (start >= media.length || end < start) {
      res.writeHead(416, { "Content-Range": `bytes */${media.length}` });
      res.end();
      return;
    }
    res.writeHead(match ? 206 : 200, {
      "Content-Type": "video/mp4",
      "Content-Length": end - start + 1,
      "Accept-Ranges": "bytes",
      "Cache-Control": "no-store",
      ...(match
        ? { "Content-Range": `bytes ${start}-${end}/${media.length}` }
        : {}),
    });
    res.end(media.subarray(start, end + 1));
  } else {
    res.writeHead(200, {
      "Content-Type": "text/html; charset=utf-8",
      "Cache-Control": "no-store",
    });
    res.end(pageBytes);
  }
});
await new Promise((resolve) =>
  server.listen(0, "127.0.0.1", () => resolve(undefined))
);
const address = server.address();
if (!address || typeof address === "string") {
  throw Error("Media fixture did not bind a TCP port");
}
report.fixture.url = `http://127.0.0.1:${address.port}/`;
const browser = new ChromePipe(chrome, output);
browser.events = [];
/** @param {number} ms */
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template T @param {()=>Promise<T|false|null|undefined>|T|false|null|undefined} read @param {string} label @param {number} [timeout] @returns {Promise<T>} */
async function until(read, label, timeout = 20000) {
  const end = Date.now() + timeout;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await delay(150);
  }
  throw Error("Timed out waiting for " + label);
}
/** @template [T=unknown] @param {string} sessionId @param {string} expression @returns {Promise<T>} */
async function evaluate(sessionId, expression) {
  const r = await browser.call("Runtime.evaluate", {
    expression,
    returnByValue: true,
    awaitPromise: true,
  }, sessionId);
  if (r.exceptionDetails) throw Error(JSON.stringify(r.exceptionDetails));
  return /** @type {T} */ (r.result.value);
}
/** @param {string} targetId */
async function attach(targetId) {
  return (await browser.call("Target.attachToTarget", {
    targetId,
    flatten: true,
  })).sessionId;
}
const uiExpression = `(${snapshotUI.toString()})(document)`;
/** @param {string} sessionId @param {string} label @returns {Promise<import("./crx-test-types.ts").UISnapshot>} */
async function snapshot(sessionId, label) {
  const value =
    /** @type {import("./crx-test-types.ts").UISnapshot} */ (await evaluate(
      sessionId,
      uiExpression,
    ));
  report.extension.snapshots.push({ label, ...value });
  save();
  return value;
}
/** @param {string} sessionId @param {string} text */
async function clickText(sessionId, text) {
  const point = /** @type {{x:number,y:number}|null} */ (await evaluate(
    sessionId,
    `(()=>{const all=[];const walk=r=>{for(const e of r.querySelectorAll('*')){all.push(e);if(e.shadowRoot)walk(e.shadowRoot)}};walk(document);const e=all.find(e=>e.matches('button,a,[role=button],span')&&e.textContent.trim()===${
      JSON.stringify(text)
    }&&e.getBoundingClientRect().width);if(!e)return null;const r=e.getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2}})()`,
  ));
  if (!point) throw Error("Actual visible product control missing: " + text);
  for (const type of ["mousePressed", "mouseReleased"]) {
    await browser.call("Input.dispatchMouseEvent", {
      type,
      ...point,
      button: "left",
      clickCount: 1,
    }, sessionId);
  }
}
try {
  report.version = await browser.call("Browser.getVersion");
  if (report.version.product !== `Chrome/${reference.browserVersion}`) {
    throw Error("Reference browser version mismatch");
  }
  await browser.call("Browser.setDownloadBehavior", {
    behavior: "allow",
    downloadPath: downloads,
    eventsEnabled: true,
  });
  const loaded = await browser.call("Extensions.loadUnpacked", {
    path: join(root, "_dist/chrome-extension-corpus/chrome-unpacked", id),
  });
  if (loaded.id !== id) throw Error("Original extension identity mismatch");
  await delay(1700);
  const page = await browser.call("Target.createTarget", {
    url: report.fixture.url,
  });
  const pageSession = await attach(page.targetId);
  await until(
    () =>
      evaluate(pageSession, "document.querySelector('video')?.currentTime>0.2"),
    "actual authored video playback",
  );
  await delay(1500);
  await browser.call("Target.activateTarget", { targetId: page.targetId });
  const tab = await until(
    async () =>
      (await browser.call("Target.getTargets", { filter: [{ type: "tab" }] }))
        .targetInfos.find((t) => t.url === report.fixture.url),
    "authored native tab",
  );
  await browser.call("Extensions.triggerAction", {
    id,
    targetId: tab.targetId,
  });
  const target = await until(
    async () =>
      (await browser.call("Target.getTargets")).targetInfos.find((t) =>
        t.type === "page" && /^chrome-extension:/.test(t.url) &&
        /content\/(sidebar|popup)\.html/.test(t.url)
      ),
    "actual DownloadHelper panel",
  );
  const session = await attach(target.targetId);
  await delay(1200);
  const firstPanel = await snapshot(session, "actual-panel");
  if (firstPanel.controls.some((c) => c.label === "Accept")) {
    await clickText(session, "Accept");
    report.extension.acceptedVendorTermsForAuthoredMedia = true;
    await delay(700);
    await snapshot(session, "accepted-normal-vendor-terms");
  }
  // The visible media-row Download control is part of the unchanged vendor UI.
  await clickText(session, "Download");
  await delay(1200);
  await snapshot(session, "after-download-click");
  const saved = await until(
    () =>
      readdirSync(downloads).filter((name) => /\.(mp4|mkv|webm)$/i.test(name))
        .map((name) => join(downloads, name)).find((path) =>
          statSync(path).size > 1000
        ),
    "actual completed media file",
    45000,
  );
  const probe =
    /** @type {import("./crx-test-types.ts").MediaProbe} */ (JSON.parse(
      execFileSync("ffprobe", [
        "-v",
        "error",
        "-show_streams",
        "-show_format",
        "-of",
        "json",
        saved,
      ], { encoding: "utf8" }),
    ));
  if (
    !probe.streams.some((s) => s.codec_type === "video") ||
    !probe.streams.some((s) => s.codec_type === "audio") ||
    Number(probe.format.duration) < 3.5
  ) {
    throw Error(
      "Saved media does not contain the authored video/audio duration",
    );
  }
  /** @param {string} path */
  const decodedHash = (path) =>
    execFileSync("ffmpeg", [
      "-nostdin",
      "-v",
      "error",
      "-i",
      path,
      "-map",
      "0:v:0",
      "-frames:v",
      "3",
      "-f",
      "hash",
      "-hash",
      "sha256",
      "-",
    ], { encoding: "utf8" }).trim();
  const actualFrames = decodedHash(saved);
  if (actualFrames !== decodedHash(mediaPath)) {
    throw Error("Saved decoded video frames differ from the authored fixture");
  }
  report.extension.savedFile = {
    path: saved,
    bytes: statSync(saved).size,
    sha256: sha(readFileSync(saved)),
    streams: probe.streams.map((s) => ({
      type: s.codec_type,
      codec: s.codec_name,
    })),
    duration: probe.format.duration,
    decodedFirstThreeFramesSha256: actualFrames,
  };
  report.extension.primaryOperationResult = "passed";
} catch (error) {
  report.extension.primaryOperationResult = "failed";
  report.extension.error = String(error);
  process.exitCode = 1;
} finally {
  report.events = (browser.events || []).filter((e) =>
    /download|exception/i.test(e.method)
  );
  save();
  await browser.close();
  server.close();
  console.log(
    JSON.stringify({
      output,
      result: report.extension.primaryOperationResult,
      error: report.extension.error,
    }),
  );
}
