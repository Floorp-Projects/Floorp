// SPDX-License-Identifier: MPL-2.0
export class CorpusBitwardenChild extends JSWindowActorChild {
  /** @param {import("./sidepanel-test-types.ts").ProbeMessage} message */
  receiveMessage(message) {
    const { name, data } = message;
    const document = this.document;
    const contentWindow = this.contentWindow;
    if (!document || !contentWindow) {
      throw Error("Private actor document closed");
    }
    if (name === "Snapshot") {
      const controls = [
        .../** @type {NodeListOf<import("./sidepanel-test-types.ts").ProbeControl>} */ (document
          .querySelectorAll("input,textarea,button,a,select")),
      ]
        .filter((element) => {
          const rect = element.getBoundingClientRect();
          return rect.width && rect.height;
        })
        .map((element) => ({
          tag: element.tagName,
          id: element.id,
          type: element.type,
          label: element.getAttribute("aria-label") || element.innerText,
          formcontrolname: element.getAttribute("formcontrolname"),
          value: element.type === "password" ? "[redacted]" : element.value,
        }));
      return {
        url: document.URL.split("?")[0],
        text: document.body?.innerText,
        controls,
      };
    }
    if (name === "ReadFilled") {
      return {
        username: /** @type {HTMLInputElement|null} */ (document.querySelector(
          "input#authored-username",
        ))?.value,
        password: /** @type {HTMLInputElement|null} */ (document.querySelector(
          "input#authored-password",
        ))?.value,
        secure: contentWindow.isSecureContext,
      };
    }
    if (!data) throw Error("Private actor command payload missing");
    const element = [
      .../** @type {NodeListOf<HTMLElement>} */ (document.querySelectorAll(
        data.selector || "button,a",
      )),
    ]
      .find((element) => {
        const rect = element.getBoundingClientRect();
        return rect.width && rect.height &&
          (!data.text || (element.textContent || "").trim() === data.text ||
            element.getAttribute("aria-label") === data.text);
      });
    if (!element) throw Error("Visible authored control unavailable");
    element.scrollIntoView({ block: "center" });
    if (name === "Click") {
      const rect = element.getBoundingClientRect();
      for (const type of ["mousedown", "mouseup"]) {
        contentWindow.synthesizeMouseEvent(
          type,
          rect.x + rect.width / 2,
          rect.y + rect.height / 2,
          { button: 0, clickCount: 1 },
        );
      }
      return { clicked: true };
    }
    if (name === "Type") {
      element.focus();
      const input = Cc["@mozilla.org/text-input-processor;1"].createInstance(
        Ci.nsITextInputProcessor,
      );
      if (
        !input.beginInputTransactionForTests(
          /** @type {WindowProxy & mozIDOMWindow} */ (contentWindow),
        )
      ) {
        throw Error("Native input unavailable");
      }
      if (typeof data.value !== "string") {
        throw Error("Native input text missing");
      }
      input.commitCompositionWith(data.value);
      return { typed: true };
    }
    throw Error("Unknown Bitwarden test operation");
  }
}
