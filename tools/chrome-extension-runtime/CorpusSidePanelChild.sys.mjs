// SPDX-License-Identifier: MPL-2.0
import { snapshotUI } from "./corpus-sidepanel-ui.sys.mjs";
export class CorpusSidePanelChild extends JSWindowActorChild {
  find(selector) {
    const roots = [this.document];
    for (let i = 0; i < roots.length; i++) {
      const root = roots[i];
      for (const element of root.querySelectorAll(selector)) {
        const rect = element.getBoundingClientRect();
        if (rect.width && rect.height) return element;
      }
      for (const element of root.querySelectorAll("*")) {
        if (element.shadowRoot) roots.push(element.shadowRoot);
      }
    }
    return null;
  }
  receiveMessage(message) {
    const { name, data } = message;
    if (name === "Snapshot") return snapshotUI(this.document);
    const element = this.find(data.selector);
    if (!element) return { missing: true };
    if (name === "ReadDraft") {
      return { value: element.value ?? element.textContent };
    }
    element.scrollIntoView({ block: "center" });
    const window = this.contentWindow;
    if (name === "Click") {
      const rect = element.getBoundingClientRect();
      const x = rect.x + rect.width / 2, y = rect.y + rect.height / 2;
      window.windowUtils.sendMouseEvent("mousedown", x, y, 0, 1, 0);
      window.windowUtils.sendMouseEvent("mouseup", x, y, 0, 1, 0);
      return { clicked: true };
    }
    if (name === "TypeDraft") {
      element.focus();
      const input = Cc["@mozilla.org/text-input-processor;1"].createInstance(
        Ci.nsITextInputProcessor,
      );
      if (!input.beginInputTransactionForTests(window)) {
        throw new Error("Native text input transaction unavailable");
      }
      input.commitCompositionWith(data.text);
      return { value: element.value ?? element.textContent };
    }
    if (name === "PressEnter") {
      element.focus();
      const input = Cc["@mozilla.org/text-input-processor;1"].createInstance(
        Ci.nsITextInputProcessor,
      );
      if (!input.beginInputTransactionForTests(window)) {
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
