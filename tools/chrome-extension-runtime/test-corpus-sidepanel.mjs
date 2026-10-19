#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { snapshotUI } from "./corpus-sidepanel-ui.sys.mjs";
import { createHash } from "node:crypto";
import { createServer } from "node:http";
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const tools = join(root, "tools/chrome-extension-runtime");
const option = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index < 0 ? fallback : process.argv[index + 1];
};
const chrome = resolve(
  option(
    "--chrome",
    join(root, "_dist/chrome-reference/extracted/opt/google/chrome/chrome"),
  ),
);
const baseline = JSON.parse(readFileSync(join(tools, "chrome-reference.json")));
if (
  createHash("sha256").update(readFileSync(chrome)).digest("hex") !==
    baseline.binary.sha256
) throw Error("Frozen Chrome binary mismatch");
const corpus = JSON.parse(readFileSync(join(tools, "corpus-lock.json")));
const ids = option("--ids", "").split(",").filter(Boolean);
const selected = corpus.extensions.filter((e) =>
  e.area === "sidePanel" && (!ids.length || ids.includes(e.id))
);
const parent = join(root, "_dist/chrome-runtime-tests");
mkdirSync(parent, { recursive: true });
const output = mkdtempSync(join(parent, "corpus-sidepanel-chrome-"));
const report = {
  stage: "actual-sidepanel-primary-operation",
  reference: baseline,
  extensions: [],
  output,
};
const save = () =>
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
const sentinel =
  "Floorp authored compatibility passage 73195: Arctic foxes cross the quiet bridge.";
const server = createServer((request, response) => {
  response.writeHead(200, {
    "Content-Type": "text/html; charset=utf-8",
    "Cache-Control": "no-store",
  });
  response.end(
    `<!doctype html><title>Authored Floorp SidePanel page ${request.url}</title><article><h1>Authored Floorp compatibility page</h1><p id=authored>${sentinel}</p><p>This page is controlled test content. A draft in the extension panel must survive a switch to the second page and back.</p></article>`,
  );
});
await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
const url = `http://corpus.floorp.test:${server.address().port}/`;
report.authoredFixture = {
  url,
  sentinelSha256: createHash("sha256").update(sentinel).digest("hex"),
  driverSha256: createHash("sha256").update(
    readFileSync(fileURLToPath(import.meta.url)),
  ).digest("hex"),
};
const proxy = process.env.HTTPS_PROXY || process.env.HTTP_PROXY;
const browser = new ChromePipe(chrome, output, [
  "--host-resolver-rules=MAP corpus.floorp.test 127.0.0.1",
  "--proxy-bypass-list=corpus.floorp.test;127.0.0.1;localhost",
  ...(proxy ? [`--proxy-server=${proxy}`] : ["--no-proxy-server"]),
]);
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const evaluate = async (session, expression) => {
  const value = await browser.call("Runtime.evaluate", {
    expression,
    returnByValue: true,
    awaitPromise: true,
  }, session);
  if (value.exceptionDetails) {
    throw Error(JSON.stringify(value.exceptionDetails));
  }
  return value.result.value;
};
const snapshot = () => `(${snapshotUI.toString()})()`;
const query = (selector) =>
  `(()=>{const element=document.querySelector(${
    JSON.stringify(selector)
  });if(!element)return null;const r=element.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2,width:r.width,height:r.height};})()`;
const click = async (session, selector) => {
  const rect = await evaluate(session, query(selector));
  if (!rect || !rect.width || !rect.height) return false;
  await browser.call("Input.dispatchMouseEvent", {
    type: "mousePressed",
    x: rect.x,
    y: rect.y,
    button: "left",
    clickCount: 1,
  }, session);
  await browser.call("Input.dispatchMouseEvent", {
    type: "mouseReleased",
    x: rect.x,
    y: rect.y,
    button: "left",
    clickCount: 1,
  }, session);
  return true;
};
try {
  const version = await browser.call("Browser.getVersion");
  if (version.product !== `Chrome/${baseline.browserVersion}`) {
    throw Error("Reference version mismatch");
  }
  for (const extension of selected) {
    const item = {
      id: extension.id,
      name: extension.name,
      version: extension.version,
      packageSha256: extension.packageSha256,
      primaryOperation: extension.primaryOperation,
      result: "failed",
      checks: [],
    };
    report.extensions.push(item);
    let loaded = false;
    try {
      const crx = readFileSync(
        join(
          root,
          `_dist/chrome-extension-corpus/packages/${extension.id}.crx`,
        ),
      );
      if (
        createHash("sha256").update(crx).digest("hex") !==
          extension.packageSha256
      ) throw Error("Frozen package digest mismatch");
      const { id } = await browser.call("Extensions.loadUnpacked", {
        path: join(
          root,
          `_dist/chrome-extension-corpus/chrome-unpacked/${extension.id}`,
        ),
      });
      loaded = true;
      if (id !== extension.id) throw Error("Original Chrome identity changed");
      await pause(1800);
      const firstURL = url + id + "/";
      item.authoredURL = firstURL;
      item.authoredSourceSha256 = createHash("sha256").update(
        await (await fetch(`http://127.0.0.1:${server.address().port}/${id}/`))
          .text(),
      ).digest("hex");
      const { targetId: first } = await browser.call("Target.createTarget", {
        url: firstURL,
      });
      const { targetId: second } = await browser.call("Target.createTarget", {
        url: firstURL + "second",
      });
      await browser.call("Target.activateTarget", { targetId: first });
      const { sessionId: pageSession } = await browser.call(
        "Target.attachToTarget",
        { targetId: first, flatten: true },
      );
      let authoredReady = false;
      for (let n = 0; n < 40; n++) {
        authoredReady = await evaluate(
          pageSession,
          `document.body?.innerText?.includes(${JSON.stringify(sentinel)})`,
        );
        if (authoredReady) break;
        await pause(100);
      }
      if (!authoredReady) {
        item.result = "harness-fixture-unavailable";
        throw Error("Controlled authored page was not loaded");
      }
      item.checks.push(
        "controlled authored text verified in the actual content tab",
      );
      const nativeTab =
        (await browser.call("Target.getTargets", { filter: [{ type: "tab" }] }))
          .targetInfos.find((info) => info.url === firstURL);
      if (!nativeTab) throw Error("Native authored tab target was not found");
      await browser.call("Extensions.triggerAction", {
        id: extension.id,
        targetId: nativeTab.targetId,
      });
      item.checks.push(
        "real extension toolbar action invoked on authored page",
      );
      const manifest = JSON.parse(
        readFileSync(
          join(
            root,
            `_dist/chrome-extension-corpus/chrome-unpacked/${id}/manifest.json`,
          ),
        ),
      );
      let target;
      for (let attempt = 0; attempt < 45; attempt++) {
        const targets = (await browser.call("Target.getTargets")).targetInfos;
        target = targets.find((t) =>
          t.url.startsWith(`chrome-extension://${id}/`) &&
          t.type === "page"
        );
        if (target) break;
        await pause(200);
      }
      const declaredPanel = manifest.side_panel?.default_path;
      if (
        declaredPanel &&
        (!target || !target.url.includes(declaredPanel.split("?")[0]))
      ) {
        const background = (await browser.call("Target.getTargets")).targetInfos
          .find((t) =>
            t.type === "service_worker" &&
            t.url.startsWith(`chrome-extension://${id}/`)
          );
        if (background || target) {
          const { sessionId: bg } = await browser.call(
            "Target.attachToTarget",
            { targetId: (target || background).targetId, flatten: true },
          );
          const opened = await browser.call("Runtime.evaluate", {
            expression:
              "(async()=>{const window=await chrome.windows.getCurrent();await chrome.sidePanel.open({windowId:window.id});return true;})()",
            awaitPromise: true,
            returnByValue: true,
            userGesture: true,
          }, bg);
          if (opened.exceptionDetails) {
            item.panelApiOpenError = opened.exceptionDetails.text;
          } else {
            item.panelOpenedUsingNativeAPI = true;
            await pause(1500);
            target =
              (await browser.call("Target.getTargets")).targetInfos.find((t) =>
                t.type === "page" &&
                t.url.startsWith(`chrome-extension://${id}/`) &&
                t.url.includes(declaredPanel.split("?")[0])
              ) || target;
          }
        }
      }
      item.targets = (await browser.call("Target.getTargets")).targetInfos
        .filter((t) => t.url.startsWith(`chrome-extension://${id}/`)).map((
          { type, url, title },
        ) => ({ type, url, title }));
      if (!target) {
        target = { targetId: first, type: "page", url: firstURL };
        item.injectedPagePanel = true;
      }
      item.panelTarget = target;
      const { sessionId } = await browser.call("Target.attachToTarget", {
        targetId: target.targetId,
        flatten: true,
      });
      let ui;
      for (let attempt = 0; attempt < 40; attempt++) {
        ui = await evaluate(sessionId, snapshot());
        if (ui.ready === "complete" && ui.text?.length > 10) break;
        await pause(250);
      }
      item.initialUI = ui;
      try {
        const { data } = await browser.call("Page.captureScreenshot", {
          format: "png",
        }, sessionId);
        writeFileSync(
          join(output, `${id}-initial.png`),
          Buffer.from(data, "base64"),
        );
      } catch (error) {
        item.initialScreenshotError = String(error);
      }
      const recipe = JSON.parse(
        readFileSync(join(tools, "corpus-sidepanel-recipes.json")),
      ).find((r) => r.id === id);
      if (recipe?.readSelector) {
        item.readClicked = await click(sessionId, recipe.readSelector);
        await pause(1000);
      }
      item.afterReadUI = await evaluate(sessionId, snapshot());
      const storageTargets = (await browser.call("Target.getTargets"))
        .targetInfos.filter((t) =>
          t.url.startsWith(`chrome-extension://${id}/`) &&
          ["service_worker", "background_page"].includes(t.type)
        );
      item.readEvidence = {
        domContainsAuthoredText: !item.injectedPagePanel &&
            item.afterReadUI.text?.includes(sentinel) ||
          false,
        storageContainsAuthoredText: false,
      };
      for (const background of storageTargets) {
        try {
          const { sessionId: bg } = await browser.call(
            "Target.attachToTarget",
            { targetId: background.targetId, flatten: true },
          );
          item.readEvidence.storageContainsAuthoredText ||= await evaluate(
            bg,
            `(async()=>{const marker=${
              JSON.stringify(sentinel)
            };for(const area of ['local','session']){try{if(JSON.stringify(await chrome.storage[area].get(null)).includes(marker))return true;}catch{}}return false;})()`,
          );
        } catch {}
      }
      const selector = recipe?.draftSelector ||
        "textarea:not([readonly]):not([disabled]),[contenteditable=true]";
      item.draftSelector = selector;
      const draft = `Floorp retained draft ${id.slice(0, 8)} 84219`;
      if (await click(sessionId, selector)) {
        await browser.call("Input.insertText", { text: draft }, sessionId);
        await pause(300);
        const readDraft = () =>
          evaluate(
            sessionId,
            `(()=>{const e=document.querySelector(${
              JSON.stringify(selector)
            });return e?.value??e?.textContent??null;})()`,
          );
        item.draftBefore = await readDraft();
        await browser.call("Target.activateTarget", { targetId: second });
        await pause(250);
        await browser.call("Target.activateTarget", { targetId: first });
        await pause(400);
        item.draftAfter = await readDraft();
        item.draftRetained = item.draftBefore?.includes(draft) &&
          item.draftAfter === item.draftBefore;
      } else item.draftRetained = false;
      item.finalUI = await evaluate(sessionId, snapshot());
      if (item.draftBefore && recipe?.submitForRead) {
        await click(sessionId, selector);
        await evaluate(
          sessionId,
          `(()=>{const e=document.querySelector(${
            JSON.stringify(selector)
          });e.select?.();})()`,
        );
        await browser.call("Input.insertText", {
          text: "Read the current page and quote its first paragraph.",
        }, sessionId);
        await browser.call("Input.dispatchKeyEvent", {
          type: "keyDown",
          key: "Enter",
          code: "Enter",
          windowsVirtualKeyCode: 13,
          nativeVirtualKeyCode: 13,
        }, sessionId);
        await browser.call("Input.dispatchKeyEvent", {
          type: "keyUp",
          key: "Enter",
          code: "Enter",
          windowsVirtualKeyCode: 13,
          nativeVirtualKeyCode: 13,
        }, sessionId);
        await pause(1500);
        item.readAttemptUI = await evaluate(sessionId, snapshot());
      }
      const read = item.readEvidence.domContainsAuthoredText ||
        item.readEvidence.storageContainsAuthoredText;
      if (read && item.draftRetained) item.result = "passed";
      else if (
        /login|log in|sign in|sign up|account|premium/i.test(
          item.readAttemptUI?.text || item.finalUI.text || "",
        ) && !read
      ) {
        item.result = "blocked-account";
        item.blocker =
          "The unchanged extension UI requires an account before authored-page reading can be verified";
      } else {item.blocker = read
          ? "Draft did not survive tab switch"
          : "Authored-page text was not observed after the actual extension read operation";}
      try {
        const finalShot = await browser.call("Page.captureScreenshot", {
          format: "png",
        }, sessionId);
        writeFileSync(
          join(output, `${id}-final.png`),
          Buffer.from(finalShot.data, "base64"),
        );
      } catch (error) {
        item.finalScreenshotError = String(error);
      }
    } catch (error) {
      item.error = String(error);
    } finally {
      if (loaded) {
        try {
          await browser.call("Extensions.uninstall", { id: extension.id });
        } catch (error) {
          item.uninstallError = String(error);
        }
      }
      for (
        const target of (await browser.call("Target.getTargets")).targetInfos
      ) {
        if (target.type === "page" && target.url !== "about:blank") {
          try {
            await browser.call("Target.closeTarget", {
              targetId: target.targetId,
            });
          } catch {}
        }
      }
      save();
      console.log(
        `${item.name}: ${item.result} ${item.blocker || item.error || ""}`,
      );
    }
  }
} finally {
  await browser.close();
  await new Promise((resolve) => server.close(resolve));
  save();
  console.log(`Report: ${join(output, "report.json")}`);
}
