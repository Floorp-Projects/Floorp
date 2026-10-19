// SPDX-License-Identifier: MPL-2.0
(() => {
  if (top !== window) return;
  const end = Date.now() + 10000;
  const read = (doc, name) => {
    const text = doc?.documentElement?.getAttribute(name);
    return text ? JSON.parse(text) : null;
  };
  const readDocument = (doc) => ({
    token: doc?.documentElement?.getAttribute("data-owned-contract-token"),
    page: read(doc, "data-owned-page-contract"),
    content: read(doc, "data-owned-content-contract"),
  });
  const timer = setInterval(() => {
    const frame = document.querySelector("iframe")?.contentDocument;
    const observation = {
      top: readDocument(document),
      frame: readDocument(frame),
    };
    const complete = Object.values(observation).every((value) =>
      value.page?.events?.length === 6 &&
      value.content?.payloads?.length === 3 &&
      value.content.payloads.every((payload) =>
        payload.snapshots.length === 2 && payload.sourceEvents.length === 2
      ) &&
      value.page.token === value.token && value.content.token === value.token
    );
    if (complete || Date.now() > end) {
      clearInterval(timer);
      const pre = document.createElement("pre");
      pre.id = "world-observation-json";
      pre.textContent = JSON.stringify({
        observationComplete: complete,
        compatibilityPass: null,
        observation,
      });
      document.body.appendChild(pre);
      document.title = complete
        ? "detail-contract-complete"
        : "detail-contract-incomplete";
    }
  }, 25);
  addEventListener("pagehide", () => clearInterval(timer), { once: true });
})();
