// SPDX-License-Identifier: MPL-2.0
// Private test actor: only authored fixtures and isolated corpus profiles register it.
import { snapshotUI } from "./corpus-sidepanel-ui.sys.mjs";
export class CorpusOperationsChild extends JSWindowActorChild {
  receiveMessage({ name, data = {} }) {
    if (name === "Snapshot") {
      return {
        ...snapshotUI(this.document),
        probe: this.document.documentElement.dataset.authoredLifecycleProbe,
        cookie: this.document.cookie,
      };
    }
    const element = [...this.document.querySelectorAll(data.selector)].reverse()
      .find(
        (element) =>
          (!data.text ||
            (data.exact
              ? element.textContent.trim() === data.text
              : element.textContent.includes(data.text))) &&
          element.getBoundingClientRect().width &&
          element.getBoundingClientRect().height,
      );
    if (!element) throw Error("Visible corpus operation control missing");
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
    if (name === "Type") {
      element.focus();
      const input = Cc["@mozilla.org/text-input-processor;1"].createInstance(
        Ci.nsITextInputProcessor,
      );
      if (!input.beginInputTransactionForTests(window)) {
        throw Error("Native corpus text input transaction unavailable");
      }
      input.commitCompositionWith(data.text);
      return { value: element.value };
    }
    throw Error("Unknown corpus operation");
  }
}
