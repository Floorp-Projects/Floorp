// SPDX-License-Identifier: MPL-2.0
/// <reference types="../../libs/@types/gecko/index.d.ts" />
// Read-only observation of real extension effects in a private test profile.
export class CorpusContentChild extends JSWindowActorChild {
  /** @param {{name: string}} message */
  receiveMessage({ name }) {
    if (name !== "Snapshot") {
      throw new Error("Unknown content corpus observation");
    }
    const document = this.document;
    const window = this.contentWindow;
    if (!document || !window) {
      throw new Error("Content corpus document is no longer available");
    }
    const root = /** @type {HTMLElement | null} */ (document.documentElement);
    const frameRoot =
      /** @type {HTMLElement | null | undefined} */ (document.querySelector(
        "iframe",
      )?.contentDocument?.documentElement);
    const body = document.body;
    const probe = document.querySelector("#probe");
    return {
      url: document.documentURI,
      ready: document.readyState,
      title: document.title,
      body: body &&
        window.getComputedStyle(body)?.getPropertyValue("background-color"),
      color: body && window.getComputedStyle(body)?.getPropertyValue("color"),
      probe: probe &&
        window.getComputedStyle(probe)?.getPropertyValue("background-color"),
      markers: [...document.querySelectorAll("style.darkreader")].map(
        (element) => element.className,
      ),
      text: probe?.textContent,
      userScript: root?.dataset.floorpUserScript,
      frameUserScript: frameRoot?.dataset.floorpUserScript,
      authoredScript: root?.dataset.authoredUserscript,
      styles: [...document.querySelectorAll("style")].map((style) => ({
        id: style.id,
        className: style.className,
        text: style.textContent.slice(0, 1000),
      })),
    };
  }
}
