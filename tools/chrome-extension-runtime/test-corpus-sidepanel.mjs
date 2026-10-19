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
/** @param {string} name @param {string} [fallback] */
const option = (name, fallback = "") => {
  const index = process.argv.indexOf(name);
  return index < 0 ? fallback : process.argv[index + 1];
};
const chrome = resolve(
  option(
    "--chrome",
    join(root, "_dist/chrome-reference/extracted/opt/google/chrome/chrome"),
  ),
);
/** @type {import("./sidepanel-test-types.ts").ReferenceMetadata} */
const baseline = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json")).toString("utf8"),
);
if (
  createHash("sha256").update(readFileSync(chrome)).digest("hex") !==
    baseline.binary.sha256
) {
  throw Error("Frozen Chrome binary mismatch");
}
/** @type {import("./corpus-types.ts").FrozenCorpus} */
const corpus = JSON.parse(
  readFileSync(join(tools, "corpus-lock.json")).toString("utf8"),
);
const ids = option("--ids", "").split(",").filter(Boolean);
const guest = process.argv.includes("--guest");
const heapProof = process.argv.includes("--heap-proof");
const selected = corpus.extensions.filter(
  (e) => e.area === "sidePanel" && (!ids.length || ids.includes(e.id)),
);
if (
  !selected.length ||
  ids.some((id) => !selected.some((extension) => extension.id === id))
) {
  throw Error("Select frozen sidePanel package IDs from corpus-lock.json");
}
const parent = join(root, "_dist/chrome-runtime-tests");
mkdirSync(parent, { recursive: true });
const output = mkdtempSync(join(parent, "corpus-sidepanel-chrome-"));
/** @type {import("./sidepanel-test-types.ts").SidePanelReport} */
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
await new Promise((resolve) =>
  server.listen(0, "127.0.0.1", () => resolve(undefined))
);
const address = server.address();
if (!address || typeof address === "string") {
  throw Error("Authored server has no TCP address");
}
const url = `http://127.0.0.1:${address.port}/`;
report.authoredFixture = {
  url,
  sentinelSha256: createHash("sha256").update(sentinel).digest("hex"),
  driverSha256: createHash("sha256")
    .update(readFileSync(fileURLToPath(import.meta.url)))
    .digest("hex"),
};
const proxy = process.env.HTTPS_PROXY || process.env.HTTP_PROXY;
const browser = new ChromePipe(
  chrome,
  output,
  [
    "--host-resolver-rules=MAP corpus.floorp.test 127.0.0.1",
    "--proxy-bypass-list=corpus.floorp.test;127.0.0.1;localhost",
    ...(proxy ? [`--proxy-server=${proxy}`] : ["--no-proxy-server"]),
  ],
  { headed: process.argv.includes("--headed") },
);
/** @param {number} ms */
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template T @param {string|undefined} session @param {string} expression @returns {Promise<T>} */
const evaluate = async (session, expression) => {
  const value = await browser.call(
    "Runtime.evaluate",
    {
      expression,
      returnByValue: true,
      awaitPromise: true,
    },
    session,
  );
  if (value.exceptionDetails) {
    throw Error(JSON.stringify(value.exceptionDetails));
  }
  return /** @type {T} */ (value.result.value);
};
const snapshot = () => `(${snapshotUI.toString()})()`;
/** @param {string} session @returns {Promise<import("./sidepanel-test-types.ts").SidePanelSnapshot>} */
const readSnapshot = (session) => evaluate(session, snapshot());
/** @param {string} selector @param {Document|ShadowRoot|null} [root] @returns {Element|null} */
function findDeep(selector, root = document) {
  if (!root) return null;
  const match = selector.startsWith("text:")
    ? [...root.querySelectorAll("*")]
      .filter(
        (element) =>
          ("innerText" in element && typeof element.innerText === "string"
              ? element.innerText
              : element.textContent || "").replace(/\s+/g, " ")
              .trim() === selector.slice(5) &&
          element.getBoundingClientRect().width,
      )
      .sort((a, b) => a.children.length - b.children.length)[0]
    : root.querySelector(selector);
  if (match) return match;
  for (const element of root.querySelectorAll("*")) {
    if (element.shadowRoot) {
      const nested = findDeep(selector, element.shadowRoot);
      if (nested) return nested;
    }
  }
  return null;
}

/** @param {string} selector */
const query = (selector) =>
  `(()=>{const findDeep=${findDeep.toString()};const element=findDeep(${
    JSON.stringify(
      selector,
    )
  });if(!element)return null;element.scrollIntoView({block:'center',inline:'center'});const r=element.getBoundingClientRect();let hit=document.elementFromPoint(r.x+r.width/2,r.y+r.height/2);while(hit?.shadowRoot){const nested=hit.shadowRoot.elementFromPoint(r.x+r.width/2,r.y+r.height/2);if(!nested||nested===hit)break;hit=nested;}if(!(hit===element||element.contains(hit)||element.parentElement===hit))return null;return {x:r.x+r.width/2,y:r.y+r.height/2,width:r.width,height:r.height};})()`;
/** @param {string} session @param {string} selector */
const click = async (session, selector) => {
  /** @type {import("./sidepanel-test-types.ts").Point|null} */
  const rect = await evaluate(session, query(selector));
  if (!rect || !rect.width || !rect.height) return false;
  await browser.call(
    "Input.dispatchMouseEvent",
    {
      type: "mousePressed",
      x: rect.x,
      y: rect.y,
      button: "left",
      clickCount: 1,
    },
    session,
  );
  await browser.call(
    "Input.dispatchMouseEvent",
    {
      type: "mouseReleased",
      x: rect.x,
      y: rect.y,
      button: "left",
      clickCount: 1,
    },
    session,
  );
  return true;
};
// Inspect the native extension isolate without inserting the authored marker
// into that isolate. A marker in an evaluated comparison would invalidate this
// evidence by adding the marker to the package's own heap.
/** @param {string} sessionId */
const receivedAuthoredContent = async (sessionId) => {
  await browser.call("HeapProfiler.enable", {}, sessionId);
  browser.events = [];
  const requestId = ++browser.next;
  await new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      browser.pending.delete(requestId);
      reject(Error("Native extension heap snapshot timed out"));
    }, 60000);
    browser.pending.set(requestId, {
      resolve(_value) {
        clearTimeout(timer);
        resolve(undefined);
      },
      reject(error) {
        clearTimeout(timer);
        reject(error);
      },
    });
    const transport = browser.process.stdio[3];
    if (!transport || !("write" in transport)) {
      throw Error("Native CDP write pipe unavailable");
    }
    transport.write(
      JSON.stringify({
        id: requestId,
        method: "HeapProfiler.takeHeapSnapshot",
        params: { reportProgress: false },
        sessionId,
      }) + "\0",
    );
  });
  const chunks = (browser.events || [])
    .filter(
      (event) =>
        event.sessionId === sessionId &&
        event.method === "HeapProfiler.addHeapSnapshotChunk",
    )
    .flatMap((event) =>
      typeof event.params?.chunk === "string" ? [event.params.chunk] : []
    );
  browser.events = [];
  /** @type {{strings:string[]}} */
  const heap = JSON.parse(chunks.join(""));
  return {
    nativeHeapContainsAuthoredText: heap.strings.some((value) =>
      value.includes(sentinel)
    ),
    stringCount: heap.strings.length,
  };
};
try {
  const version = await browser.call("Browser.getVersion");
  if (version.product !== `Chrome/${baseline.browserVersion}`) {
    throw Error("Reference version mismatch");
  }
  for (const extension of selected) {
    /** @type {import("./sidepanel-test-types.ts").SidePanelEvidence} */
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
      ) {
        throw Error("Frozen package digest mismatch");
      }
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
      item.authoredSourceSha256 = createHash("sha256")
        .update(
          await (
            await fetch(`http://127.0.0.1:${address.port}/${id}/`)
          ).text(),
        )
        .digest("hex");
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
      if (guest) {
        // Some packages update their toolbar popup only on tab activation,
        // after their asynchronously imported content script starts listening.
        await pause(2500);
        await browser.call("Target.activateTarget", { targetId: second });
        await pause(100);
        await browser.call("Target.activateTarget", { targetId: first });
        await pause(500);
      }
      const nativeTab = (
        await browser.call("Target.getTargets", { filter: [{ type: "tab" }] })
      ).targetInfos.find((info) => info.url === firstURL);
      if (!nativeTab) throw Error("Native authored tab target was not found");
      await browser.call("Extensions.triggerAction", {
        id: extension.id,
        targetId: nativeTab.targetId,
      });
      item.checks.push(
        "real extension toolbar action invoked on authored page",
      );
      /** @type {{side_panel?:{default_path?:string}}} */
      const manifest = JSON.parse(
        readFileSync(
          join(
            root,
            `_dist/chrome-extension-corpus/chrome-unpacked/${id}/manifest.json`,
          ),
        ).toString("utf8"),
      );
      /** @type {import("./sidepanel-test-types.ts").SidePanelRecipe|undefined} */
      const recipe =
        /** @type {import("./sidepanel-test-types.ts").SidePanelRecipe[]} */ (JSON
          .parse(
            readFileSync(join(tools, "corpus-sidepanel-recipes.json")).toString(
              "utf8",
            ),
          )).find((r) => r.id === id);
      /** @type {({targetId:string;type:string;url:string;title?:string})|undefined} */
      let target;
      for (let attempt = 0; attempt < 45; attempt++) {
        const targets = (await browser.call("Target.getTargets")).targetInfos;
        const pages = targets.filter(
          (t) =>
            t.url.startsWith(`chrome-extension://${id}/`) && t.type === "page",
        );
        target = pages.find(
          (t) =>
            manifest.side_panel?.default_path &&
            t.url.includes(manifest.side_panel.default_path.split("?")[0]),
        ) || pages[0];
        if (!target && guest && recipe?.onboardingOrigin) {
          target = targets.find(
            (candidate) =>
              candidate.type === "page" &&
              candidate.url.startsWith(recipe.onboardingOrigin || ""),
          );
          if (target) item.onboardingOnly = true;
        }
        if (target) break;
        await pause(200);
      }
      const declaredPanel = manifest.side_panel?.default_path;
      if (
        declaredPanel &&
        (!target || !target.url.includes(declaredPanel.split("?")[0]))
      ) {
        const background = (
          await browser.call("Target.getTargets")
        ).targetInfos.find(
          (t) =>
            t.type === "service_worker" &&
            t.url.startsWith(`chrome-extension://${id}/`),
        );
        const contextTarget = target || background;
        if (contextTarget) {
          const { sessionId: bg } = await browser.call(
            "Target.attachToTarget",
            { targetId: contextTarget.targetId, flatten: true },
          );
          const opened = await browser.call(
            "Runtime.evaluate",
            {
              expression:
                "(async()=>{const window=await chrome.windows.getCurrent();await chrome.sidePanel.open({windowId:window.id});return true;})()",
              awaitPromise: true,
              returnByValue: true,
              userGesture: true,
            },
            bg,
          );
          if (opened.exceptionDetails) {
            item.panelApiOpenError = opened.exceptionDetails.text;
          } else {
            item.panelOpenedUsingNativeAPI = true;
            await pause(1500);
            target = (await browser.call("Target.getTargets")).targetInfos.find(
              (t) =>
                t.type === "page" &&
                t.url.startsWith(`chrome-extension://${id}/`) &&
                t.url.includes(declaredPanel.split("?")[0]),
            ) || target;
          }
        }
      }
      item.targets = (await browser.call("Target.getTargets")).targetInfos
        .filter((t) => t.url.startsWith(`chrome-extension://${id}/`))
        .map(({ type, url, title }) => ({ type, url, title }));
      if (!target) {
        target = { targetId: first, type: "page", url: firstURL };
        item.injectedPagePanel = true;
      }
      item.panelTarget = target;
      let { sessionId } = await browser.call("Target.attachToTarget", {
        targetId: target.targetId,
        flatten: true,
      });
      /** @type {import("./sidepanel-test-types.ts").SidePanelSnapshot|undefined} */
      let ui;
      for (let attempt = 0; attempt < 40; attempt++) {
        ui = await readSnapshot(sessionId);
        if (ui.ready === "complete" && ui.text?.length > 10) break;
        await pause(250);
      }
      if (!ui) throw Error("Actual panel snapshot unavailable");
      item.initialUI = ui;
      try {
        const { data } = await browser.call(
          "Page.captureScreenshot",
          {
            format: "png",
          },
          sessionId,
        );
        writeFileSync(
          join(output, `${id}-initial.png`),
          Buffer.from(data, "base64"),
        );
      } catch (error) {
        item.initialScreenshotError = String(error);
      }
      if (guest) {
        item.guestSteps = [];
        if (recipe?.refreshText && ui.text?.includes(recipe.refreshText)) {
          /** @type {string|null} */
          const selector = await evaluate(
            sessionId,
            `(()=>{const buttons=[...document.querySelectorAll('button')];const index=buttons.findIndex(element=>element.textContent.trim()===${
              JSON.stringify(recipe.refreshText)
            });return index>=0?'button:nth-of-type('+(index+1)+')':null;})()`,
          );
          if (selector && (await click(sessionId, selector))) {
            item.guestSteps.push({
              action: "actual advertised page refresh",
              text: recipe.refreshText,
            });
            await pause(3000);
            await browser.call("Target.activateTarget", { targetId: first });
            await browser.call("Extensions.triggerAction", {
              id,
              targetId: nativeTab.targetId,
            });
            await pause(1500);
            const pages = (
              await browser.call("Target.getTargets")
            ).targetInfos.filter(
              (t) =>
                t.type === "page" &&
                t.url.startsWith(`chrome-extension://${id}/`),
            );
            const previousURL = target.url;
            target = pages.find((t) => t.url === previousURL) ||
              pages[0] || { targetId: first, url: firstURL, type: "page" };
            item.injectedPagePanel = !target.url.startsWith(
              `chrome-extension://${id}/`,
            );
            item.panelTarget = target;
            ({ sessionId } = await browser.call("Target.attachToTarget", {
              targetId: target.targetId,
              flatten: true,
            }));
            item.afterRefreshUI = await readSnapshot(sessionId);
          }
        }
        for (let round = 0; round < 2; round++) {
          for (const text of recipe?.guestDismissText || []) {
            for (let n = 0; n < 4; n++) {
              if (!(await click(sessionId, "text:" + text))) break;
              await pause(400);
              item.guestSteps.push({
                action: "dismiss tutorial",
                text,
                ui: await readSnapshot(sessionId),
              });
            }
          }
          const editorVisible = recipe?.reopenWhenEditorMissing &&
            await evaluate(
              sessionId,
              `(()=>{const findDeep=${findDeep.toString()};const element=findDeep(${
                JSON.stringify(recipe.reopenWhenEditorMissing)
              });return !!element?.getBoundingClientRect().width;})()`,
            );
          if (!recipe?.reopenWhenEditorMissing || editorVisible || round > 0) {
            break;
          }
          await browser.call("Target.activateTarget", { targetId: first });
          await browser.call("Extensions.triggerAction", {
            id,
            targetId: nativeTab.targetId,
          });
          await pause(1500);
          item.guestSteps.push({
            action: "reopen native toolbar action after first-use prompt",
            ui: await readSnapshot(sessionId),
          });
        }
        // Dismiss only real, advertised close controls. No package state or
        // authentication result is written by the driver.
        for (const selector of recipe?.dismissSelectors || []) {
          for (let n = 0; n < 4; n++) {
            if (!(await click(sessionId, selector))) break;
            await pause(400);
            item.guestSteps.push({
              action: "dismiss",
              selector,
              ui: await readSnapshot(sessionId),
            });
          }
        }
      }
      if (guest && recipe?.guestReadText) {
        item.guestReadTarget = await evaluate(
          sessionId,
          `(()=>{const findDeep=${findDeep.toString()};const element=findDeep(${
            JSON.stringify("text:" + recipe.guestReadText)
          });if(!element)return null;const r=element.getBoundingClientRect();return {html:element.outerHTML.slice(0,1500),disabled:element.disabled,ariaDisabled:element.getAttribute('aria-disabled'),pointerEvents:getComputedStyle(element).pointerEvents,rect:{x:r.x,y:r.y,width:r.width,height:r.height}};})()`,
        );
        item.guestReadClicked = await click(
          sessionId,
          "text:" + recipe.guestReadText,
        );
        await pause(1000);
      }
      if (recipe?.readSelector) {
        item.readClicked = await click(sessionId, recipe.readSelector);
        await pause(1000);
      }
      item.afterReadUI = await readSnapshot(sessionId);
      const storageTargets = (
        await browser.call("Target.getTargets")
      ).targetInfos.filter(
        (t) =>
          t.url.startsWith(`chrome-extension://${id}/`) &&
          ["service_worker", "background_page"].includes(t.type),
      );
      item.readEvidence = {
        domContainsAuthoredText: (!item.onboardingOnly &&
          (item.injectedPagePanel
            ? item.afterReadUI.shadowText?.includes(sentinel)
            : item.afterReadUI.text?.includes(sentinel))) ||
          false,
        storageContainsAuthoredText: false,
      };
      if (
        heapProof &&
        !item.injectedPagePanel &&
        target.url.startsWith(`chrome-extension://${id}/`)
      ) {
        item.readEvidence = {
          ...item.readEvidence,
          ...(await receivedAuthoredContent(sessionId)),
        };
      }
      for (const background of storageTargets) {
        try {
          const { sessionId: bg } = await browser.call(
            "Target.attachToTarget",
            { targetId: background.targetId, flatten: true },
          );
          item.readEvidence.storageContainsAuthoredText ||= await evaluate(
            bg,
            `(async()=>{const values=[];for(const area of ['local','session']){try{values.push(JSON.stringify(await chrome.storage[area].get(null)));}catch{}}return values;})()`,
          ).then(/** @param {string[]} values */ (values) =>
            values.some((value) => value.includes(sentinel))
          );
        } catch {
          // An extension view may disappear while its optional evidence is inspected.
        }
      }
      const selector = recipe?.draftSelector ||
        "textarea:not([readonly]):not([disabled]),[contenteditable=true]";
      item.draftSelector = selector;
      const draft = `Floorp retained draft ${id.slice(0, 8)} 84219`;
      if (!item.onboardingOnly && (await click(sessionId, selector))) {
        await browser.call("Input.insertText", { text: draft }, sessionId);
        await pause(300);
        /** @returns {Promise<string|null>} */
        const readDraft = () =>
          evaluate(
            sessionId,
            `(()=>{const findDeep=${findDeep.toString()};const e=findDeep(${
              JSON.stringify(
                selector,
              )
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
      item.finalUI = await readSnapshot(sessionId);
      if (guest) {
        item.otherExtensionViews = [];
        for (
          const candidate of (
            await browser.call("Target.getTargets")
          ).targetInfos.filter(
            (t) =>
              t.type === "page" &&
              t.targetId !== target.targetId &&
              t.url.startsWith(`chrome-extension://${id}/`),
          )
        ) {
          try {
            const { sessionId: other } = await browser.call(
              "Target.attachToTarget",
              { targetId: candidate.targetId, flatten: true },
            );
            item.otherExtensionViews.push(await readSnapshot(other));
          } catch {
            // An extension view may disappear while its optional evidence is inspected.
          }
        }
      }
      if (!guest && item.draftBefore && recipe?.submitForRead) {
        await click(sessionId, selector);
        await evaluate(
          sessionId,
          `(()=>{const e=document.querySelector(${
            JSON.stringify(
              selector,
            )
          });e.select?.();})()`,
        );
        await browser.call(
          "Input.insertText",
          {
            text: "Read the current page and quote its first paragraph.",
          },
          sessionId,
        );
        await browser.call(
          "Input.dispatchKeyEvent",
          {
            type: "keyDown",
            key: "Enter",
            code: "Enter",
            windowsVirtualKeyCode: 13,
            nativeVirtualKeyCode: 13,
          },
          sessionId,
        );
        await browser.call(
          "Input.dispatchKeyEvent",
          {
            type: "keyUp",
            key: "Enter",
            code: "Enter",
            windowsVirtualKeyCode: 13,
            nativeVirtualKeyCode: 13,
          },
          sessionId,
        );
        await pause(1500);
        item.readAttemptUI = await readSnapshot(sessionId);
      }
      const read = item.readEvidence.domContainsAuthoredText ||
        item.readEvidence.storageContainsAuthoredText ||
        item.readEvidence.nativeHeapContainsAuthoredText;
      if (read && item.draftRetained) item.result = "passed";
      else if (
        item.onboardingOnly &&
        /upstream connect error|ERR_(?:CERT|CONNECTION|NAME_NOT_RESOLVED)|No approved upstream/i
          .test(
            item.finalUI.text || "",
          )
      ) {
        item.result = "blocked-external-service";
        item.blocker =
          "The package's actual account entry page could not reach its external service";
      } else if (
        /login|log in|sign in|sign up|account|premium/i.test(
          item.readAttemptUI?.text || item.finalUI.text || "",
        ) &&
        !read
      ) {
        item.result = "blocked-account";
        item.blocker =
          "The unchanged extension UI requires an account before authored-page reading can be verified";
      } else {
        item.blocker = read
          ? "Draft did not survive tab switch"
          : "Authored-page text was not observed after the actual extension read operation";
      }
      try {
        const finalShot = await browser.call(
          "Page.captureScreenshot",
          {
            format: "png",
          },
          sessionId,
        );
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
        const target of (await browser.call("Target.getTargets"))
          .targetInfos
      ) {
        if (target.type === "page" && target.url !== "about:blank") {
          try {
            await browser.call("Target.closeTarget", {
              targetId: target.targetId,
            });
          } catch {
            // An extension view may disappear while its optional evidence is inspected.
          }
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
