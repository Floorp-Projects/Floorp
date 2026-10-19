// SPDX-License-Identifier: MPL-2.0
// Authored fixture observer and genuine native input in an unchanged product panel.
/** @import {DevtoolsMessage} from "./assessment-test-types.ts" */
export class CorpusDevtoolsChild extends JSWindowActorChild {
  /** @param {DevtoolsMessage} message */
  receiveMessage({ name, data }) {
    const doc = this.document;
    const win = this.contentWindow;
    if (!doc || !win) throw Error("Private DevTools document is unavailable");
    if (name === "Snapshot") {
      const props = /** @type {HTMLElement|null} */ (doc.querySelector(
        '[data-testname="InspectedElementPropsTree"]',
      ));
      return {
        url: doc.documentURI,
        ready: doc.readyState,
        text: doc.body?.innerText,
        reactCount: doc.getElementById("react-count")?.textContent,
        reduxCount: doc.getElementById("redux-count")?.textContent,
        props: props
          ? {
            text: props.innerText,
            inputs: [...props.querySelectorAll("input")].map((input) => ({
              value: input.value,
              test: input.getAttribute("data-testname"),
            })),
          }
          : null,
      };
    }
    if (name === "Click") {
      const elements = [...doc.querySelectorAll(data.selector)];
      const element = data.text
        ? elements.find((element) =>
          (element.textContent || "").trim() === data.text
        )
        : elements[0];
      if (!element) {
        throw Error("Missing actual native UI control " + data.selector);
      }
      element.scrollIntoView({ block: "center", inline: "center" });
      const rect = element.getBoundingClientRect();
      if (!rect.width || !rect.height) {
        throw Error("Native UI control is hidden");
      }
      win.synthesizeMouseEvent(
        "mousemove",
        rect.x + rect.width / 2,
        rect.y + rect.height / 2,
        {},
      );
      for (const [type, buttons] of [["mousedown", 1], ["mouseup", 0]]) {
        win.synthesizeMouseEvent(
          type,
          rect.x + rect.width / 2,
          rect.y + rect.height / 2,
          { button: 0, buttons, clickCount: 1 },
        );
      }
      return true;
    }
    throw Error("Unknown native DevTools corpus command: " + name);
  }
}
