// SPDX-License-Identifier: MPL-2.0
// Read-only test actor registered only in an isolated corpus test profile.
export class CorpusInspectorChild extends JSWindowActorChild {
  receiveMessage(message) {
    if (message.name !== "Snapshot") {
      throw new Error("Unknown corpus inspection operation");
    }
    const document = this.document;
    return {
      url: document.documentURI,
      title: document.title,
      readyState: document.readyState,
      text: document.body?.innerText?.slice(0, 18000),
      controls: [
        ...document.querySelectorAll(
          'button,a,input,textarea,select,[role="button"]',
        ),
      ]
        .slice(0, 100).map((element) => ({
          tag: element.tagName,
          id: element.id,
          type: element.type,
          text: (element.innerText || element.getAttribute("aria-label") ||
            element.getAttribute("placeholder") || "").slice(0, 200),
        })),
    };
  }
}
