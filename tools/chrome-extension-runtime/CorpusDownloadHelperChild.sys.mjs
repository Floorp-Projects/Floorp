/// <reference path="../../libs/@types/gecko/index.d.ts" />
// SPDX-License-Identifier: MPL-2.0
// Profile-private observation/native-input actor; never changes extension state.
import { snapshotUI } from "./corpus-sidepanel-ui.sys.mjs";
export class CorpusDownloadHelperChild extends JSWindowActorChild {
  /** @param {{name:string,data?:{text?:string}}} message */
  receiveMessage({ name, data = {} }) {
    const document = this.document;
    const contentWindow = this.contentWindow;
    if (!document || !contentWindow) {
      throw Error("Private media actor lost its document");
    }
    if (name === "Snapshot") return snapshotUI(document);
    if (name === "Playback") {
      const video = document.querySelector("video");
      return video &&
        {
          currentTime: video.currentTime,
          width: video.videoWidth,
          height: video.videoHeight,
          duration: video.duration,
          error: video.error?.message,
        };
    }
    if (name !== "Click") throw Error("Unknown private media operation");
    /** @type {Element[]} */
    const elements = [];
    /** @param {Document|ShadowRoot} root */
    const walk = (root) => {
      for (const element of root.querySelectorAll("*")) {
        elements.push(element);
        if (element.shadowRoot) walk(element.shadowRoot);
      }
    };
    walk(document);
    const element = elements.find((element) =>
      element.matches("button,a,[role=button]") &&
      (element.textContent || "").trim() === data.text &&
      element.getBoundingClientRect().width
    );
    if (!element) {
      throw Error("Actual visible vendor control missing: " + data.text);
    }
    element.scrollIntoView({ block: "center", inline: "center" });
    const rect = element.getBoundingClientRect(),
      x = rect.x + rect.width / 2,
      y = rect.y + rect.height / 2;
    let hit = document.elementFromPoint(x, y);
    while (hit?.shadowRoot) {
      const next = hit.shadowRoot.elementFromPoint(x, y);
      if (!next || next === hit) break;
      hit = next;
    }
    if (hit !== element && !element.contains(hit)) {
      throw Error("Vendor control is obscured: " + data.text);
    }
    contentWindow.synthesizeMouseEvent("mousedown", x, y, {
      button: 0,
      clickCount: 1,
    });
    contentWindow.synthesizeMouseEvent("mouseup", x, y, {
      button: 0,
      clickCount: 1,
    });
    return { text: data.text, html: String(element.outerHTML).slice(0, 1500) };
  }
}
