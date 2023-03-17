/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class ContentMetaParent extends JSWindowActorParent {
  receiveMessage(message) {
    if (message.name == "Meta:SetPageInfo") {
      let browser = this.manager.browsingContext.top.embedderElement;
      if (browser) {
        let event = new browser.ownerGlobal.CustomEvent("pageinfo", {
          bubbles: true,
          cancelable: false,
          detail: {
            url: message.data.url,
            description: message.data.description,
            previewImageURL: message.data.previewImageURL,
          },
        });
        browser.dispatchEvent(event);
      }
    }
  }
}
