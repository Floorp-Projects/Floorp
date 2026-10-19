// SPDX-License-Identifier: MPL-2.0
(async () => {
  const report = { passed: false };
  const assert = (value, description) => {
    if (!value) throw new Error(description);
  };
  try {
    const id = chrome.runtime.id;
    const resourceURL = chrome.runtime.getURL("resource.txt");
    report.runtimeId = id;
    report.urlScheme = new URL(resourceURL).protocol;
    report.resource = (await (await fetch(resourceURL)).text()).trim();
    report.selfMessage = await chrome.runtime.sendMessage(id, {
      target: "background",
      value: "self-message",
    });
    const port = chrome.runtime.connect(id, { name: "comparison-background" });
    report.selfPort = await new Promise((resolve, reject) => {
      port.onMessage.addListener((value) => {
        resolve(value);
        port.disconnect();
      });
      port.onDisconnect.addListener(() =>
        reject(
          new Error(
            chrome.runtime.lastError?.message || port.error?.message ||
              "Port disconnected",
          ),
        )
      );
      port.postMessage({ value: "self-port" });
    });
    report.background = await chrome.runtime.sendMessage({
      target: "background-info",
    });
    report.contextsBefore = await chrome.runtime.getContexts({});
    const options = {
      url: "offscreen.html",
      reasons: ["DOM_PARSER"],
      justification: "Compare native offscreen contexts and messaging",
    };
    await chrome.offscreen.createDocument(options);
    report.offscreenExists = await chrome.offscreen.hasDocument();
    const [first] = await chrome.runtime.getContexts({
      contextTypes: ["OFFSCREEN_DOCUMENT"],
    });
    assert(first, "An actual offscreen context must exist");
    report.offscreenContext = first;
    report.contextIdFilterCount =
      (await chrome.runtime.getContexts({ contextIds: [first.contextId] }))
        .length;
    report.documentIdFilterCount =
      (await chrome.runtime.getContexts({ documentIds: [first.documentId] }))
        .length;
    report.offscreenMessage = await chrome.runtime.sendMessage({
      target: "offscreen",
      value: "offscreen-message",
    });
    await chrome.offscreen.closeDocument();
    report.offscreenExistsAfterClose = await chrome.offscreen.hasDocument();
    report.contextsAfterClose = (await chrome.runtime.getContexts({
      contextTypes: ["OFFSCREEN_DOCUMENT"],
    })).length;
    await chrome.offscreen.createDocument(options);
    const [second] = await chrome.runtime.getContexts({
      contextTypes: ["OFFSCREEN_DOCUMENT"],
    });
    report.replacementDocumentIdChanged =
      second.documentId !== first.documentId;
    await chrome.offscreen.closeDocument();
    assert(
      report.resource === "comparison resource loaded",
      "Native extension resource must load",
    );
    assert(
      report.selfMessage.senderId === id &&
        report.selfMessage.receiverId === id,
      "Self-message IDs agree",
    );
    assert(
      report.selfPort.senderId === id && report.selfPort.receiverId === id,
      "Self-Port IDs agree",
    );
    assert(
      report.offscreenMessage.senderId === id &&
        report.offscreenMessage.receiverId === id,
      "Offscreen message IDs agree",
    );
    assert(
      report.offscreenMessage.storage === "undefined" &&
        report.offscreenMessage.tabs === "undefined" &&
        report.offscreenMessage.getBackgroundPage === "undefined",
      "Offscreen APIs are restricted",
    );
    assert(
      report.contextIdFilterCount === 1 && report.documentIdFilterCount === 1,
      "Native context IDs filter correctly",
    );
    assert(
      report.offscreenExists && !report.offscreenExistsAfterClose &&
        report.contextsAfterClose === 0 &&
        report.replacementDocumentIdChanged,
      "Offscreen lifecycle tracks real documents",
    );
    report.passed = true;
  } catch (error) {
    report.error = String(error);
    report.stack = error.stack;
  } finally {
    globalThis.floorpReferenceReport = report;
    document.title = JSON.stringify(report);
  }
})();
