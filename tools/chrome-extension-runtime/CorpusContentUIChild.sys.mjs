// SPDX-License-Identifier: MPL-2.0
// Real native input into unchanged extension UI, registered only by private tests.
/** @import {ContentUIMessage,CodeMirrorElement,NativeInputWindow} from "./assessment-test-types.ts" */
export class CorpusContentUIChild extends JSWindowActorChild {
  /** @param {ContentUIMessage} message */
  receiveMessage({ name, data }) {
    const document = this.document,
      window = /** @type {NativeInputWindow|null} */ (this.contentWindow);
    if (!document || !window) {
      throw Error("Private extension editor document is unavailable");
    }
    if (name === "SnapshotUI") {
      return {
        url: document.documentURI,
        ready: document.readyState,
        text: document.body?.innerText?.slice(0, 10000),
        hasEditor: !!Cu.waiveXrays(
          /** @type {CodeMirrorElement|null} */ (document.querySelector(
            ".CodeMirror",
          )),
        )
          ?.CodeMirror,
        name:
          /** @type {HTMLInputElement|null} */ (document.querySelector("#name"))
            ?.value,
        code: Cu.waiveXrays(
          /** @type {CodeMirrorElement|null} */ (document.querySelector(
            ".CodeMirror",
          )),
        )?.CodeMirror
          ?.getValue(),
        saveDisabled:
          /** @type {HTMLButtonElement|null} */ (document.querySelector(
            "#save-button",
          ))?.disabled,
        controls: [
          .../** @type {NodeListOf<HTMLInputElement|HTMLButtonElement|HTMLTextAreaElement|HTMLSelectElement>} */ (document
            .querySelectorAll("button,input,textarea,select")),
        ]
          .slice(0, 100).map((element) => ({
            id: element.id,
            type: element.type,
            text: element.textContent,
            disabled: element.disabled,
            checked: "checked" in element ? element.checked : undefined,
            value: element.value,
          })),
      };
    }
    const element = data.menuText
      ? [...document.querySelectorAll("*")].find((element) =>
        !element.children.length &&
        (element.textContent || "").trim() === data.menuText &&
        element.getBoundingClientRect().width
      )
      : data.buttonText
      ? [...document.querySelectorAll("button")].find((element) =>
        (element.textContent || "").trim() === data.buttonText
      )
      : data.selector
      ? document.querySelector(data.selector)
      : null;
    if (!element) throw Error("Missing extension UI control: " + data.selector);
    if (name === "Click") {
      element.scrollIntoView({ block: "center" });
      const rect = element.getBoundingClientRect();
      if (!rect.width || !rect.height) {
        throw Error(
          "Extension UI control is hidden: " +
            (data.selector || data.menuText || data.buttonText),
        );
      }
      window.synthesizeMouseEvent(
        "mousemove",
        rect.x + rect.width / 2,
        rect.y + rect.height / 2,
        {},
      );
      window.synthesizeMouseEvent(
        "mousedown",
        rect.x + rect.width / 2,
        rect.y + rect.height / 2,
        { button: 0, buttons: 1, clickCount: 1 },
      );
      window.synthesizeMouseEvent(
        "mouseup",
        rect.x + rect.width / 2,
        rect.y + rect.height / 2,
        { button: 0, buttons: 0, clickCount: 1 },
      );
      return { clicked: true };
    }
    if (name === "Type") {
      if (typeof data.text !== "string") {
        throw Error("Actual editor text is required");
      }
      if (!("focus" in element) || typeof element.focus !== "function") {
        throw Error("Actual editor control cannot receive focus");
      }
      element.focus();
      const tip = Cc["@mozilla.org/text-input-processor;1"].createInstance(
        Ci.nsITextInputProcessor,
      );
      if (
        !tip.beginInputTransactionForTests(window, { onNotify: () => true })
      ) throw Error("Native input transaction unavailable");
      tip.keydown(
        new window.KeyboardEvent("keydown", { key: "Control" }),
        tip.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT,
      );
      const key = new window.KeyboardEvent("keydown", {
        key: "a",
        code: "KeyA",
        keyCode: 65,
        ctrlKey: true,
      });
      tip.keydown(key);
      tip.keyup(
        new window.KeyboardEvent("keyup", {
          key: "a",
          code: "KeyA",
          keyCode: 65,
          ctrlKey: true,
        }),
      );
      tip.keyup(
        new window.KeyboardEvent("keyup", { key: "Control" }),
        tip.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT,
      );
      if (!tip.commitCompositionWith(data.text)) {
        throw Error("Native editor text input rejected");
      }
      return { typed: true };
    }
    throw Error("Unknown content UI operation: " + name);
  }
}
