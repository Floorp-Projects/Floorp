/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRSettingsChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRSettingsChild created!");
    const window = this.contentWindow;
    if (window?.location.port === "5183") {
      console.debug("NRTabManager 5183!");
      Cu.exportFunction(this.NRSPing.bind(this), window, {
        defineAs: "NRSPing",
      });
    }
  }
  NRSPing(url: string, options: object, callback: () => void) {
    const promise = new Promise<void>((resolve, _reject) => {
      this.resolveAddTrustedTab = resolve;
    });
    this.sendAsyncMessage("Tabs:AddTrustedTab", url, options);
    promise.then((_v) => callback());
  }

  resolveAddTrustedTab: (() => void) | null = null;
  resolveLoadTrustedUrl: (() => void) | null = null;
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "Tabs:AddTrustedTab": {
        this.resolveAddTrustedTab?.();
        this.resolveLoadTrustedUrl = null;
        break;
      }
    }
  }
}
