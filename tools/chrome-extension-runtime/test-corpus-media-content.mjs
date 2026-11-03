#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
/** @import {Truthy, CorpusLock, CorpusReport, CorpusOperation, MediaSnapshot, Point, MediaFixture} from "./assessment-test-types.ts" */
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
const tools = dirname(fileURLToPath(import.meta.url));
const output = process.env.FLOORP_MEDIA_CORPUS_OUTPUT,
  chrome = process.env.FLOORP_MEDIA_CHROME;
if (!output || !chrome) {
  throw Error("Private fixture output and trusted Chrome wrapper are required");
}
/** @type {MediaFixture} */
const fixture = JSON.parse(
  readFileSync(join(output, "fixture.json")).toString("utf8"),
);
/** @type {CorpusLock} */
const corpus = JSON.parse(
  readFileSync(join(tools, "corpus-lock.json")).toString("utf8"),
);
const index = process.argv.indexOf("--ids"),
  ids =
    (index < 0 ? "ponfpcnoihfmfllpaingbgckeeldkhle" : process.argv[index + 1])
      .split(",");
/** @type {CorpusReport<MediaSnapshot>} */
const report = {
  stage: "actual-package-primary-operation",
  reference: JSON.parse(
    readFileSync(join(tools, "chrome-reference.json")).toString("utf8"),
  ),
  fixture,
  extensions: [],
  output,
  primaryOperationPassRate: null,
};
const save = () =>
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
const browser = new ChromePipe(chrome, output, [
  "--no-proxy-server",
  "--host-resolver-rules=" +
  fixture.hosts.map((host) => `MAP ${host} 127.0.0.1`).join(", "),
], { headed: process.argv.includes("--headed") });
browser.events = [];
/** @param {number} ms @returns {Promise<void>} */
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template T @param {string} sessionId @param {string} expression @returns {Promise<T>} */
async function evaluate(sessionId, expression) {
  const r = await browser.call("Runtime.evaluate", {
    expression,
    returnByValue: true,
    awaitPromise: true,
  }, sessionId);
  if (r.exceptionDetails) throw Error(JSON.stringify(r.exceptionDetails));
  return /** @type {T} */ (r.result.value);
}
/** @template T @param {()=>T|Promise<T>} read @param {string} label @returns {Promise<Truthy<T>>} */
async function until(read, label) {
  const end = Date.now() + 25000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return /** @type {Truthy<T>} */ (value);
    await delay(100);
  }
  throw Error("Timed out waiting for " + label);
}
/** @param {string} sessionId @param {string} selector */
async function click(sessionId, selector) {
  /** @type {Point} */
  const point = await evaluate(
    sessionId,
    `(()=>{const e=document.querySelector(${
      JSON.stringify(selector)
    });if(!e)throw Error('Missing authored control');e.scrollIntoView({block:'center'});const r=e.getBoundingClientRect();if(!r.width||!r.height)throw Error('Hidden authored control');return{x:r.x+r.width/2,y:r.y+r.height/2}})()`,
  );
  for (const [type, buttons] of [["mousePressed", 1], ["mouseReleased", 0]]) {
    await browser.call("Input.dispatchMouseEvent", {
      type,
      ...point,
      button: "left",
      buttons,
      clickCount: 1,
    }, sessionId);
  }
}
const snapshotExpression =
  `(()=>{const v=document.querySelector('video');return {url:location.href,ready:document.readyState,visibility:document.visibilityState,focused:document.hasFocus(),video:v&&{readyState:v.readyState,paused:v.paused,currentTime:v.currentTime,duration:v.duration,playbackRate:v.playbackRate,seeking:v.seeking,seekable:Array.from({length:v.seekable.length},(_,i)=>[v.seekable.start(i),v.seekable.end(i)]),error:v.error?.code},bezel:document.querySelector('#efyt-bezel')?.textContent,body:document.body.innerText.slice(0,10000),dislike:document.querySelector('dislike-button-view-model')?.textContent}})()`;
/** @param {string} sessionId @returns {Promise<MediaSnapshot>} */
const readMediaSnapshot = (sessionId) =>
  evaluate(sessionId, snapshotExpression);
/** @param {string} sessionId @param {string} targetId */

async function readyAndPlay(sessionId, targetId) {
  await browser.call("Target.activateTarget", { targetId });
  await browser.call("Page.bringToFront", {}, sessionId);
  await until(async () => {
    const value = await readMediaSnapshot(sessionId).catch(() => null);
    return value?.ready === "complete" && (value.video?.readyState ?? 0) >= 2 &&
      value;
  }, "real native decoded media");
  await click(sessionId, "#play");
  return until(async () => {
    const value = await readMediaSnapshot(sessionId);
    return value.video && !value.video.paused &&
      value.video.currentTime > 0.1 && value;
  }, "real native media playback");
}
/** @param {string} sessionId @param {string} targetId */
async function controlSpeed(sessionId, targetId) {
  await browser.call("Target.activateTarget", { targetId });
  await browser.call("Page.bringToFront", {}, sessionId);
  /** @type {Point} */
  const point = await evaluate(
    sessionId,
    `(()=>{const r=document.querySelector('video').getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2}})()`,
  );
  await browser.call("Input.dispatchMouseEvent", {
    type: "mouseMoved",
    ...point,
    button: "none",
    modifiers: 2,
  }, sessionId).catch((error) => {
    throw Error("Native pointer movement failed: " + error);
  });
  await browser.call("Input.dispatchMouseEvent", {
    type: "mouseWheel",
    ...point,
    deltaX: 0,
    deltaY: -120,
    modifiers: 2,
  }, sessionId).catch((error) => {
    throw Error("Native Ctrl-wheel failed: " + error);
  });
}
/** @param {string} sessionId @param {string} [url] */
async function navigate(sessionId, url) {
  const priorOrigin = await evaluate(sessionId, "performance.timeOrigin");
  await browser.call(
    url ? "Page.navigate" : "Page.reload",
    url ? { url } : {},
    sessionId,
  );
  await until(
    async () =>
      await evaluate(
        sessionId,
        `document.readyState==='complete'&&performance.timeOrigin!==${
          JSON.stringify(priorOrigin)
        }${url ? "&&location.href===" + JSON.stringify(url) : ""}`,
      ).catch(() => false),
    "new native document after navigation",
  );
}
try {
  report.version = await browser.call("Browser.getVersion");
  for (const id of ids) {
    const record = corpus.extensions.find((item) => item.id === id);
    if (!record) throw Error("Unknown frozen package " + id);
    /** @type {CorpusOperation<MediaSnapshot>} */
    const item = {
      id,
      name: record.name,
      version: record.version,
      packageSha256: record.packageSha256,
      sourceIntegrity: fixture.sourceIntegrity?.[id],
      primaryOperation: record.primaryOperation,
      primaryOperationResult: "not-run",
      checks: [],
    };
    report.extensions.push(item);
    let loaded = false;
    let verifyRemoval;
    const url = record.name === "Enhancer for YouTube"
      ? "https://www.youtube.com/embed/FloorpTest1"
      : "https://www.youtube.com/watch?v=FloorpTest1";
    const { targetId } = await browser.call("Target.createTarget", { url });
    const { sessionId } = await browser.call("Target.attachToTarget", {
      targetId,
      flatten: true,
    });
    await browser.call("Target.activateTarget", { targetId });
    await browser.call("Page.bringToFront", {}, sessionId);
    await browser.call("Runtime.enable", {}, sessionId);
    await browser.call("Log.enable", {}, sessionId);
    try {
      item.baseline = record.name === "Enhancer for YouTube"
        ? await readyAndPlay(sessionId, targetId)
        : await until(async () => {
          const value = await readMediaSnapshot(sessionId).catch(() => null);
          return value?.ready === "complete" && value.dislike && value;
        }, "authored reaction controls");
      const priorTargets = new Set(
        (await browser.call("Target.getTargets")).targetInfos.map((target) =>
          target.targetId
        ),
      );
      const loadedExtension = await browser.call("Extensions.loadUnpacked", {
        path: join(output, "chrome-packages", id),
      });
      loaded = true;
      if (loadedExtension.id !== id) {
        throw Error("Frozen extension ID mismatch");
      }
      if (["Enhancer for YouTube", "SponsorBlock"].includes(record.name)) {
        await delay(500);
        item.onboardingTargets = (await browser.call("Target.getTargets"))
          .targetInfos.filter((target) =>
            target.type === "page" && !priorTargets.has(target.targetId)
          );
        for (const target of item.onboardingTargets) {
          await browser.call("Target.closeTarget", {
            targetId: target.targetId,
          });
        }
      }
      await browser.call("Target.activateTarget", { targetId });
      await browser.call("Page.bringToFront", {}, sessionId);
      await navigate(sessionId);
      if (record.name === "Enhancer for YouTube") {
        item.playing = await readyAndPlay(sessionId, targetId);
        await until(
          async () =>
            await evaluate(sessionId, `!!document.querySelector('#efyt-bezel')`)
              .catch(() => false),
          "unchanged MAIN-world extension initialization",
        );
        await controlSpeed(sessionId, targetId);
        item.transformed = await until(async () => {
          const value = await readMediaSnapshot(sessionId);
          item.lastSnapshot = value;
          return value.video &&
            Math.abs(value.video.playbackRate - 1.02) < 0.00001 &&
            value.bezel === "1.02x" && value;
        }, "actual extension native playback speed change");
        item.checks.push(
          "Native Ctrl+wheel invokes the unchanged extension and changes actual HTMLVideoElement.playbackRate with visible speed feedback",
        );
        await navigate(sessionId, url + "?second");
        await readyAndPlay(sessionId, targetId);
        await until(
          async () =>
            await evaluate(sessionId, `!!document.querySelector('#efyt-bezel')`)
              .catch(() => false),
          "extension after navigation",
        );
        await controlSpeed(sessionId, targetId);
        item.afterNavigation = await until(async () => {
          const value = await readMediaSnapshot(sessionId);
          return value.video &&
            Math.abs(value.video.playbackRate - 1.02) < 0.00001 &&
            value.bezel === "1.02x" && value;
        }, "native speed control after navigation");
        item.checks.push(
          "Real navigation retains functioning native player controls",
        );
      } else if (record.name === "Return YouTube Dislike") {
        item.transformed = await until(async () => {
          const value = await readMediaSnapshot(sessionId);
          item.lastSnapshot = value;
          return value.dislike?.includes("123") && value;
        }, "native extension metadata rendered in authored reaction control");
        item.checks.push(
          "Unchanged signed extension uses real TLS transport to render authored dislike metadata into native page controls",
        );
        await navigate(sessionId, url + "&second");
        item.afterNavigation = await until(async () => {
          const value = await readMediaSnapshot(sessionId);
          return value.dislike?.includes("123") && value;
        }, "dislike metadata after native navigation");
        item.checks.push("Real navigation retains actual metadata rendering");
      } else if (record.name === "SponsorBlock") {
        const { runSponsorReference } = await import(
          "./corpus-sponsor-reference.mjs"
        );
        verifyRemoval = await runSponsorReference({
          browser,
          targetId,
          item,
          url,
          snapshot: () => readMediaSnapshot(sessionId),
          until,
          navigate: (next) => navigate(sessionId, next),
          readyAndPlay: () => readyAndPlay(sessionId, targetId),
          delay,
        });
      } else throw Error("Primary recipe not implemented for " + record.name);
      await browser.call("Extensions.uninstall", { id });
      loaded = false;
      await browser.call("Target.activateTarget", { targetId });
      await browser.call("Page.bringToFront", {}, sessionId);
      await navigate(sessionId);
      if (record.name === "Enhancer for YouTube") {
        await readyAndPlay(sessionId, targetId);
        await controlSpeed(sessionId, targetId);
        await delay(200);
      }
      if (verifyRemoval) await verifyRemoval();
      item.afterRemoval = await readMediaSnapshot(sessionId);
      if (
        record.name === "Enhancer for YouTube"
          ? item.afterRemoval.bezel !== undefined ||
            item.afterRemoval.video?.playbackRate !== 1
          : item.afterRemoval.dislike !== item.baseline.dislike
      ) throw Error("Uninstall did not restore authored native behavior");
      item.checks.push(
        "Actual uninstall restores the authored native control behavior",
      );
      item.primaryOperationResult = "passed";
    } catch (error) {
      item.primaryOperationResult = "failed";
      item.error = String(error);
      item.lastSnapshot = await readMediaSnapshot(sessionId).catch(() =>
        undefined
      );
    } finally {
      if (loaded) {
        await browser.call("Extensions.uninstall", { id }).catch(() => {});
      }
      item.nativeEvents = browser.events.filter((event) =>
        event.method === "Runtime.exceptionThrown" ||
        event.method === "Log.entryAdded"
      );
      await browser.call("Target.closeTarget", { targetId }).catch(() => {});
      save();
    }
  }
} finally {
  await browser.close();
  save();
}
console.log(
  JSON.stringify(
    report.extensions.map(({ name, primaryOperationResult, error }) => ({
      name,
      primaryOperationResult,
      error,
    })),
  ),
);
console.log("REPORT " + join(output, "report.json"));
if (
  !report.extensions.length ||
  report.extensions.some((item) => item.primaryOperationResult !== "passed")
) process.exitCode = 1;
