/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRWebScraperChild extends JSWindowActorChild {
  receiveMessage(
    message: { name: string; data: { selector: string; value: string } },
  ) {
    switch (message.name) {
      case "WebScraper:GetHTML":
        return this.getHTML();
      case "WebScraper:InputElement":
        this.inputElement(message.data.selector, message.data.value);
        break;
    }
    return null;
  }

  getHTML(): string | null {
    try {
      if (this.contentWindow) {
        return this.contentWindow.document.documentElement.outerHTML;
      }
      return null;
    } catch (e) {
      console.error("Error getting HTML:", e);
      return null;
    }
  }

  inputElement(selector: string, value: string): void {
    const element = this.document?.querySelector(selector) as
      | HTMLInputElement
      | HTMLTextAreaElement;
    if (element) {
      element.value = value;
    }
  }
}
