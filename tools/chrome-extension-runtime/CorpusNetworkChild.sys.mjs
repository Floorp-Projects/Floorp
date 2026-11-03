// SPDX-License-Identifier: MPL-2.0
// Test-only actor: drives the unchanged product's own controls and reads HTTP evidence.
export class CorpusNetworkChild extends JSWindowActorChild {
  /** @param {import("./network-test-types.ts").NetworkActorMessage} message */
  async receiveMessage({ name, data = {} }) {
    const nativeWindow = this.contentWindow;
    if (!nativeWindow || !this.document)
      throw Error("Native corpus document unavailable");
    const window =
      /** @type {import("./network-test-types.ts").NetworkContentWindow} */ (
        Cu.waiveXrays(nativeWindow)
      );
    const document = this.document;
    /** @type {(Document | ShadowRoot)[]} */
    const roots = [document];
    for (let index = 0; index < roots.length; ++index) {
      for (const element of roots[index].querySelectorAll("*")) {
        if (element.shadowRoot) roots.push(element.shadowRoot);
      }
    }
    /** @param {string} selector @returns {import("./network-test-types.ts").FormElement[]} */
    const query = (selector) =>
      roots.flatMap((root) =>
        [...root.querySelectorAll(selector)].map(
          (element) =>
            /** @type {import("./network-test-types.ts").FormElement} */ (
              element
            ),
        ),
      );
    if (name === "Snapshot") {
      return {
        url: document.documentURI,
        ready: document.readyState,
        text: roots
          .flatMap((root) =>
            [...root.children]
              .filter((element) => element.tagName !== "STYLE")
              .map(
                (element) =>
                  /** @type {HTMLElement} */ (element).innerText || "",
              ),
          )
          .join("\n")
          .slice(0, 16000),
        controls: query(
          "button,a,input,select,textarea,[data-level],[role=button],[data-home-control]",
        )
          .slice(0, 120)
          .map((element) => ({
            tag: element.tagName,
            id: element.id,
            name: element.name,
            value: element.value,
            text: element.textContent?.slice(0, 180),
            model: element.getAttribute("ng-model"),
            data: { ...element.dataset },
          })),
      };
    }
    if (name === "Exists") {
      if (!data.selector) throw Error("UI selector missing");
      return query(data.selector).length > 0;
    }
    if (name === "ProxySettings") return window.chrome.proxy.settings.get({});
    if (name === "AdState") return !!window.__corpusAdLoaded;
    if (name === "Fetch") {
      try {
        return await window
          .fetch(
            data.url ??
              (() => {
                throw Error("Fetch URL missing");
              })(),
            { signal: window.AbortSignal.timeout(4000) },
          )
          .then((response) => response.json());
      } catch (error) {
        return { failed: true, error: String(error) };
      }
    }
    if (!data.selector) throw Error("UI selector missing");
    let element = query(data.selector).find(
      (element) =>
        data.text === undefined || element.textContent?.trim() === data.text,
    );
    if (!element) {
      if (data.optional) return { missing: true };
      throw Error("Product control missing: " + data.selector);
    }
    if (name === "Click") {
      if (!element.getBoundingClientRect().width && element.labels) {
        const label = [...element.labels].find(
          (label) => label.getBoundingClientRect().width,
        );
        if (label) element = label;
      }
      if (!element)
        throw Error("Product control unavailable after label lookup");
      element.scrollIntoView({ block: "nearest" });
      const rect = element.getBoundingClientRect();
      if (!rect.width || !rect.height) {
        if (data.optional) return { missing: true };
        throw Error("Actual product control is hidden: " + data.selector);
      }
      // Reply before product handlers can close their real popup/actor.
      nativeWindow.setTimeout(() => {
        nativeWindow.synthesizeMouseEvent(
          "mousedown",
          rect.x + rect.width / 2,
          rect.y + rect.height / 2,
          { button: 0, buttons: 1, clickCount: 1 },
        );
        nativeWindow.synthesizeMouseEvent(
          "mouseup",
          rect.x + rect.width / 2,
          rect.y + rect.height / 2,
          { button: 0, buttons: 0, clickCount: 1 },
        );
      }, 20);
      return { clicked: true, backend: "native-mouse" };
    }
    if (name === "Fill") {
      element.focus();
      if (data.value === undefined) throw Error("UI value missing");
      element.value = data.value;
      for (const type of ["input", "change", "blur"])
        element.dispatchEvent(new window.Event(type, { bubbles: true }));
      return { value: element.value };
    }
    throw Error("Unknown network corpus UI operation: " + name);
  }
}
