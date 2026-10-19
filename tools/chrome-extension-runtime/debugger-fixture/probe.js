// SPDX-License-Identifier: MPL-2.0
(async () => {
  const globals =
    /** @type {import("../debugger-reference-types.ts").DebuggerFixtureGlobals} */ (
      /** @type {unknown} */ (globalThis)
    );
  const { chrome, floorpDebuggerConfig } = globals;
  const document = globalThis.document;
  if (!document) throw Error("Debugger fixture document missing");
  /** @type {import("../debugger-reference-types.ts").DebuggerProbeReport} */
  const report = { passed: false, cases: {}, events: [] };
  /** @type {import("../debugger-reference-types.ts").DebuggerTab | undefined} */
  let tab;
  /** @param {string} name @param {unknown} value */
  const check = (name, value) => {
    report.cases[name] = !!value;
    if (!value) throw new Error(name);
  };
  /** @param {number} ms */
  const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  /** @template T @param {() => T | Promise<T>} read @returns {Promise<T>} */
  const until = async (read) => {
    const deadline = Date.now() + 10000;
    while (Date.now() < deadline) {
      const value = await read();
      if (value) return value;
      await delay(20);
    }
    throw new Error("Timed out waiting for native side effect");
  };
  try {
    tab = await chrome.tabs.create({
      url: floorpDebuggerConfig.url,
      active: true,
    });
    const targetTab = tab;
    await until(
      async () => (await chrome.tabs.get(targetTab.id)).status === "complete",
    );
    const source = { tabId: tab.id };
    /** @template {import("../cdp-types.ts").CDPMethod} M @param {M} method @param {Record<string, unknown>} [params] */
    const command = (method, params = {}) =>
      chrome.debugger.sendCommand(source, method, params);
    /** @template [T=unknown] @param {string} expression @param {Record<string, unknown>} [params] @returns {Promise<T>} */
    const evaluate = async (expression, params = {}) => {
      const value = await command("Runtime.evaluate", {
        expression,
        returnByValue: true,
        ...params,
      });
      return /** @type {T} */ (value.result.value);
    };
    chrome.debugger.onEvent.addListener((owner, method) => {
      if (owner.tabId === targetTab.id) report.events.push(method);
    });
    await chrome.debugger.attach(source, "1.3");
    check(
      "publicAttachment",
      (await chrome.debugger.getTargets()).some(
        (target) => target.tabId === targetTab.id && target.attached,
      ),
    );
    await command("Runtime.enable");
    await command("Page.enable");
    await command("DOM.enable");
    check("evaluate", (await evaluate("2+3")) === 5);
    check(
      "userGesture",
      (await evaluate("navigator.userActivation.isActive", {
        userGesture: true,
      })) === true,
    );
    const { result: object } = await command("Runtime.evaluate", {
      expression: "({count:2})",
    });
    if (!object.objectId) throw Error("Native remote object handle missing");
    const call = await command("Runtime.callFunctionOn", {
      objectId: object.objectId,
      functionDeclaration: "function(n){this.count+=n;return this.count}",
      arguments: [{ value: 3 }],
      returnByValue: true,
    });
    check("callFunction", call.result.value === 5);
    const properties = await command("Runtime.getProperties", {
      objectId: object.objectId,
      ownProperties: true,
    });
    check(
      "objectProperties",
      properties.result.some(
        (item) => item.name === "count" && item.value?.value === 5,
      ),
    );
    check(
      "awaitPromise",
      /** @type {{answer: number}} */ (
        await evaluate("Promise.resolve({answer:7})", { awaitPromise: true })
      ).answer === 7,
    );
    const { root } = await command("DOM.getDocument");
    const { nodeId } = await command("DOM.querySelector", {
      nodeId: root.nodeId,
      selector: "#item",
    });
    await command("DOM.setAttributeValue", {
      nodeId,
      name: "data-native",
      value: "changed",
    });
    check(
      "nativeDOMMutation",
      (await evaluate('document.querySelector("#item").dataset.native')) ===
        "changed",
    );
    const model = (await command("DOM.getBoxModel", { nodeId })).model;
    check("nativeGeometry", model.border[0] !== model.content[0]);
    const input = (
      await command("DOM.querySelector", {
        nodeId: root.nodeId,
        selector: "#editor",
      })
    ).nodeId;
    await command("Page.bringToFront");
    await command("DOM.focus", { nodeId: input });
    await command("Input.insertText", { text: " inserted" });
    check(
      "nativeTextInput",
      await until(async () => {
        const value = await evaluate('document.querySelector("#editor").value');
        return typeof value === "string" && value.includes("inserted");
      }),
    );
    const { nodeId: fileInput } = await command("DOM.querySelector", {
      nodeId: root.nodeId,
      selector: "#upload",
    });
    await command("DOM.setFileInputFiles", {
      nodeId: fileInput,
      files: [floorpDebuggerConfig.upload],
    });
    check(
      "nativeFileUpload",
      (await evaluate('document.querySelector("#upload").files[0].size')) ===
        floorpDebuggerConfig.uploadSize,
    );
    const prompt = command("Runtime.evaluate", {
      expression: 'globalThis.answer=prompt("native contract prompt","before")',
    });
    await until(() => report.events.includes("Page.javascriptDialogOpening"));
    await command("Page.handleJavaScriptDialog", {
      accept: true,
      promptText: "accepted",
    });
    await prompt;
    check("nativePrompt", (await evaluate("globalThis.answer")) === "accepted");
    await command("Network.enable");
    await command("Network.setUserAgentOverride", {
      userAgent: "Chrome-Floorp-Debugger-Contract",
    });
    /** @type {import("../debugger-reference-types.ts").DebuggerEcho} */
    const response = await evaluate(
      'fetch("/debugger-echo",{method:"POST",body:"native body"}).then(r=>r.json())',
      { awaitPromise: true },
    );
    check(
      "nativeUserAgent",
      response.userAgent === "Chrome-Floorp-Debugger-Contract",
    );
    check("nativePostBody", response.body === "native body");
    check(
      "nativeScreenshot",
      (
        await command("Page.captureScreenshot", { format: "png" })
      ).data.startsWith("iVBOR"),
    );
    check(
      "nativePageMetrics",
      (await command("Page.getLayoutMetrics")).cssLayoutViewport.clientWidth >
        0,
    );
    await chrome.tabs.setZoom(tab.id, 1.25);
    await delay(100);
    report.zoomMetrics = await command("Page.getLayoutMetrics");
    check(
      "nativePageZoomMetrics",
      report.zoomMetrics.cssVisualViewport.zoom === 1.25,
    );
    check(
      "nativeLegacyZoomMetrics",
      Math.abs(
        report.zoomMetrics.layoutViewport.clientWidth -
          report.zoomMetrics.cssLayoutViewport.clientWidth * 1.25,
      ) < 1,
    );
    await chrome.tabs.setZoom(tab.id, 1);
    await command("Runtime.releaseObject", { objectId: object.objectId });
    let released = false;
    try {
      await command("Runtime.getProperties", { objectId: object.objectId });
    } catch {
      released = true;
    }
    check("releasedObjectRejected", released);
    if (floorpDebuggerConfig.nativeOverrides) {
      await command("Emulation.setFocusEmulationEnabled", { enabled: true });
      const other = await chrome.tabs.create({
        url: "about:blank",
        active: true,
      });
      try {
        check(
          "nativeFocusEmulation",
          await until(
            async () =>
              await evaluate(
                "document.hasFocus() && document.visibilityState === 'visible'",
              ),
          ),
        );
        await command("Emulation.setFocusEmulationEnabled", { enabled: false });
        check(
          "nativeFocusReset",
          await until(
            async () =>
              await evaluate(
                "!document.hasFocus() && document.visibilityState === 'hidden'",
              ),
          ),
        );
      } finally {
        await chrome.tabs.remove(other.id);
      }
      /** @param {string} suffix @param {boolean} [attached] */
      const navigate = async (suffix, attached = true) => {
        await chrome.tabs.update(targetTab.id, {
          url: new URL(suffix, floorpDebuggerConfig.url).href,
        });
        await until(
          async () =>
            (await chrome.tabs.get(targetTab.id)).status === "complete",
        );
        if (attached) {
          await until(async () => {
            try {
              return await evaluate("document.readyState === 'complete'");
            } catch {
              return false;
            }
          });
        }
      };
      /** @param {string} marker */
      const inline = async (marker) => {
        await evaluate(
          `{const script=document.createElement('script');script.textContent=${JSON.stringify(
            `globalThis.cspMarker=${JSON.stringify(marker)}`,
          )};document.body.appendChild(script);}`,
        );
        return evaluate("globalThis.cspMarker");
      };
      await command("Page.setBypassCSP", { enabled: false });
      await navigate("/csp-target?blocked");
      check("nativeCSPEnforced", (await inline("blocked")) === undefined);
      await command("Page.setBypassCSP", { enabled: true });
      await navigate("/csp-target?bypass");
      check("nativeCSPBypass", (await inline("bypassed")) === "bypassed");
      await chrome.debugger.detach(source);
      await navigate("/csp-target?after-detach", false);
      await chrome.debugger.attach(source, "1.3");
      check(
        "nativeCSPRestoredOnDetach",
        (await inline("should-stay-blocked")) === undefined,
      );
    }
    await chrome.debugger.detach(source);
    let detached = false;
    try {
      await command("Runtime.evaluate", { expression: "1" });
    } catch {
      detached = true;
    }
    check("detachedCommandsRejected", detached);
    report.passed = true;
  } catch (error) {
    report.error = String(error);
    report.stack = error instanceof Error ? error.stack : undefined;
  } finally {
    if (tab) await chrome.tabs.remove(tab.id);
    globals.floorpReferenceReport = report;
    document.title = JSON.stringify(report);
  }
})();
