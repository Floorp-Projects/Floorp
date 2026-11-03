// SPDX-License-Identifier: MPL-2.0
// Product-specific test operator. Vendor code and storage are never written here.
import { createHash, randomUUID } from "node:crypto";
import { writeFileSync } from "node:fs";
import { join } from "node:path";

const uiVisionId = "gcbalfbdmfieckjlnblleoemohcganoc";
/** @param {number} ms */
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

/**
 * This function is serialized into the actual extension document. Its only DOM
 * change is scrolling the genuine control into view before native pointer input.
 * @param {import("./uivision-settings-types.ts").UIVisionSettingsControl} control
 * @param {string} expectedURL
 * @param {boolean} [scroll]
 * @returns {import("./uivision-settings-types.ts").UIVisionSettingsControlState}
 */
function controlState(control, expectedURL, scroll = true) {
  const page = document;
  if (!page) throw Error("UI.Vision product document missing");
  if (location.href !== expectedURL) {
    throw Error("UI.Vision operator left the expected product document");
  }
  /** @type {HTMLElement|null} */
  let element = null;
  /** @type {HTMLInputElement|null} */
  let checkbox = null;
  let title = null;
  if (control === "dataTab" || control === "logsTab") {
    const tabs = [
      ...page.querySelectorAll(
        control === "dataTab"
          ? '.sidepanel .sidebar-inner > .ant-tabs > .ant-tabs-nav [role="tab"]'
          : '.sidepanel .data-tab > .ant-tabs > .ant-tabs-nav [role="tab"]',
      ),
    ].filter((tab) =>
      tab.textContent?.trim() === (control === "dataTab" ? "Data" : "Logs") &&
      (control !== "dataTab" || !tab.closest(".data-tab"))
    );
    if (tabs.length > 1) {
      throw Error("Ambiguous genuine UI.Vision log navigation tab");
    }
    element = /** @type {HTMLElement|undefined} */ (tabs[0]) || null;
  } else if (control === "welcome") {
    const dialog = page.querySelector(".macro-setup-dialog");
    title = dialog?.querySelector(".ant-modal-title")?.textContent?.trim() ||
      null;
    if (dialog && dialog.getBoundingClientRect().width) {
      if (title !== "Your macros are all still here") {
        throw Error("Unknown UI.Vision welcome dialog");
      }
      element = dialog.querySelector(
        ".ant-modal-footer button.ant-btn-primary",
      );
      if (element?.textContent?.trim() !== "OK") {
        throw Error("Unknown UI.Vision welcome action");
      }
    }
  } else if (control === "websiteRuns") {
    const labels = [...page.querySelectorAll(".api-pane label")].filter(
      (label) =>
        label.textContent?.replace("Edit Whitelist", "").replace(/\s+/g, " ")
          .trim() === "Run embedded macros from public websites",
    );
    if (labels.length > 1) throw Error("Ambiguous UI.Vision website setting");
    checkbox = labels[0]?.querySelector('input[type="checkbox"]') || null;
    element = labels[0]?.querySelector(".ant-checkbox") || checkbox;
  } else if (control === "editWhitelist") {
    const anchors = [...page.querySelectorAll(".api-pane a")].filter(
      (anchor) => anchor.textContent?.trim() === "Edit Whitelist",
    );
    if (anchors.length > 1) throw Error("Ambiguous UI.Vision whitelist action");
    element = /** @type {HTMLElement|undefined} */ (anchors[0]) || null;
  } else {
    const dialog = page.querySelector(".whitelist-modal");
    title = dialog?.querySelector(".ant-modal-title")?.textContent?.trim() ||
      null;
    if (dialog && dialog.getBoundingClientRect().width) {
      if (title !== "Embedded Macros Website Whitelist") {
        throw Error("Unknown UI.Vision whitelist dialog");
      }
      element = control === "whitelistText"
        ? dialog.querySelector(
          'textarea[placeholder="One URL prefix per line, e. g. https://ui.vision/rpa or ui.vision (covers http and https)"]',
        )
        : dialog.querySelector(".ant-modal-footer button.ant-btn-primary");
      if (
        control === "saveWhitelist" && element?.textContent?.trim() !== "Save"
      ) {
        throw Error("Unknown UI.Vision whitelist save action");
      }
    }
  }
  if (scroll) element?.scrollIntoView({ block: "center", inline: "center" });
  const rect = element?.getBoundingClientRect();
  const x = rect ? rect.x + rect.width / 2 : 0;
  const y = rect ? rect.y + rect.height / 2 : 0;
  const visible = !!element && !!rect?.width && !!rect.height &&
    getComputedStyle(element)?.getPropertyValue("visibility") === "visible" &&
    getComputedStyle(element)?.getPropertyValue("opacity") !== "0";
  const hit = visible ? page.elementFromPoint(x, y) : null;
  const enabled = !!element && !element.hasAttribute("disabled") &&
    element.getAttribute("aria-disabled") !== "true" && !checkbox?.disabled;
  const selected = element?.getAttribute("aria-selected") === "true";
  const paneID = element?.getAttribute("aria-controls");
  const pane = paneID ? page.getElementById(paneID) : null;
  const paneRect = pane?.getBoundingClientRect();
  const logViewVisible = control === "logsTab" && selected &&
    !!paneRect?.width && !!paneRect.height &&
    !!pane?.querySelector(".log-content,.no-data") &&
    getComputedStyle(pane)?.getPropertyValue("visibility") === "visible";
  return {
    control,
    url: location.href,
    ready: page.readyState,
    present: !!element,
    visible,
    enabled,
    unobstructed: !!element && (hit === element || element.contains(hit)),
    checked: checkbox?.checked ?? null,
    value: element instanceof HTMLTextAreaElement ? element.value : null,
    title,
    x,
    y,
    width: rect?.width || 0,
    height: rect?.height || 0,
    ...(control === "dataTab" || control === "logsTab"
      ? { selected, logViewVisible }
      : {}),
  };
}

/**
 * @template T
 * @param {import("./chrome-cdp-pipe.mjs").ChromePipe} browser
 * @param {string} session
 * @param {string} expression
 * @returns {Promise<T>}
 */
async function evaluate(browser, session, expression) {
  const result = await browser.call("Runtime.evaluate", {
    expression,
    returnByValue: true,
    awaitPromise: true,
  }, session);
  if (result.exceptionDetails) {
    throw Error(JSON.stringify(result.exceptionDetails));
  }
  return /** @type {T} */ (result.result.value);
}

/**
 * @template T
 * @param {() => Promise<T>} read
 * @param {(value: T) => boolean} ready
 * @param {string} description
 * @returns {Promise<T>}
 */
async function waitFor(read, ready, description) {
  let last;
  for (let attempt = 0; attempt < 50; attempt++) {
    last = await read();
    if (ready(last)) return last;
    await delay(200);
  }
  throw Error(`UI.Vision ${description} timed out: ${JSON.stringify(last)}`);
}

/**
 * @param {import("./chrome-cdp-pipe.mjs").ChromePipe} browser
 * @param {string} session
 * @param {import("./uivision-settings-types.ts").UIVisionSettingsControlState} state
 */
async function click(browser, session, state) {
  if (!state.visible || !state.enabled || !state.unobstructed) {
    throw Error(`UI.Vision control unavailable: ${JSON.stringify(state)}`);
  }
  // Runtime.evaluate returns only after this expression has synchronously
  // installed its listeners. Its Promise handle is an explicit registration
  // acknowledgement; no globals or cross-command execution ordering are needed.
  const registered = await browser.call("Runtime.evaluate", {
    expression: `(()=>{
    const current=(${controlState.toString()})(${
      JSON.stringify(state.control)
    },${JSON.stringify(state.url)},false);
    if(!current.visible||!current.enabled||!current.unobstructed||current.ready!=='complete')throw Error('Genuine UI.Vision control became unavailable before native input');
    const x=${state.x},y=${state.y};
    if(Math.abs(current.x-x)>0.1||Math.abs(current.y-y)>0.1||Math.abs(current.width-${state.width})>0.1||Math.abs(current.height-${state.height})>0.1)throw Error('Genuine UI.Vision control moved before native input');
    // current.unobstructed was computed from this exact finite control and point.
    const target=document.elementFromPoint(x,y);
    if(!target)throw Error('UI.Vision click witness lost its target');
    return new Promise(resolve=>{
      const events=[];
      const finish=()=>{clearTimeout(timer);for(const type of ['mousedown','mouseup','click'])document.removeEventListener(type,observe,true);resolve(events)};
      const observe=event=>{if((event.target===target||target.contains(event.target))&&Math.abs(event.clientX-x)<2&&Math.abs(event.clientY-y)<2){events.push({type:event.type,trusted:event.isTrusted});if(event.type==='click')finish()}};
      const timer=setTimeout(finish,2000);
      for(const type of ['mousedown','mouseup','click'])document.addEventListener(type,observe,true);
    });
  })()`,
    returnByValue: false,
    awaitPromise: false,
  }, session);
  if (registered.exceptionDetails) {
    throw Error(JSON.stringify(registered.exceptionDetails));
  }
  const promiseObjectId = registered.result.objectId;
  if (!promiseObjectId) {
    throw Error("UI.Vision click witness did not return its owned Promise");
  }
  try {
    await browser.call("Input.dispatchMouseEvent", {
      type: "mouseMoved",
      x: state.x,
      y: state.y,
    }, session);
    await browser.call("Input.dispatchMouseEvent", {
      type: "mousePressed",
      x: state.x,
      y: state.y,
      button: "left",
      buttons: 1,
      clickCount: 1,
    }, session);
    await browser.call("Input.dispatchMouseEvent", {
      type: "mouseReleased",
      x: state.x,
      y: state.y,
      button: "left",
      buttons: 0,
      clickCount: 1,
    }, session);
    const observed = await browser.call("Runtime.awaitPromise", {
      promiseObjectId,
      returnByValue: true,
    }, session);
    if (observed.exceptionDetails) {
      throw Error(JSON.stringify(observed.exceptionDetails));
    }
    const events =
      /** @type {import("./uivision-settings-types.ts").UIVisionInputEvent[]} */ (
        /** @type {unknown} */ (observed.result.value)
      );
    state.events = events;
    if (
      !["mousedown", "mouseup", "click"].every((type) =>
        events.some((event) => event.type === type && event.trusted)
      )
    ) {
      throw Error(
        `UI.Vision control did not receive trusted native pointer events: ${
          JSON.stringify(events)
        }`,
      );
    }
    return events;
  } finally {
    // The observer itself removes listeners on click or its 2-second deadline.
    await browser.call(
      "Runtime.releaseObject",
      { objectId: promiseObjectId },
      session,
    );
  }
}

/**
 * @param {import("./chrome-cdp-pipe.mjs").ChromePipe} browser
 * @param {string} session
 * @param {string} url
 * @param {import("./uivision-settings-types.ts").UIVisionSettingsControl} control
 */
async function availableControl(browser, session, url, control) {
  /** @type {import("./uivision-settings-types.ts").UIVisionSettingsControlState|undefined} */
  let previous;
  return await waitFor(
    () => /** @type {Promise<import("./uivision-settings-types.ts").UIVisionSettingsControlState>} */ (evaluate(
      browser,
      session,
      `(${controlState.toString()})(${JSON.stringify(control)},${
        JSON.stringify(url)
      })`,
    )),
    (value) => {
      const usable = value.ready === "complete" && value.visible &&
        value.enabled && value.unobstructed;
      const stable = usable && !!previous && previous.visible &&
        previous.enabled && previous.unobstructed &&
        Math.abs(value.x - previous.x) <= 0.1 &&
        Math.abs(value.y - previous.y) <= 0.1 &&
        Math.abs(value.width - previous.width) <= 0.1 &&
        Math.abs(value.height - previous.height) <= 0.1;
      previous = usable ? value : undefined;
      return stable;
    },
    `stable genuine ${control}`,
  );
}

/**
 * Preserve the actual live failure before the owned Settings tab closes. This
 * reads only the finite product controls, visible document and three policy
 * fields; it does not scroll, focus, click, or change vendor state.
 * @param {import("./chrome-cdp-pipe.mjs").ChromePipe} browser
 * @param {string|undefined} session
 * @param {string} settingsURL
 * @param {import("./uivision-settings-types.ts").UIVisionSettingsControlState[]} steps
 * @returns {Promise<import("./uivision-settings-types.ts").UIVisionSettingsFailure>}
 */
async function captureSettingsFailure(browser, session, settingsURL, steps) {
  /** @type {import("./uivision-settings-types.ts").UIVisionSettingsFailure} */
  const diagnostics = { settingsURL, steps };
  if (!session) {
    diagnostics.snapshotError = "Settings target session was not attached";
    diagnostics.screenshotError = "Settings target session was not attached";
    return diagnostics;
  }
  try {
    diagnostics.snapshot = await evaluate(
      browser,
      session,
      `(async()=>{
      if(location.href!==${
        JSON.stringify(settingsURL)
      })throw Error('Settings diagnostic document mismatch');
      const controls=['websiteRuns','editWhitelist','whitelistText','saveWhitelist'].map(control=>(${controlState.toString()})(control,${
        JSON.stringify(settingsURL)
      },false));
      const {config={}}=await chrome.storage.local.get('config');
      const active=document.activeElement;
      return {url:location.href,ready:document.readyState,title:document.title,text:(document.body?.innerText||'').slice(0,12000),activeElement:active?{tag:active.tagName,id:active.id,type:active.getAttribute('type'),placeholder:active.getAttribute('placeholder')}:null,controls,configuration:{allowRunFromHttpSchema:config.allowRunFromHttpSchema,websiteInvokePolicyVersion:config.websiteInvokePolicyVersion,websiteWhiteList:config.websiteWhiteList}};
    })()`,
    );
  } catch (error) {
    diagnostics.snapshotError = String(error).slice(0, 2000);
  }
  try {
    const screenshot = await browser.call("Page.captureScreenshot", {
      format: "png",
    }, session);
    if (screenshot.data.length > 16 * 1024 * 1024) {
      throw Error(
        "Settings diagnostic screenshot exceeds 16 MiB encoded bound",
      );
    }
    const bytes = Buffer.from(screenshot.data, "base64");
    const path = join(browser.output, "uivision-settings-failure.png");
    writeFileSync(path, bytes, { flag: "wx" });
    diagnostics.screenshot = {
      path,
      sha256: createHash("sha256").update(bytes).digest("hex"),
      bytes: bytes.length,
    };
  } catch (error) {
    diagnostics.screenshotError = String(error).slice(0, 2000);
  }
  return diagnostics;
}

/**
 * Operate the real UI.Vision action panel and Settings page. The allowlist is
 * exactly the owned fixture URL. No policy or onboarding storage is set directly.
 * @param {import("./chrome-cdp-pipe.mjs").ChromePipe} browser
 * @param {string} fixtureURL
 * @returns {Promise<import("./uivision-settings-types.ts").UIVisionTrustedSettingsResult>}
 */
export async function configureUIVisionThroughSettings(browser, fixtureURL) {
  const fixture = new URL(fixtureURL);
  if (
    fixture.protocol !== "http:" || fixture.hostname !== "127.0.0.1" ||
    !fixture.port || Number(fixture.port) < 1 ||
    fixture.pathname !== "/automation" ||
    fixture.search || fixture.hash || fixture.username || fixture.password ||
    fixture.href !== fixtureURL
  ) {
    throw Error(
      "UI.Vision Settings requires the exact authored loopback fixture",
    );
  }
  const tab = (await browser.call("Target.getTargets", {
    filter: [{ type: "tab" }],
  })).targetInfos.filter((target) => target.url === fixtureURL);
  if (tab.length !== 1) throw Error("Expected one actual authored browser tab");
  await browser.call("Target.activateTarget", { targetId: tab[0].targetId });
  await browser.call("Extensions.triggerAction", {
    id: uiVisionId,
    targetId: tab[0].targetId,
  });
  const panelURL = `chrome-extension://${uiVisionId}/sidepanel.html`;
  const panel = await waitFor(
    async () =>
      (await browser.call("Target.getTargets")).targetInfos.find((target) =>
        target.url === panelURL
      ),
    (target) => !!target,
    "real action panel",
  );
  if (!panel) throw Error("Actual UI.Vision action panel missing");
  const { sessionId: panelSession } = await browser.call(
    "Target.attachToTarget",
    {
      targetId: panel.targetId,
      flatten: true,
    },
  );
  /** @param {string} session @param {string} url @param {import("./uivision-settings-types.ts").UIVisionSettingsControl} control */
  const state = (session, url, control) =>
    evaluate(
      browser,
      session,
      `(${controlState.toString()})(${JSON.stringify(control)},${
        JSON.stringify(url)
      })`,
    );
  /** @param {string} session @param {string} url @param {import("./uivision-settings-types.ts").UIVisionSettingsControl} control */
  const available = (session, url, control) =>
    availableControl(browser, session, url, control);
  await waitFor(
    () =>
      evaluate(
        browser,
        panelSession,
        `location.href===${
          JSON.stringify(panelURL)
        }&&document.readyState==='complete'&&(document.body?.innerText?.length??0)>20`,
      ),
    (ready) => ready === true,
    "rendered action panel",
  );
  // Opening the panel lets the vendor perform its normal policy migration.
  const welcome =
    /** @type {import("./uivision-settings-types.ts").UIVisionSettingsControlState} */ (
      await state(panelSession, panelURL, "welcome")
    );
  /** @type {import("./uivision-settings-types.ts").UIVisionInputEvent[]} */
  let welcomeEvents = [];
  if (welcome.visible) {
    welcomeEvents = await click(
      browser,
      panelSession,
      await available(panelSession, panelURL, "welcome"),
    );
    await waitFor(
      () => /** @type {Promise<import("./uivision-settings-types.ts").UIVisionSettingsControlState>} */ (state(
        panelSession,
        panelURL,
        "welcome",
      )),
      (value) => !value.visible,
      "welcome dismissal",
    );
  }
  const settingsURL = `chrome-extension://${uiVisionId}/options.html#api`;
  const options = await browser.call("Target.createTarget", {
    url: settingsURL,
  });
  /** @type {import("./uivision-settings-types.ts").UIVisionSettingsControlState[]} */
  const steps = [];
  /** @type {string|undefined} */
  let optionsSession;
  try {
    const { sessionId } = await browser.call("Target.attachToTarget", {
      targetId: options.targetId,
      flatten: true,
    });
    optionsSession = sessionId;
    await browser.call("Target.activateTarget", { targetId: options.targetId });
    await browser.call("Page.bringToFront", {}, sessionId);
    await waitFor(
      () =>
        evaluate(
          browser,
          sessionId,
          `location.href===${
            JSON.stringify(settingsURL)
          }&&document.readyState==='complete'`,
        ),
      (ready) => ready === true,
      "API settings document",
    );
    const websiteRuns = await available(sessionId, settingsURL, "websiteRuns");
    /** @returns {Promise<import("./uivision-settings-types.ts").UIVisionWebsiteConfiguration>} */
    const read = () =>
      evaluate(
        browser,
        sessionId,
        `(async()=>{if(location.href!==${
          JSON.stringify(settingsURL)
        })throw Error('Unexpected settings document');const {config={}}=await chrome.storage.local.get('config');return {allowRunFromHttpSchema:config.allowRunFromHttpSchema,websiteInvokePolicyVersion:config.websiteInvokePolicyVersion,websiteWhiteList:config.websiteWhiteList}})()`,
      );
    const before = await waitFor(
      read,
      (config) => config.websiteInvokePolicyVersion === 1,
      "normal vendor policy migration",
    );
    steps.push(websiteRuns);
    if (websiteRuns.checked !== true) {
      await click(browser, sessionId, websiteRuns);
      await waitFor(
        read,
        (config) => config.allowRunFromHttpSchema === true,
        "website setting persistence",
      );
    }
    const edit = await available(sessionId, settingsURL, "editWhitelist");
    steps.push(edit);
    await click(browser, sessionId, edit);
    const textarea = await available(sessionId, settingsURL, "whitelistText");
    steps.push(textarea);
    await click(browser, sessionId, textarea);
    // Normal focused textarea replacement. The vendor's onChange owns its state.
    for (
      const event of [
        {
          type: "rawKeyDown",
          key: "Control",
          code: "ControlLeft",
          windowsVirtualKeyCode: 17,
          modifiers: 2,
        },
        {
          type: "rawKeyDown",
          key: "a",
          code: "KeyA",
          windowsVirtualKeyCode: 65,
          modifiers: 2,
        },
        {
          type: "keyUp",
          key: "a",
          code: "KeyA",
          windowsVirtualKeyCode: 65,
          modifiers: 2,
        },
        {
          type: "keyUp",
          key: "Control",
          code: "ControlLeft",
          windowsVirtualKeyCode: 17,
          modifiers: 0,
        },
      ]
    ) await browser.call("Input.dispatchKeyEvent", event, sessionId);
    await browser.call("Input.insertText", { text: fixtureURL }, sessionId);
    await waitFor(
      () => /** @type {Promise<import("./uivision-settings-types.ts").UIVisionSettingsControlState>} */ (state(
        sessionId,
        settingsURL,
        "whitelistText",
      )),
      (value) => value.value === fixtureURL,
      "authored whitelist textarea input",
    );
    const save = await available(sessionId, settingsURL, "saveWhitelist");
    steps.push(save);
    await click(browser, sessionId, save);
    await waitFor(
      () => /** @type {Promise<import("./uivision-settings-types.ts").UIVisionSettingsControlState>} */ (state(
        sessionId,
        settingsURL,
        "whitelistText",
      )),
      (value) => !value.visible,
      "whitelist dialog closure",
    );
    const after = await waitFor(
      read,
      (config) =>
        config.allowRunFromHttpSchema === true &&
        config.websiteInvokePolicyVersion === 1 &&
        config.websiteWhiteList?.length === 1 &&
        config.websiteWhiteList[0] === fixtureURL,
      "exact authored whitelist persistence",
    );
    return {
      panelSession,
      onboarding: {
        shown: welcome.visible,
        clicked: welcome.visible,
        title: welcome.title,
        events: welcomeEvents,
      },
      configuration: {
        method: "actual-product-settings-native-CDP-input",
        settingsURL,
        before,
        after,
        steps,
      },
    };
  } catch (error) {
    const diagnostics = await captureSettingsFailure(
      browser,
      optionsSession,
      settingsURL,
      steps,
    );
    throw Object.assign(error instanceof Error ? error : Error(String(error)), {
      settingsDiagnostics: diagnostics,
    });
  } finally {
    await browser.call("Target.closeTarget", { targetId: options.targetId });
    await browser.call("Target.activateTarget", { targetId: tab[0].targetId });
  }
}

/**
 * Observe the vendor's public acknowledgement and standard page errors around
 * the existing documented dispatch. These observations never count as primary
 * macro success and do not change the macro or bridge payload.
 * @param {import("./chrome-cdp-pipe.mjs").ChromePipe} browser
 * @param {string} session
 * @param {string} fixtureURL
 * @param {import("./automation-test-types.ts").VisionMacro} macro
 * @returns {Promise<import("./uivision-settings-types.ts").UIVisionInvocationObservation>}
 */
export async function invokeUIVisionWithWitness(
  browser,
  session,
  fixtureURL,
  macro,
) {
  const marker = `floorp-ui-vision-invocation-${randomUUID()}`;
  browser.events ??= [];
  await browser.call("Runtime.enable", {}, session);
  // The response acknowledges synchronous registration. The finite closure is
  // retained only by this owned remote object and its three-second timer.
  const registered = await browser.call("Runtime.evaluate", {
    expression: `(()=>{
      if(location.href!==${
      JSON.stringify(fixtureURL)
    })throw Error('Expected authored automation target');
      const marker=${JSON.stringify(marker)};
      const types=['kantuInvokeSuccess','error','unhandledrejection'];
      let seen=0,timer;
      const stop=()=>{clearTimeout(timer);for(const type of types)window.removeEventListener(type,observe)};
      const observe=event=>{
        if(seen>=20)return;
        seen++;
        let message;
        try{message=event.type==='error'?String(event.message||'').slice(0,1000):event.type==='unhandledrejection'?String(event.reason).slice(0,1000):undefined}catch{message='<unserializable error>'}
        console.info(marker,JSON.stringify({type:event.type,message}));
      };
      try{for(const type of types)window.addEventListener(type,observe);timer=setTimeout(stop,3000);return {stop}}
      catch(error){stop();throw error}
    })()`,
    awaitPromise: false,
    returnByValue: false,
  }, session);
  if (registered.exceptionDetails) {
    throw Error(JSON.stringify(registered.exceptionDetails));
  }
  const objectId = registered.result.objectId;
  if (!objectId) {
    throw Error(
      "UI.Vision invocation observer registration was not acknowledged",
    );
  }
  /** @type {import("./uivision-settings-types.ts").UIVisionInvocationObservation} */
  const observation = {
    dispatchedDocumentedBridge: false,
    witnessMs: 3000,
    transport: "Runtime.consoleAPICalled",
    events: [],
  };
  /** @param {unknown} error */
  const contextEnded = (error) =>
    /Inspected target navigated or closed|Execution context was destroyed|Cannot find context with specified id|Could not find object with given id/
      .test(String(error));
  try {
    try {
      // Return immediately: the real macro can navigate this document. The
      // unchanged primary loop observes its destination and engine completion.
      const dispatched = await evaluate(
        browser,
        session,
        `(()=>{
        if(location.href!==${
          JSON.stringify(fixtureURL)
        })throw Error('Expected authored automation target');
        window.dispatchEvent(new CustomEvent('kantuSaveAndRunMacro',{detail:${
          JSON.stringify({ json: macro, direct: true, storageMode: "browser" })
        }}));
        return {dispatchedDocumentedBridge:true};
      })()`,
      );
      if (
        !dispatched || typeof dispatched !== "object" ||
        !("dispatchedDocumentedBridge" in dispatched) ||
        dispatched.dispatchedDocumentedBridge !== true
      ) {
        throw Error(
          "UI.Vision documented bridge dispatch was not acknowledged",
        );
      }
      observation.dispatchedDocumentedBridge = true;
    } catch (error) {
      if (!contextEnded(error)) throw error;
      observation.dispatchError = String(error).slice(0, 1000);
      observation.witnessContextEnded = true;
    }
    // This wait belongs to the host, so expected target navigation cannot abort it.
    await delay(3000);
    for (const event of browser.events) {
      if (observation.events.length >= 20) break;
      if (
        event.method !== "Runtime.consoleAPICalled" ||
        event.sessionId !== session || event.params?.type !== "info"
      ) continue;
      const args = event.params.args;
      if (!Array.isArray(args) || args.length !== 2) continue;
      const [tag, payload] = /** @type {unknown[]} */ (args);
      if (
        !tag || typeof tag !== "object" || !("value" in tag) ||
        tag.value !== marker ||
        !payload || typeof payload !== "object" || !("value" in payload) ||
        typeof payload.value !== "string" || payload.value.length > 8000
      ) continue;
      let value;
      try {
        value = /** @type {unknown} */ (JSON.parse(payload.value));
      } catch {
        continue;
      }
      if (
        !value || typeof value !== "object" || !("type" in value) ||
        typeof value.type !== "string" ||
        !["kantuInvokeSuccess", "error", "unhandledrejection"].includes(
          value.type,
        )
      ) continue;
      const message = "message" in value ? value.message : undefined;
      if (
        message !== undefined &&
        (typeof message !== "string" || message.length > 1000)
      ) continue;
      observation.events.push({
        type: value.type,
        ...(typeof message === "string" ? { message } : {}),
      });
    }
    return observation;
  } finally {
    for (const method of ["stop", "release"]) {
      try {
        if (method === "stop") {
          const stopped = await browser.call("Runtime.callFunctionOn", {
            objectId,
            functionDeclaration: "function(){this.stop();return true}",
            returnByValue: true,
          }, session);
          if (stopped.exceptionDetails) {
            observation.cleanupErrors ??= [];
            observation.cleanupErrors.push(
              JSON.stringify(stopped.exceptionDetails).slice(0, 1000),
            );
          }
        } else {
          await browser.call(
            "Runtime.releaseObject",
            { objectId },
            session,
          );
        }
      } catch (error) {
        if (contextEnded(error)) observation.witnessContextEnded = true;
        else {
          observation.cleanupErrors ??= [];
          observation.cleanupErrors.push(String(error).slice(0, 1000));
        }
      }
    }
  }
}

/**
 * Navigate the actual Data > Logs tabs. This changes only the product's visible
 * view; it does not choose AI settings or manufacture macro completion.
 * @param {import("./chrome-cdp-pipe.mjs").ChromePipe} browser
 * @param {string} session
 */
export async function showUIVisionLogs(browser, session) {
  const panelURL = `chrome-extension://${uiVisionId}/sidepanel.html`;
  /** @type {import("./uivision-settings-types.ts").UIVisionSettingsControlState[]} */
  const steps = [];
  try {
    for (const control of /** @type {const} */ (["dataTab", "logsTab"])) {
      const state = await availableControl(browser, session, panelURL, control);
      steps.push(state);
      await click(browser, session, state);
    }
    const view = await waitFor(
      () =>
        evaluate(
          browser,
          session,
          `(${controlState.toString()})("logsTab",${
            JSON.stringify(panelURL)
          },false)`,
        ),
      (value) =>
        !!value && typeof value === "object" && "logViewVisible" in value &&
        value.logViewVisible === true,
      "genuine active Data > Logs view",
    );
    return { steps, view };
  } catch (error) {
    /** @type {import("./uivision-settings-types.ts").UIVisionLogViewFailure} */
    const diagnostics = { panelURL, steps };
    try {
      diagnostics.snapshot = await evaluate(
        browser,
        session,
        `(()=>{
        if(location.href!==${
          JSON.stringify(panelURL)
        })throw Error('UI.Vision log diagnostic document mismatch');
        return {url:location.href,ready:document.readyState,title:document.title,text:(document.body?.innerText||'').slice(0,12000),controls:['dataTab','logsTab'].map(control=>(${controlState.toString()})(control,${
          JSON.stringify(panelURL)
        },false))};
      })()`,
      );
    } catch (failure) {
      diagnostics.snapshotError = String(failure).slice(0, 2000);
    }
    try {
      const screenshot = await browser.call("Page.captureScreenshot", {
        format: "png",
      }, session);
      if (screenshot.data.length > 16 * 1024 * 1024) {
        throw Error("Panel diagnostic screenshot exceeds 16 MiB encoded bound");
      }
      const bytes = Buffer.from(screenshot.data, "base64");
      const path = join(browser.output, "uivision-log-view-failure.png");
      writeFileSync(path, bytes, { flag: "wx" });
      diagnostics.screenshot = {
        path,
        sha256: createHash("sha256").update(bytes).digest("hex"),
        bytes: bytes.length,
      };
    } catch (failure) {
      diagnostics.screenshotError = String(failure).slice(0, 2000);
    }
    throw Object.assign(error instanceof Error ? error : Error(String(error)), {
      panelDiagnostics: diagnostics,
    });
  }
}
