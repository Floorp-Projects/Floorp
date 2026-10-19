// SPDX-License-Identifier: MPL-2.0
// Read-only observation of real extension effects in a private test profile.
export class CorpusContentChild extends JSWindowActorChild {
  receiveMessage({ name }) {
    if (name !== "Snapshot") {
      throw new Error("Unknown content corpus observation");
    }
    const document = this.document;
    const window = this.contentWindow;
    const body = document.body;
    const probe = document.querySelector("#probe");
    return {
      url: document.documentURI,
      ready: document.readyState,
      title: document.title,
      body: body && window.getComputedStyle(body).backgroundColor,
      color: body && window.getComputedStyle(body).color,
      probe: probe && window.getComputedStyle(probe).backgroundColor,
      markers: [...document.querySelectorAll("style.darkreader")].map(
        (element) => element.className,
      ),
      text: probe?.textContent,
      authoredScript: document.documentElement.dataset.authoredUserscript,
      styles: [...document.querySelectorAll("style")].map((style) => ({
        id: style.id,
        className: style.className,
        text: style.textContent.slice(0, 1000),
      })),
    };
  }
}
