// SPDX-License-Identifier: MPL-2.0
// Private actor observes real signed extension effects and sends native input only.
/** @import {MediaMessage} from "./assessment-test-types.ts" */
export class CorpusMediaChild extends JSWindowActorChild {
  /** @param {MediaMessage} message */
  receiveMessage({ name, data }) {
    const document = this.document,
      window = this.contentWindow,
      video = document?.querySelector("video");
    if (!document || !window) {
      throw Error("Private media document is unavailable");
    }
    const performance = window.performance;
    if (!performance) throw Error("Private media performance is unavailable");
    if (name === "Snapshot") {
      return {
        url: document.documentURI,
        ready: document.readyState,
        visibility: document.visibilityState,
        focused: document.hasFocus(),
        origin: performance.timeOrigin,
        video: video &&
          {
            readyState: video.readyState,
            paused: video.paused,
            currentTime: video.currentTime,
            duration: video.duration,
            playbackRate: video.playbackRate,
            seeking: video.seeking,
            seekable: Array.from(
              { length: video.seekable.length },
              (
                _,
                index,
              ) => [video.seekable.start(index), video.seekable.end(index)],
            ),
            error: video.error?.code,
          },
        bezel: document.querySelector("#efyt-bezel")?.textContent,
        dislike: document.querySelector("dislike-button-view-model")
          ?.textContent,
        body: document.body?.innerText.slice(0, 10000),
      };
    }
    const element = document.querySelector(data?.selector || "video");
    if (!element) throw Error("Missing authored native media control");
    element.scrollIntoView({ block: "center" });
    const rect = element.getBoundingClientRect();
    if (!rect.width || !rect.height) {
      throw Error("Authored native media control is hidden");
    }
    const x = rect.x + rect.width / 2, y = rect.y + rect.height / 2;
    if (name === "Click") {
      window.synthesizeMouseEvent("mousedown", x, y, {
        button: 0,
        buttons: 1,
        clickCount: 1,
      });
      window.synthesizeMouseEvent("mouseup", x, y, {
        button: 0,
        buttons: 0,
        clickCount: 1,
      });
      return true;
    }
    if (name === "ControlSpeed") {
      window.synthesizeMouseEvent("mousemove", x, y, { ctrlKey: true });
      window.windowUtils.sendWheelEvent(x, y, 0, -120, 0, 0, 2, 0, 0, 0);
      return true;
    }
    throw Error("Unknown authored media operation " + name);
  }
}
