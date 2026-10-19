// SPDX-License-Identifier: MPL-2.0
import { snapshotUI } from "./corpus-sidepanel-ui.sys.mjs";
export class CorpusSidePanelChild extends JSWindowActorChild {
  /** @param {string} selector @returns {import("./sidepanel-test-types.ts").ProbeControl|null} */
  find(selector) {
    if (!this.document) throw Error("Private actor document closed");
    /** @type {(Document|ShadowRoot)[]} */
    const roots = [this.document];
    for (let i = 0; i < roots.length; i++) {
      const root = roots[i];
      for (const element of root.querySelectorAll(selector)) {
        const rect = element.getBoundingClientRect();
        if (rect.width && rect.height) {
          return /** @type {import("./sidepanel-test-types.ts").ProbeControl} */ (element);
        }
      }
      for (const element of root.querySelectorAll("*")) {
        if (element.shadowRoot) roots.push(element.shadowRoot);
      }
    }
    return null;
  }
  /** @param {import("./sidepanel-test-types.ts").ProbeMessage} message */
  receiveMessage(message) {
    const { name, data } = message;
    if (!this.document || !this.contentWindow) {
      throw Error("Private actor document closed");
    }
    if (name === "Snapshot") return snapshotUI(this.document);
    if (!data) throw Error("Private actor command payload missing");
    const element = this.find(data.selector || "button,a");
    if (!element) return { missing: true };
    if (name === "ReadDraft") {
      return { value: element.value ?? element.textContent };
    }
    element.scrollIntoView({ block: "center" });
    const window = this.contentWindow;
    if (name === "Click") {
      const rect = element.getBoundingClientRect();
      const x = rect.x + rect.width / 2, y = rect.y + rect.height / 2;
      window.synthesizeMouseEvent("mousedown", x, y, {
        button: 0,
        clickCount: 1,
      });
      window.synthesizeMouseEvent("mouseup", x, y, {
        button: 0,
        clickCount: 1,
      });
      return { clicked: true };
    }
    if (name === "TypeDraft") {
      element.focus();
      const input = Cc["@mozilla.org/text-input-processor;1"].createInstance(
        Ci.nsITextInputProcessor,
      );
      if (
        !input.beginInputTransactionForTests(
          /** @type {WindowProxy & mozIDOMWindow} */ (window),
        )
      ) {
        throw new Error("Native text input transaction unavailable");
      }
      if (typeof data.text !== "string") {
        throw Error("Native draft text missing");
      }
      input.commitCompositionWith(data.text);
      return { value: element.value ?? element.textContent };
    }
    if (name === "PressEnter") {
      element.focus();
      const input = Cc["@mozilla.org/text-input-processor;1"].createInstance(
        Ci.nsITextInputProcessor,
      );
      if (
        !input.beginInputTransactionForTests(
          /** @type {WindowProxy & mozIDOMWindow} */ (window),
        )
      ) {
        throw new Error("Native key input transaction unavailable");
      }
      const key = new window.KeyboardEvent("", {
        key: "Enter",
        code: "Enter",
        keyCode: 13,
      });
      input.keydown(key);
      input.keyup(key);
      return { pressed: true };
    }
    throw new Error(`Unknown sidepanel operation: ${name}`);
  }
}
