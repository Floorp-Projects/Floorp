// SPDX-License-Identifier: MPL-2.0
export function snapshotUI(document = globalThis.document) {
  const all = [];
  const roots = [];
  const walk = (root) => {
    for (const element of root.querySelectorAll("*")) {
      all.push(element);
      if (element.shadowRoot) {
        roots.push(element.shadowRoot);
        walk(element.shadowRoot);
      }
    }
  };
  walk(document);
  const visible = (element) => {
    const rect = element.getBoundingClientRect();
    const style = document.defaultView.getComputedStyle(element);
    return rect.width > 0 && rect.height > 0 && style.visibility !== "hidden" &&
      style.display !== "none";
  };
  return {
    url: document.documentURI,
    title: document.title,
    ready: document.readyState,
    text: [
      document.body?.innerText,
      ...roots.map((root) =>
        [...root.children].filter((e) =>
          !["STYLE", "SCRIPT"].includes(e.tagName)
        ).map((e) => e.innerText || e.textContent).join("\n")
      ),
    ]
      .filter(Boolean).join("\n").slice(0, 18000),
    controls: all.filter((element) =>
      visible(element) &&
      element.matches(
        'button,a,input,textarea,select,[role="button"],[contenteditable="true"]',
      )
    ).slice(0, 140).map((element) => ({
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
      ].filter(Boolean).join(" | ").slice(0, 220),
      value: element.value?.slice(0, 200),
      html: element.outerHTML.slice(0, 2500),
    })),
  };
}
