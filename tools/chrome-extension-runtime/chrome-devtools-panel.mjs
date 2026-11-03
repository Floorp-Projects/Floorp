// SPDX-License-Identifier: MPL-2.0
// Drive the actual native Chrome DevTools frontend and its registered extension frames.
/** @import {ChromeFrontend,ChromePanel,NativeControlPoint,NativeTabInfo,PanelSnapshot,Point,Truthy,NativeTargetInfo} from "./assessment-test-types.ts" */
/** @param {number} ms @returns {Promise<void>} */
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template T @param {()=>T|Promise<T>} read @param {string} label @param {number} [timeout] @returns {Promise<Truthy<T>>} */
export async function waitForNativeUI(read, label, timeout = 30000) {
  const end = Date.now() + timeout;
  while (Date.now() < end) {
    const value = await read();
    if (value) return /** @type {Truthy<T>} */ (value);
    await delay(100);
  }
  throw Error("Timed out waiting for " + label);
}
/** @template T @param {import("./chrome-cdp-pipe.mjs").ChromePipe} browser @param {string} sessionId @param {string} expression @returns {Promise<T>} */
export async function evaluateNativeUI(browser, sessionId, expression) {
  const result = await browser.call("Runtime.evaluate", {
    expression,
    returnByValue: true,
    awaitPromise: true,
  }, sessionId);
  if (result.exceptionDetails) {
    throw Error(JSON.stringify(result.exceptionDetails));
  }
  return /** @type {T} */ (result.result.value);
}
const deepFind =
  `function collect(root){const result=[];for(const e of root.querySelectorAll('*')){result.push(e);if(e.shadowRoot)result.push(...collect(e.shadowRoot));}return result;}`;
/** @param {import("./chrome-cdp-pipe.mjs").ChromePipe} browser @param {string} inspectedURL @returns {Promise<ChromeFrontend>} */
export async function attachNativeDevtools(browser, inspectedURL) {
  const inspected = new URL(inspectedURL);
  const host = inspected.host;
  const identity = host + inspected.pathname;
  const target = await waitForNativeUI(async () => {
    const targets =
      (await browser.call("Target.getTargets", { filter: [{}] })).targetInfos;
    return targets.find((target) =>
      target.type === "page" && target.url.startsWith("devtools://") &&
      target.title.includes(identity)
    );
  }, "real DevTools frontend for " + host);
  const { sessionId } = await browser.call("Target.attachToTarget", {
    targetId: target.targetId,
    flatten: true,
  });
  await browser.call("Runtime.enable", {}, sessionId);
  /** @template T @param {string} expression @returns {Promise<T>} */
  const evaluate = (expression) =>
    evaluateNativeUI(browser, sessionId, expression);
  /** @returns {Promise<NativeTabInfo[]>} */
  const tabs = () =>
    evaluate(
      `(()=>{${deepFind}return collect(document).filter(e=>e.getAttribute('role')==='tab').map(e=>({id:e.id,text:e.textContent.trim(),selected:e.getAttribute('aria-selected'),rect:e.getBoundingClientRect().toJSON()}))})()`,
    );
  /** @param {NativeControlPoint} point */
  async function clickPoint(point) {
    if (!point || point.width === 0 || point.height === 0) {
      throw Error("Native UI control is hidden");
    }
    await browser.call("Target.activateTarget", { targetId: target.targetId });
    await browser.call("Page.bringToFront", {}, sessionId);
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
      }, sessionId);
    }
  }
  /** @param {string} title */
  async function selectPanel(title) {
    /** @returns {Promise<NativeControlPoint|null>} */
    const pointForTitle = () =>
      evaluate(
        `(()=>{${deepFind}const e=collect(document).find(e=>e.getAttribute('role')==='tab'&&e.textContent.trim()===${
          JSON.stringify(title)
        });if(!e)return null;const r=e.getBoundingClientRect();return r.width&&r.height?{x:r.x+r.width/2,y:r.y+r.height/2,width:r.width,height:r.height}:null})()`,
      );
    let point = await pointForTitle();
    if (!point) {
      // Right-docked DevTools overflows extension tabs. Use the real native
      // dock-position shortcut, then select the actual visible registered tab.
      // This changes only this private frontend's UI, never extension state.
      const primary = await waitForNativeUI(
        async () =>
          (await tabs()).find((tab) =>
            tab.id === "tab-elements" && tab.rect.width
          ),
        "initialized native DevTools tab strip",
      );
      // Later frontends in this private profile retain bottom docking. Wait for
      // their asynchronously registered extension tab without toggling it back.
      if (primary.rect.x > 100 && primary.rect.y < 100) {
        await browser.call("Target.activateTarget", {
          targetId: target.targetId,
        });
        await browser.call("Page.bringToFront", {}, sessionId);
        for (const type of ["keyDown", "keyUp"]) {
          await browser.call("Input.dispatchKeyEvent", {
            type,
            key: "D",
            code: "KeyD",
            windowsVirtualKeyCode: 68,
            modifiers: 10,
          }, sessionId);
        }
      }
      point = await waitForNativeUI(
        pointForTitle,
        "registered native DevTools panel " + title,
      );
    }
    await clickPoint(point);
    await waitForNativeUI(
      async () =>
        (await tabs()).some((tab) =>
          tab.text === title && tab.selected === "true"
        ),
      "native selected panel " + title,
    );
    return tabs();
  }
  /** @param {string} extensionId @param {string} [path] @returns {Promise<ChromePanel>} */
  async function attachPanel(extensionId, path) {
    const frame = await waitForNativeUI(async () => {
      const targets = /** @type {NativeTargetInfo[]} */ (
        (await browser.call("Target.getTargets", { filter: [{}] })).targetInfos
      );
      return targets.find((frame) =>
        frame.type === "iframe" && frame.parentId === target.targetId &&
        frame.url.startsWith(`chrome-extension://${extensionId}/`) &&
        (!path || frame.url.includes(path))
      );
    }, "actual extension panel frame " + extensionId);
    const { sessionId: panelSession } = await browser.call(
      "Target.attachToTarget",
      { targetId: frame.targetId, flatten: true },
    );
    await browser.call("Runtime.enable", {}, panelSession);
    /** @template T @param {string} expression @returns {Promise<T>} */
    const panelEvaluate = (expression) =>
      evaluateNativeUI(browser, panelSession, expression);
    /** @param {string} expression */
    async function clickExpression(expression) {
      /** @type {Point} */
      const local = await panelEvaluate(
        `(()=>{${deepFind}const e=(${expression});if(!e)throw Error('Missing actual extension panel control');e.scrollIntoView({block:'center',inline:'center'});const r=e.getBoundingClientRect();if(!r.width||!r.height)throw Error('Actual extension panel control is hidden');return{x:r.x+r.width/2,y:r.y+r.height/2}})()`,
      );
      /** @type {NativeControlPoint} */
      const offset = await evaluate(
        `(()=>{${deepFind}const e=collect(document).find(e=>e.tagName==='IFRAME'&&e.src===${
          JSON.stringify(frame.url)
        });if(!e)throw Error('Actual panel iframe is absent');const r=e.getBoundingClientRect();return{x:r.x,y:r.y,width:r.width,height:r.height}})()`,
      );
      await clickPoint({
        x: local.x + offset.x,
        y: local.y + offset.y,
        width: offset.width,
        height: offset.height,
      });
    }
    return {
      target: frame,
      sessionId: panelSession,
      evaluate: panelEvaluate,
      clickExpression,
      snapshot: () =>
        panelEvaluate(
          `({url:location.href,text:document.body.innerText,ready:document.readyState})`,
        ),
    };
  }
  return {
    target,
    sessionId,
    evaluate,
    tabs,
    selectPanel,
    attachPanel,
    clickPoint,
  };
}
