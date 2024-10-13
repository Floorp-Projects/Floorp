/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRTabManagerChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRTabManagerChild created!");
    const window = this.contentWindow;
    if (window?.location.port === "5183") {
      console.debug("NRTabManager 5183!");
      Cu.exportFunction(this.NRAddTab.bind(this), window, {
        defineAs: "NRAddTab",
      });
    }
  }
  NRAddTab(url: string, options: object = {}, callback: () => void = () => {}) {
    const promise = new Promise<void>((resolve, _reject) => {
      this.resolveAddTab = resolve;
    });
    this.sendAsyncMessage("Tabs:AddTab", { url, options });
    promise.then((_v) => callback());
  }

  resolveAddTab: (() => void) | null = null;
  resolveLoadTrustedUrl: (() => void) | null = null;
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "Tabs:AddTab": {
        this.resolveAddTab?.();
        this.resolveLoadTrustedUrl = null;
        break;
      }
    }
  }
}
