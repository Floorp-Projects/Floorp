// SPDX-License-Identifier: MPL-2.0
/** @param {Document|null} [document] @returns {import("./sidepanel-test-types.ts").SidePanelSnapshot} */
export function snapshotUI(document = globalThis.document) {
  if (!document) throw Error("Snapshot document closed");
  /** @type {import("./sidepanel-test-types.ts").ProbeControl[]} */
  const all = [];
  /** @type {ShadowRoot[]} */
  const roots = [];
  /** @param {Document|ShadowRoot} root */
  const walk = (root) => {
    for (const element of root.querySelectorAll("*")) {
      all.push(
        /** @type {import("./sidepanel-test-types.ts").ProbeControl} */ (element),
      );
      if (element.shadowRoot) {
        roots.push(element.shadowRoot);
        walk(element.shadowRoot);
      }
    }
  };
  walk(document);
  const view = document.defaultView;
  if (!view) throw Error("Snapshot document has no live window");
  /** @param {Element} element */
  const visible = (element) => {
    const rect = element.getBoundingClientRect();
    const style = view.getComputedStyle(element);
    return (
      style && rect.width > 0 &&
      rect.height > 0 &&
      style.getPropertyValue("visibility") !== "hidden" &&
      style.getPropertyValue("display") !== "none"
    );
  };
  return {
    url: document.documentURI,
    title: document.title,
    ready: document.readyState,
    shadowText: roots
      .map((root) =>
        [...root.children]
          .filter((element) => !["STYLE", "SCRIPT"].includes(element.tagName))
          .map((element) =>
            /** @type {HTMLElement} */ (element).innerText ||
              element.textContent
          )
          .join("\n")
      )
      .join("\n")
      .slice(0, 18000),
    text: [
      document.body?.innerText,
      ...roots.map((root) =>
        [...root.children]
          .filter((e) => !["STYLE", "SCRIPT"].includes(e.tagName))
          .map((e) => /** @type {HTMLElement} */ (e).innerText || e.textContent)
          .join("\n")
      ),
    ]
      .filter(Boolean)
      .join("\n")
      .slice(0, 18000),
    controls: all
      .filter(
        (element) =>
          visible(element) &&
          element.matches(
            'button,a,input,textarea,select,[role="button"],[contenteditable="true"]',
          ),
      )
      .slice(0, 140)
      .map((element) => ({
        tag: element.tagName,
        id: element.id,
        type: element.type,
        editable: element.isContentEditable,
        role: element.getAttribute("role"),
        label: [
          element.innerText,
          element.getAttribute("aria-label"),
          element.getAttribute("title"),
          element.getAttribute("placeholder"),
          element.getAttribute("data-tooltip-content"),
        ]
          .filter(Boolean)
          .join(" | ")
          .slice(0, 220),
        value: element.value?.slice(0, 200),
        html: String(element.outerHTML).slice(0, 2500),
      })),
  };
}
