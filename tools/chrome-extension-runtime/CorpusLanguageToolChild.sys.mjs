// SPDX-License-Identifier: MPL-2.0
// Real native input into unchanged extension UI, registered only by private tests.
export class CorpusLanguageToolChild extends JSWindowActorChild {
  /** @param {import("./network-test-types.ts").LanguageActorMessage} message */
  receiveMessage({ name, data = {} }) {
    const document = this.document,
      window = this.contentWindow;
    if (!document || !window) throw Error("Native corpus document unavailable");
    if (name === "Snapshot") {
      /** @type {import("./network-test-types.ts").LanguageProduct[]} */
      const product = [];
      /** @param {Document | ShadowRoot} root */
      const visit = (root) => {
        for (const element of root.querySelectorAll("*")) {
          if (element.shadowRoot) visit(element.shadowRoot);
          const rect = element.getBoundingClientRect();
          if (
            rect.width &&
            rect.height &&
            (element.tagName.startsWith("LT-") ||
              element.getAttribute("role") === "dialog" ||
              (typeof element.className === "string" &&
                element.className.includes("lt-")))
          ) {
            product.push({
              tag: element.tagName,
              cls:
                typeof element.className === "string" ? element.className : "",
              text:
                /** @type {HTMLElement} */ (element).innerText?.slice(
                  0,
                  1500,
                ) ?? element.textContent?.slice(0, 1500),
              html: element.outerHTML.slice(0, 2500),
            });
          }
        }
      };
      visit(document);
      return {
        url: document.documentURI,
        ready: document.readyState,
        origin: window.performance?.timeOrigin,
        installed:
          document.documentElement?.getAttribute("data-lt-installed") ?? null,
        text: /** @type {HTMLTextAreaElement | null} */ (
          document.querySelector("textarea#editor")
        )?.value,
        body: document.body?.innerText?.slice(0, 12000) ?? "",
        product: product.slice(-60),
      };
    }
    const candidate = data.menuText
      ? [...document.querySelectorAll("*")].find(
          (element) =>
            !element.children.length &&
            element.textContent?.trim() === data.menuText &&
            element.getBoundingClientRect().width,
        )
      : data.buttonText
        ? [...document.querySelectorAll("button")].find(
            (element) => element.textContent?.trim() === data.buttonText,
          )
        : name === "Click" && data.text
          ? [...document.querySelectorAll(data.selector ?? "")].find(
              (element) => element.textContent?.trim() === data.text,
            )
          : document.querySelector(data.selector ?? "");
    if (!candidate)
      throw Error("Missing extension UI control: " + data.selector);
    const element = /** @type {HTMLElement} */ (candidate);
    if (name === "Click") {
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
      element.focus();
      const tip = Cc["@mozilla.org/text-input-processor;1"].createInstance(
        Ci.nsITextInputProcessor,
      );
      if (!window.docShell) throw Error("Native document shell unavailable");
      if (
        !tip.beginInputTransactionForTests(window.docShell.domWindow, {
          onNotify: () => true,
        })
      )
        throw Error("Native input transaction unavailable");
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
      if (data.text === undefined) throw Error("Text input missing");
      if (!tip.commitCompositionWith(data.text)) {
        throw Error("Native editor text input rejected");
      }
      return { typed: true };
    }
    throw Error("Unknown content UI operation: " + name);
  }
}
