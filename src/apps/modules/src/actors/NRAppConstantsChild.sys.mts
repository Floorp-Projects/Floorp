/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRAppConstantsChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRAppConstantsChild created!");
    const window = this.contentWindow;
    if (
      window?.location.port === "5183" ||
      window?.location.href.startsWith("chrome://") ||
      window?.location.href.startsWith("about:")
    ) {
      console.debug("NRAppConstants 5183 ! or Chrome Page!");
      Cu.exportFunction(this.NRGetConstants.bind(this), window, {
        defineAs: "NRGetConstants",
      });
    }
  }

  NRGetConstants(callback: (constants: string) => void = () => {}) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetConstants = resolve;
    });
    this.sendAsyncMessage("AppConstants:GetConstants");
    promise.then((constants) => callback(constants));
  }

  resolveGetConstants: ((constants: string) => void) | null = null;
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "AppConstants:GetConstants": {
        this.resolveGetConstants?.(message.data);
        this.resolveGetConstants = null;
        break;
      }
    }
  }
}
