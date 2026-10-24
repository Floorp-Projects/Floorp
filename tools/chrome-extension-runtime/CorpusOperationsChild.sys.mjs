// SPDX-License-Identifier: MPL-2.0
// Private test actor: only authored fixtures and isolated corpus profiles register it.
import { snapshotUI } from "./corpus-sidepanel-ui.sys.mjs";
export class CorpusOperationsChild extends JSWindowActorChild {
  /** @param {{name: string, data?: import("./browser-tools-test-types.ts").OperationRequest}} message */
  receiveMessage({ name, data = {} }) {
    const document = this.document;
    const window = this.contentWindow;
    if (!document || !window) {
      throw Error("Corpus document is no longer available");
    }
    if (name === "Snapshot") {
      return {
        ...snapshotUI(document),
        probe: document.documentElement?.getAttribute(
          "data-authored-lifecycle-probe",
        ) ?? undefined,
        cookie: document.cookie,
      };
    }
    if (name === "PressKey") {
      if (!data.key || !["Enter", "Tab"].includes(data.key)) {
        throw Error("Unsupported native corpus key");
      }
      const before = document.activeElement;
      const input = Cc["@mozilla.org/text-input-processor;1"].createInstance(
        Ci.nsITextInputProcessor,
      );
      if (
        !input.beginInputTransactionForTests(
          /** @type {mozIDOMWindow} */ (/** @type {unknown} */ (window)),
        )
      ) throw Error("Native corpus key transaction unavailable");
      const event = new window.KeyboardEvent("", {
        key: data.key,
        code: data.key,
        keyCode: data.key === "Enter" ? 13 : 9,
      });
      input.keydown(event);
      input.keyup(event);
      return {
        pressed: data.key,
        focusBefore: { tag: before?.localName, id: before?.id },
        focusAfter: {
          tag: document.activeElement?.localName,
          id: document.activeElement?.id,
          text: document.activeElement?.textContent?.trim().slice(0, 180),
        },
      };
    }
    if (!data.selector) throw Error("Corpus control selector missing");
    /** @type {(Document | ShadowRoot)[]} */
    const roots = [document];
    /** @type {HTMLElement | undefined} */
    let element;
    for (let index = 0; index < roots.length && !element; index++) {
      const root = roots[index];
      element = [
        .../** @type {NodeListOf<HTMLElement>} */ (root.querySelectorAll(
          data.selector,
        )),
      ].reverse().find(
        (candidate) => {
          const rect = candidate.getBoundingClientRect();
          return ((name !== "Click" && name !== "InspectHit") || !data.text ||
            (data.exact
              ? candidate.textContent.trim() === data.text
              : candidate.textContent.includes(data.text))) &&
            rect.width && rect.height;
        },
      );
      for (const candidate of root.querySelectorAll("*")) {
        if (candidate.shadowRoot) roots.push(candidate.shadowRoot);
      }
    }
    if (!element) throw Error("Visible corpus operation control missing");
    element.scrollIntoView({ block: "center", behavior: "instant" });
    if (name === "Click" || name === "InspectHit") {
      const rect = element.getBoundingClientRect();
      const x = rect.x + rect.width / 2, y = rect.y + rect.height / 2;
      if (name === "InspectHit") {
        const hit = document.elementFromPoint(x, y);
        return {
          selected: {
            tag: element.localName,
            class: element.className,
            tabIndex: element.tabIndex,
            html: String(element.outerHTML).slice(0, 1600),
          },
          hit: { tag: hit?.localName, class: hit?.className },
          unobstructed: hit === element || element.contains(hit),
          x,
          y,
        };
      }
      /** @type {import("./browser-tools-test-types.ts").NativeClickObservation["events"]} */
      const events = [];
      /** @param {Event} event */
      const observe = (event) => {
        const target = /** @type {Element | null} */ (event.target);
        events.push({
          type: event.type,
          trusted: event.isTrusted,
          target: target?.localName,
          id: target?.id,
        });
      };
      for (const type of ["mousedown", "mouseup", "click"]) {
        document.addEventListener(type, observe, true);
      }
      window.synthesizeMouseEvent("mousedown", x, y, {
        button: 0,
        clickCount: 1,
      }, { toWindow: true });
      window.synthesizeMouseEvent("mouseup", x, y, {
        button: 0,
        clickCount: 1,
      }, { toWindow: true });
      for (const type of ["mousedown", "mouseup", "click"]) {
        document.removeEventListener(type, observe, true);
      }
      return {
        selected: {
          tag: element.localName,
          id: element.id,
          class: element.className,
          pointerEvents: window.getComputedStyle(element)?.getPropertyValue(
            "pointer-events",
          ),
        },
        hit: {
          tag: document.elementFromPoint(x, y)?.localName,
          class: document.elementFromPoint(x, y)?.className,
        },
        clicked: true,
        events,
        x,
        y,
        viewport: { width: window.innerWidth, height: window.innerHeight },
      };
    }
    if (name === "Type") {
      if (
        !(element instanceof window.HTMLInputElement ||
          element instanceof window.HTMLTextAreaElement)
      ) throw Error("Native text control required");
      const control =
        /** @type {HTMLInputElement | HTMLTextAreaElement} */ (element);
      control.focus();
      if (data.replace) {
        if (typeof control.select !== "function") {
          throw Error("Native replacement requires a selectable text control");
        }
        control.select();
      }
      const input = Cc["@mozilla.org/text-input-processor;1"].createInstance(
        Ci.nsITextInputProcessor,
      );
      if (
        !input.beginInputTransactionForTests(
          /** @type {mozIDOMWindow} */ (/** @type {unknown} */ (window)),
        )
      ) {
        throw Error("Native corpus text input transaction unavailable");
      }
      if (data.text === undefined) {
        throw Error("Native corpus input text missing");
      }
      input.commitCompositionWith(data.text);
      return { value: control.value };
    }
    throw Error("Unknown corpus operation");
  }
}
