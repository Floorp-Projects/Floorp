#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
// Real signed Grammarly UI, native authored text input, and ordinary native TLS.
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { evaluateNativeUI, waitForNativeUI } from "./chrome-devtools-panel.mjs";
import { verifyCorpusScripts } from "./corpus-script-integrity.mjs";
import { createHash } from "node:crypto";
import { createServer } from "node:http";
import { execFileSync } from "node:child_process";
import { cpSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
const root = resolve(dirname(fileURLToPath(import.meta.url)), "../.."),
  tools = join(root, "tools/chrome-extension-runtime");
const output = mkdtempSync(
  join(root, "_dist/chrome-runtime-tests/corpus-grammarly-reference-"),
);
/** @param {import("node:crypto").BinaryLike} bytes */
const digest = (bytes) => createHash("sha256").update(bytes).digest("hex");
const id = "kbfnbcaeplbcioakkpcpgfkobkghlhen";
/** @type {import("./grammarly-test-types.ts").GrammarlyCorpus} */
const corpus = JSON.parse(
  readFileSync(join(tools, "corpus-lock.json"), "utf8"),
);
const record = corpus.extensions.find((record) => record.id === id);
if (!record) throw Error("Frozen Grammarly corpus entry is unavailable");
/** @type {import("./grammarly-test-types.ts").GrammarlyReference} */
const reference = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json"), "utf8"),
);
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome",
);
if (digest(readFileSync(chrome)) !== reference.binary.sha256) {
  throw Error("Reference binary digest mismatch");
}
/** @type {import("./grammarly-test-types.ts").PrivateChromeTrust} */
const trust = JSON.parse(
  execFileSync("python3", [
    join(tools, "private-browser-trust.py"),
    "--output",
    output,
    "--browser",
    chrome,
  ], { encoding: "utf8" }),
);
const proxy = process.env.https_proxy || process.env.HTTPS_PROXY ||
  process.env.http_proxy || process.env.HTTP_PROXY || process.env.ALL_PROXY;
if (!proxy) {
  throw Error(
    "Normal external-service probe requires the configured environment proxy",
  );
}
const proxyURL = new URL(proxy);
if (proxyURL.protocol !== "http:" || proxyURL.username || proxyURL.password) {
  throw Error("Unsupported environment proxy transport or credentials");
}
const proxyServer = `http://${proxyURL.hostname}:${proxyURL.port || 80}`;
const bypass = (process.env.no_proxy || process.env.NO_PROXY || "").split(",")
  .filter(Boolean).join(";");
const html = readFileSync(join(tools, "fixtures/corpus-writing.html"));
const server = createServer((request, response) => {
  const writing = request.url?.startsWith("/writing");
  response.writeHead(writing ? 200 : 404, {
    "Content-Type": "text/html; charset=utf-8",
    "Cache-Control": "no-store",
  });
  response.end(writing ? html : "Not found");
});
await new Promise((resolve) =>
  server.listen(0, "127.0.0.1", () => resolve(undefined))
);
const address = server.address();
if (!address || typeof address === "string") {
  throw Error("Authored writing server has no TCP address");
}
const url = `http://127.0.0.1:${address.port}/writing`;
/** @type {import("./grammarly-test-types.ts").GrammarlyEvidence} */
const item = {
  id,
  name: record.name,
  version: record.version,
  packageSha256: record.packageSha256,
  result: "not-run",
  primaryOperationResult: "not-run",
};
/** @type {import("./grammarly-test-types.ts").GrammarlyReport} */
const report = {
  stage: "actual-package-writing-primary-prerequisites",
  reference,
  displayMode: "headed",
  fixture: {
    url,
    sha256: digest(html),
    authoredText: "She go to school every days.",
  },
  network: {
    trust,
    proxy: {
      protocol: proxyURL.protocol,
      hostname: proxyURL.hostname,
      port: proxyURL.port || 80,
    },
    bypass,
  },
  extensions: [item],
  primaryOperationPassRate: null,
  output,
};
const browser = new ChromePipe(trust.wrapper, output, [
  "--proxy-server=" + proxyServer,
  ...(bypass ? ["--proxy-bypass-list=" + bypass] : []),
  "--window-size=1600,1200",
], { headed: true });
browser.events = [];
/** @param {number} ms */
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const snapshot =
  `(()=>{function all(root){return[...root.querySelectorAll('*')].flatMap(e=>[e,...(e.shadowRoot?all(e.shadowRoot):[])])}const elements=all(document);return{url:location.href,ready:document.readyState,text:elements.filter(e=>!e.children.length&&!['STYLE','SCRIPT','TITLE','META','LINK'].includes(e.tagName)&&e.getClientRects().length).map(e=>e.innerText||'').filter(Boolean).join('\\n').slice(0,10000),writing:document.getElementById('writing')?.value,productNodes:elements.filter(e=>e.tagName.toLowerCase().includes('grammarly')||e.getAttribute('data-grammarly-shadow-root')!==null).map(e=>({tag:e.tagName,rect:e.getBoundingClientRect().toJSON()})),buttons:elements.filter(e=>e.tagName==='BUTTON'||e.getAttribute('role')==='button').map(e=>({text:e.textContent.trim().slice(0,100),aria:e.getAttribute('aria-label'),title:e.title,rect:e.getBoundingClientRect().toJSON()}))}})()`;
/** @type {Set<string>} */
const networkSessions = new Set();
/** @param {{targetId: string}} target */
async function observe(target) {
  const { sessionId } = await browser.call("Target.attachToTarget", {
    targetId: target.targetId,
    flatten: true,
  });
  await browser.call("Runtime.enable", {}, sessionId);
  await browser.call("Network.enable", {}, sessionId);
  networkSessions.add(sessionId);
  return sessionId;
}
try {
  report.version = await browser.call("Browser.getVersion");
  const directory = join(output, "extension");
  cpSync(
    join(root, "_dist/chrome-extension-corpus/chrome-unpacked", id),
    directory,
    {
      recursive: true,
      filter: (path) => !path.split("/").includes("_metadata"),
    },
  );
  Object.assign(item, verifyCorpusScripts(root, record, directory));
  const loaded = await browser.call("Extensions.loadUnpacked", {
    path: directory,
  });
  if (loaded.id !== id) throw Error("Original Chrome identity differs");
  for (
    const target of (await browser.call("Target.getTargets")).targetInfos
      .filter((target) =>
        target.type === "service_worker" &&
        target.url.startsWith(`chrome-extension://${id}/`)
      )
  ) await observe(target);
  const target = await browser.call("Target.createTarget", { url });
  const appSession = await observe(target);
  await waitForNativeUI(
    () =>
      evaluateNativeUI(
        browser,
        appSession,
        "Boolean(document.getElementById('writing'))",
      ),
    "real authored writing field",
  );
  await browser.call("Target.activateTarget", { targetId: target.targetId });
  await browser.call("Page.bringToFront", {}, appSession);
  /** @type {{x: number; y: number}} */
  const point = await evaluateNativeUI(
    browser,
    appSession,
    "(()=>{const r=document.getElementById('writing').getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2}})()",
  );
  for (
    const [type, buttons] of [["mouseMoved", 0], ["mousePressed", 1], [
      "mouseReleased",
      0,
    ]]
  ) {
    await browser.call("Input.dispatchMouseEvent", {
      type,
      x: point.x,
      y: point.y,
      button: type === "mouseMoved" ? "none" : "left",
      buttons,
      clickCount: 1,
    }, appSession);
  }
  await browser.call(
    "Input.insertText",
    { text: report.fixture.authoredText },
    appSession,
  );
  /** @type {import("./grammarly-test-types.ts").GrammarlySnapshot} */
  const afterNativeInput = await evaluateNativeUI(
    browser,
    appSession,
    snapshot,
  );
  item.afterNativeInput = afterNativeInput;
  if (afterNativeInput.writing !== report.fixture.authoredText) {
    throw Error("Native authored text input failed");
  }
  await pause(4000);
  /** @type {import("./grammarly-test-types.ts").GrammarlySnapshot} */
  const afterProductInitialization = await evaluateNativeUI(
    browser,
    appSession,
    snapshot,
  );
  item.afterProductInitialization = afterProductInitialization;
  for (
    const worker of (await browser.call("Target.getTargets")).targetInfos
      .filter((target) =>
        target.type === "service_worker" &&
        target.url.startsWith(`chrome-extension://${id}/`)
      )
  ) {
    item.nativeWorker = worker;
    await observe(worker);
  }
  await browser.call("Target.activateTarget", { targetId: target.targetId });
  await browser.call("Page.bringToFront", {}, appSession);
  for (
    const [type, buttons] of [["mouseMoved", 0], ["mousePressed", 1], [
      "mouseReleased",
      0,
    ]]
  ) {
    await browser.call("Input.dispatchMouseEvent", {
      type,
      x: point.x,
      y: point.y,
      button: type === "mouseMoved" ? "none" : "left",
      buttons,
      clickCount: 1,
    }, appSession);
  }
  await browser.call("Input.insertText", {
    text: " This are a second authored sentence.",
  }, appSession);
  await pause(8000);
  item.afterCheckRequest = await evaluateNativeUI(
    browser,
    appSession,
    snapshot,
  );
  writeFileSync(
    join(output, "writing.png"),
    Buffer.from(
      (await browser.call("Page.captureScreenshot", {}, appSession)).data,
      "base64",
    ),
  );
  item.contentControls = await evaluateNativeUI(
    browser,
    appSession,
    `(()=>{function all(root){return [...root.querySelectorAll('*')].flatMap(e=>[e,...(e.shadowRoot?all(e.shadowRoot):[])])}return all(document).filter(e=>e.getBoundingClientRect().width&&((e.getAttribute('class')||'').includes('status')||e.getAttribute('role')||e.getAttribute('aria-label'))).map(e=>({tag:e.tagName,role:e.getAttribute('role'),aria:e.getAttribute('aria-label'),title:e.getAttribute('title'),className:e.getAttribute('class'),text:e.textContent.trim().slice(0,100),rect:e.getBoundingClientRect().toJSON()}))})()`,
  );
  // Read genuine text geometry and hover the actual product-marked word.
  // No product suggestion handlers or application state are invoked directly.
  /** @type {import("./grammarly-test-types.ts").GrammarlyPoint} */
  const hoverPoint = await evaluateNativeUI(
    browser,
    appSession,
    `(()=>{const e=document.getElementById('writing'),r=e.getBoundingClientRect(),s=getComputedStyle(e),c=document.createElement('canvas').getContext('2d');if(!c)throw Error('Actual text metrics unavailable');c.font=s.font;return{x:r.x+parseFloat(s.borderLeftWidth)+parseFloat(s.paddingLeft)+c.measureText('She ').width+c.measureText('go').width/2,y:r.y+parseFloat(s.borderTopWidth)+parseFloat(s.paddingTop)+parseFloat(s.fontSize)/2}})()`,
  );
  await browser.call("Target.activateTarget", { targetId: target.targetId });
  await browser.call("Page.bringToFront", {}, appSession);
  await browser.call("Input.dispatchMouseEvent", {
    type: "mouseMoved",
    ...hoverPoint,
    buttons: 0,
  }, appSession);
  await pause(1200);
  item.nativeSuggestionAction = {
    kind: "native-hover",
    word: "go",
    point: hoverPoint,
  };
  /** @type {import("./grammarly-test-types.ts").GrammarlySnapshot} */
  const inlineSuggestion = await evaluateNativeUI(
    browser,
    appSession,
    snapshot,
  );
  item.inlineSuggestion = inlineSuggestion;
  writeFileSync(
    join(output, "inline-suggestion.png"),
    Buffer.from(
      (await browser.call("Page.captureScreenshot", {}, appSession)).data,
      "base64",
    ),
  );
  await browser.call("Target.activateTarget", { targetId: target.targetId });
  const nativeTab =
    (await browser.call("Target.getTargets", { filter: [{ type: "tab" }] }))
      .targetInfos.find((tab) => tab.url === url);
  if (!nativeTab) throw Error("Native authored tab is unavailable");
  await browser.call("Extensions.triggerAction", {
    id,
    targetId: nativeTab.targetId,
  });
  const popup = await waitForNativeUI(
    async () =>
      (await browser.call("Target.getTargets")).targetInfos.find((target) =>
        target.type === "page" &&
        target.url === `chrome-extension://${id}/src/popup.html`
      ),
    "actual native Grammarly popup",
  );
  item.nativePopup = popup;
  const popupSession = await observe(popup);
  await waitForNativeUI(
    async () => {
      /** @type {import("./grammarly-test-types.ts").GrammarlySnapshot} */
      const current = await evaluateNativeUI(browser, popupSession, snapshot);
      item.popup = current;
      return current.ready === "complete" && current.text.trim();
    },
    "real Grammarly popup rendering",
    15000,
  );
  await pause(1500);
  /** @type {import("./grammarly-test-types.ts").GrammarlySnapshot} */
  const popupUI = await evaluateNativeUI(browser, popupSession, snapshot);
  item.popup = popupUI;
  const text = popupUI.text + "\n" + afterProductInitialization.text;
  item.blockers = [];
  if (/sign in|log in|create (?:an? )?account|sign up/i.test(text)) {
    item.observedAccountUI =
      "Real product offers sign-in for account features; mandatory account requirement for core writing correction is not established";
  }
  const inlineText = inlineSuggestion.text;
  if (
    /you[’']re signed out of Grammarly/i.test(inlineText) &&
    /won[’']t get any writing suggestions until you sign back in/i.test(
      inlineText,
    )
  ) {
    item.observedRequiredAccountGate = {
      text: inlineText,
      controls: inlineSuggestion.buttons.map((button) =>
        button.text || button.aria || ""
      ).filter(Boolean),
    };
    item.primaryOperationAttempted = false;
    item.observedPrerequisiteStatus =
      "mandatory-sign-in-for-inline-suggestions";
    item.observedAccountUI =
      "Actual inline suggestion action requires sign-in; the separate toolbar popup also advertises optional account features";
    item.blockers.push(
      "Actual inline suggestion card requires signing back in before correction controls are available",
    );
    item.result = item.primaryOperationResult = "not-attempted-account-gate";
    item.primaryOperationExplanation =
      "The genuine product displays an explicit sign-in requirement at the actual underlined-word suggestion action. No login was attempted and no correction/navigation primary pass is claimed.";
  } else {
    item.result = item.primaryOperationResult = "primary-not-completed";
    item.primaryOperationExplanation =
      "The real product has not applied a correction and repeated it after navigation. Popup/content initialization alone is not a primary-operation pass.";
  }
} catch (error) {
  item.result = item.primaryOperationResult = "failed";
  item.error = error instanceof Error
    ? error.stack || String(error)
    : String(error);
} finally {
  /** @param {string} value */
  const safeURL = (value) => {
    try {
      const url = new URL(value);
      return url.origin === "null"
        ? url.protocol + "//" + url.host + url.pathname
        : url.origin + url.pathname;
    } catch {
      return "[non-URL]";
    }
  };
  /** @template {keyof import("devtools-protocol/types/protocol-mapping.d.ts").ProtocolMapping.Events} Method
   * @param {import("./cdp-types.ts").CDPEvent} event @param {Method} method
   * @returns {event is import("./cdp-types.ts").CDPEvent & import("./grammarly-test-types.ts").NativeEvent<Method>} */
  const isNativeEvent = (event, method) =>
    event.method === method && !!event.params;
  report.nativeResponses = browser.events.filter((event) =>
    isNativeEvent(event, "Network.responseReceived")
  ).filter((event) => !!event.sessionId && networkSessions.has(event.sessionId))
    .map((event) => {
      const response = event.params.response;
      return {
        url: safeURL(response.url),
        status: response.status,
        mimeType: response.mimeType,
      };
    });
  /** @param {import("./cdp-types.ts").CDPEvent} event
   * @returns {event is import("./cdp-types.ts").CDPEvent & import("./grammarly-test-types.ts").NativeWebSocketEvent} */
  const isWebSocketEvent = (event) =>
    event.method.startsWith("Network.webSocket") && !!event.params;
  report.nativeWebSockets = browser.events.filter(isWebSocketEvent).map(
    (event) => ({
      method: event.method,
      url: event.method === "Network.webSocketCreated"
        ? safeURL(event.params.url)
        : undefined,
      status: event.method === "Network.webSocketHandshakeResponseReceived"
        ? event.params.response.status
        : undefined,
      errorMessage: event.method === "Network.webSocketFrameError"
        ? event.params.errorMessage
        : undefined,
    }),
  );
  report.nativeRequestFailures = browser.events.filter((event) =>
    isNativeEvent(event, "Network.loadingFailed")
  ).filter((event) => !!event.sessionId && networkSessions.has(event.sessionId))
    .map((event) => ({
      errorText: event.params.errorText,
      type: event.params.type,
    }));
  if (report.nativeResponses.some((response) => response.status === 503)) {
    (item.blockers ||= []).push(
      "Actual native product HTTPS response503 from the current network environment",
    );
  }
  await browser.close();
  await new Promise((resolve, reject) =>
    server.close((error) => error ? reject(error) : resolve(undefined))
  );
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
  console.log("REPORT " + join(output, "report.json"));
}
if (item.primaryOperationResult !== "passed") process.exitCode = 1;
