#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
// Drive unmodified frozen capture extensions through their real action/UI.
// UI/account failures are evidence, never primary-operation passes.
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
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
const tools = dirname(fileURLToPath(import.meta.url)),
  root = resolve(tools, "../..");
const option = (name, fallback) => {
  const i = process.argv.indexOf(name);
  return i < 0 ? fallback : process.argv[i + 1];
};
const chrome = resolve(
  option(
    "--chrome",
    join(root, "_dist/chrome-reference/extracted/opt/google/chrome/chrome"),
  ),
);
const reference = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json")),
);
const sha = (bytes) => createHash("sha256").update(bytes).digest("hex");
if (sha(readFileSync(chrome)) !== reference.binary.sha256) {
  throw Error("Reference binary hash mismatch");
}
const corpus = JSON.parse(readFileSync(join(tools, "corpus-lock.json"))),
  selected = option("--ids", "").split(",").filter(Boolean);
const parent = join(root, "_dist/chrome-runtime-tests");
mkdirSync(parent, { recursive: true });
const output = mkdtempSync(join(parent, "corpus-capture-reference-")),
  downloads = join(output, "downloads");
mkdirSync(downloads);
const report = {
  stage: "actual-package-primary-operation",
  browser: "Chrome",
  reference,
  fixture: "authored local page",
  qualified: false,
  primaryOperationPassRate: null,
  extensions: [],
  output,
};
const save = () =>
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
async function until(read, label, ms = 20000) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await delay(150);
  }
  throw Error("Timed out waiting for " + label);
}
const server = createServer((req, res) => {
  res.writeHead(200, {
    "Content-Type": "text/html; charset=utf-8",
    "Cache-Control": "no-store",
  });
  res.end(
    "<!doctype html><meta charset=utf-8><title>Authored capture corpus page</title><style>body{margin:0;background:rgb(29,170,85);font:24px sans-serif}header{height:150px;background:rgb(41,126,216);color:white;padding:20px}section{height:2400px;background:linear-gradient(rgb(29,170,85),rgb(169,85,204))}</style><header>Floorp frozen extension capture verification</header><section>This authored page must appear in the extension-produced saved image.<p>Controlled page, no account or third-party content.</p></section>",
  );
});
await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
const fixture = `http://127.0.0.1:${server.address().port}/`,
  browser = new ChromePipe(chrome, output, [], {
    headed: process.argv.includes("--headed"),
  }),
  events = [];
// Observe native CDP events without changing the shared pipe helper.
let eventBuffer = "";
browser.process.stdio[4].on("data", (chunk) => {
  eventBuffer += chunk;
  let i;
  while ((i = eventBuffer.indexOf("\0")) >= 0) {
    const raw = eventBuffer.slice(0, i);
    eventBuffer = eventBuffer.slice(i + 1);
    if (!raw) continue;
    const event = JSON.parse(raw);
    if (event.method) events.push(event);
  }
});
async function evaluate(sessionId, expression) {
  const value = await browser.call("Runtime.evaluate", {
    expression,
    returnByValue: true,
    awaitPromise: true,
    userGesture: true,
  }, sessionId);
  if (value.exceptionDetails) {
    throw Error(JSON.stringify(value.exceptionDetails));
  }
  return value.result.value;
}
async function attach(targetId) {
  return (await browser.call("Target.attachToTarget", {
    targetId,
    flatten: true,
  })).sessionId;
}
async function targets(id) {
  return (await browser.call("Target.getTargets")).targetInfos.filter(
    (target) =>
      target.url.startsWith(`chrome-extension://${id}/`) &&
      target.type === "page",
  );
}
async function snapshot(target) {
  const sessionId = await attach(target.targetId);
  const ui = await evaluate(
    sessionId,
    `({url:location.href,title:document.title,text:document.body?.innerText?.slice(0,16000),controls:[...document.querySelectorAll('button,a,input,[role="button"],.capture-menu>div')].filter(e=>e.getBoundingClientRect().width&&e.getBoundingClientRect().height).slice(0,100).map(e=>({tag:e.tagName,id:e.id,type:e.type,text:(e.innerText||e.getAttribute('aria-label')||'').slice(0,200)})),images:[...document.images].filter(i=>i.naturalWidth>200).map(i=>({width:i.naturalWidth,height:i.naturalHeight,src:i.src.slice(0,150)}))})`,
  );
  return { sessionId, ui };
}
async function click(sessionId, selector) {
  const rect = await evaluate(
    sessionId,
    `(()=>{const e=document.querySelector(${
      JSON.stringify(selector)
    });if(!e)return null;e.scrollIntoView({block:'center'});const r=e.getBoundingClientRect();return r.width&&r.height?{x:r.x+r.width/2,y:r.y+r.height/2}:null})()`,
  );
  if (!rect) throw Error("Visible UI control missing: " + selector);
  await browser.call("Input.dispatchMouseEvent", {
    type: "mousePressed",
    ...rect,
    button: "left",
    clickCount: 1,
  }, sessionId);
  await browser.call("Input.dispatchMouseEvent", {
    type: "mouseReleased",
    ...rect,
    button: "left",
    clickCount: 1,
  }, sessionId);
}
try {
  report.version = await browser.call("Browser.getVersion");
  if (report.version.product !== `Chrome/${reference.browserVersion}`) {
    throw Error("Reference version mismatch");
  }
  const anchorTarget = (await browser.call("Target.getTargets")).targetInfos
    .find((target) => target.type === "page" && target.url === "about:blank")
    ?.targetId;
  await browser.call("Browser.setDownloadBehavior", {
    behavior: "allow",
    downloadPath: downloads,
    eventsEnabled: true,
  });
  for (
    const extension of corpus.extensions.filter((e) =>
      e.area === "capture" && (!selected.length || selected.includes(e.id))
    )
  ) {
    const record = {
      id: extension.id,
      name: extension.name,
      version: extension.version,
      packageSha256: extension.packageSha256,
      primaryOperation: extension.primaryOperation,
      primaryOperationResult: "not-run",
      steps: [],
    };
    report.extensions.push(record);
    save();
    if (extension.status !== "frozen") {
      record.primaryOperationResult = "blocked-package";
      record.reason = extension.error;
      save();
      continue;
    }
    let loaded = false;
    try {
      const archive = join(
        root,
        "_dist/chrome-extension-corpus/packages",
        extension.id + ".crx",
      );
      if (sha(readFileSync(archive)) !== extension.packageSha256) {
        throw Error("Frozen package hash mismatch");
      }
      const { id } = await browser.call("Extensions.loadUnpacked", {
        path: join(
          root,
          "_dist/chrome-extension-corpus/chrome-unpacked",
          extension.id,
        ),
      });
      loaded = true;
      if (id !== extension.id) {
        throw Error("Original extension identity changed");
      }
      await delay(700);
      // Close installation welcome tabs before invoking the actual action on the authored web tab.
      for (const target of await targets(id)) {
        await browser.call("Target.closeTarget", { targetId: target.targetId });
      }
      const { targetId } = await browser.call("Target.createTarget", {
        url: fixture,
      });
      const fixtureSession = await attach(targetId);
      await until(
        () => evaluate(fixtureSession, 'document.readyState==="complete"'),
        "authored page",
      );
      await browser.call("Target.activateTarget", { targetId });
      const nativeTab = await until(
        async () =>
          (await browser.call("Target.getTargets", {
            filter: [{ type: "tab" }],
          })).targetInfos.find((tab) => tab.url === fixture),
        "browser tab target",
      );
      await browser.call("Extensions.triggerAction", {
        id,
        targetId: nativeTab.targetId,
      });
      record.steps.push("Native extension action invoked on authored tab");
      const entry = await until(async () => {
        const list = await targets(id);
        return list.find((t) => /popup|initializer|capture/.test(t.url)) ||
          list[0];
      }, "real action target");
      await delay(800);
      let state = await until(
        async () => {
          const current = await snapshot(entry);
          return current.ui.text?.length > 20 && current;
        },
        "loaded real action UI",
        15000,
      );
      record.entry = state.ui;
      record.steps.push("Actual extension action UI inspected");
      if (extension.name === "GoFullPage") {
        const capture = await until(
          async () => {
            const list = await targets(id);
            for (const target of list) {
              if (!/capture\.html/.test(target.url)) continue;
              const candidate = await snapshot(target);
              if (candidate.ui.images.some((i) => i.height > 1500)) {
                return { target, ...candidate };
              }
            }
            return false;
          },
          "extension stitched full-page image",
          35000,
        );
        record.resultUI = capture.ui;
        await delay(600);
        const image = await evaluate(
          capture.sessionId,
          `(()=>{const image=[...document.images].find(i=>i.naturalHeight>1500);const c=document.createElement('canvas');c.width=image.naturalWidth;c.height=image.naturalHeight;const x=c.getContext('2d');x.drawImage(image,0,0);return{width:c.width,height:c.height,pixel:Array.from(x.getImageData(30,30,1,1).data)}})()`,
        );
        if (
          !(image.height > 2400 && Math.abs(image.pixel[0] - 41) < 4 &&
            Math.abs(image.pixel[1] - 126) < 4 &&
            Math.abs(image.pixel[2] - 216) < 4)
        ) {
          throw Error(
            "Saved extension image does not match authored source: " +
              JSON.stringify(image),
          );
        }
        record.media = image;
        const before = new Set(readdirSync(downloads));
        await click(capture.sessionId, "#btn-download");
        record.steps.push("Clicked actual extension Download image UI");
        if (process.argv.includes("--headed")) {
          await delay(500);
          const hasDownloads = await evaluate(
            capture.sessionId,
            'chrome.permissions.contains({permissions:["downloads"]})',
          );
          if (!hasDownloads) {
            const focus = execFileSync("xdotool", [
              "getwindowfocus",
              "getwindowname",
            ], { encoding: "utf8" });
            if (!focus.includes(capture.ui.title)) {
              throw Error(
                "Native permission dialog lost its owned browser focus",
              );
            }
            execFileSync("xdotool", ["key", "Tab", "Return"]);
            await until(
              () =>
                evaluate(
                  capture.sessionId,
                  'chrome.permissions.contains({permissions:["downloads"]})',
                ),
              "native downloads permission approval",
            );
            record.steps.push(
              "Approved real native downloads permission dialog using focused Allow button",
            );
          }
        }
        const filename = await until(
          () =>
            readdirSync(downloads).find((name) =>
              !before.has(name) && !name.startsWith(".") &&
              !name.endsWith(".crdownload") &&
              statSync(join(downloads, name)).isFile() &&
              statSync(join(downloads, name)).size > 1000
            ),
          "extension saved image",
          20000,
        );
        const path = join(downloads, filename), bytes = readFileSync(path);
        if (
          bytes.length < 1000 ||
          bytes.subarray(0, 8).toString("hex") !== "89504e470d0a1a0a"
        ) {
          throw Error("Extension download is not a usable PNG");
        }
        record.media = {
          ...record.media,
          path,
          bytes: bytes.length,
          sha256: sha(bytes),
        };
        record.primaryOperationResult = "pass";
      } else if (extension.name === "Awesome Screenshot") {
        await click(state.sessionId, '[data-tab="screenshot"]');
        await delay(250);
        await click(state.sessionId, "#visible");
        record.steps.push("Clicked actual extension Visible Part UI");
        const result = await until(async () => {
          const list = await targets(id);
          const target = list.find((t) =>
            /edit|annotat|capture/.test(t.url) && t.targetId !== entry.targetId
          );
          return target && { target, ...await snapshot(target) };
        }, "Awesome Screenshot editor");
        record.resultUI = result.ui;
        record.primaryOperationResult = "needs-recipe";
        record.reason =
          "Actual capture editor opened; save/decoded image verification must complete before pass";
      } else {
        // These products authenticate before offering their recording operation.
        const ui = state.ui;
        if (
          /sign in|log in|create an account|login|sign up/i.test(ui.text || "")
        ) {
          record.primaryOperationResult = "blocked-account";
          record.reason =
            "Real extension requires account authentication before recording";
        } else {
          record.primaryOperationResult = "needs-recipe";
          record.reason =
            "Actual action UI did not yet expose a verified recording/save path";
        }
      }
      const pages = await targets(id);
      record.finalTargets = pages.map(({ type, url, title }) => ({
        type,
        url,
        title,
      }));
    } catch (error) {
      record.primaryOperationResult = "failure";
      record.error = String(error);
      try {
        record.browserPages = (await browser.call("Target.getTargets"))
          .targetInfos.filter((target) => target.type === "page").map((
            { url, title },
          ) => ({ url, title }));
        record.failureUI = [];
        for (const target of await targets(extension.id)) {
          try {
            record.failureUI.push((await snapshot(target)).ui);
          } catch {}
        }
      } catch {}
    } finally {
      if (loaded) {
        try {
          await browser.call("Extensions.uninstall", { id: extension.id });
        } catch {}
      }
      for (
        const target of (await browser.call("Target.getTargets")).targetInfos
          .filter((t) => t.type === "page" && t.targetId !== anchorTarget)
      ) {
        try {
          await browser.call("Target.closeTarget", {
            targetId: target.targetId,
          });
        } catch {}
      }
      save();
      console.log(
        `${record.name}: ${record.primaryOperationResult}${
          record.error ? " " + record.error : ""
        }`,
      );
    }
  }
  report.downloadEvents = events.filter((e) =>
    e.method.startsWith("Browser.download")
  );
} finally {
  await browser.close();
  await new Promise((resolve) => server.close(resolve));
  save();
}
console.log("REPORT " + join(output, "report.json"));
