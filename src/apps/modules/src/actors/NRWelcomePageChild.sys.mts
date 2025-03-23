/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRWelcomePageChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRWelcomePageChild created!");
    const window = this.contentWindow;
    if (
      window?.location.port === "5187" ||
      window?.location.href.startsWith("chrome://")
    ) {
      console.debug("NRWelcomePage 5187 ! or Chrome Page!");
      Cu.exportFunction(this.getLocaleInfo.bind(this), window, {
        defineAs: "getLocaleInfo",
      });

      Cu.exportFunction(this.setAppLocale.bind(this), window, {
        defineAs: "setAppLocale",
      });

      Cu.exportFunction(this.installLangPack.bind(this), window, {
        defineAs: "installLangPack",
      });
    }
  }

  getLocaleInfo(callback: (localeInfo: string) => void = () => {}) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetLocaleInfo = resolve;
    });
    this.sendAsyncMessage("WelcomePage:getLocaleInfo");
    promise.then((localeInfo) => callback(localeInfo));
  }

  resolveGetLocaleInfo: ((localeInfo: string) => void) | null = null;

  setAppLocale(
    locale: string,
    callback: (response: string) => void = () => {},
  ) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveSetAppLocale = resolve;
    });
    this.sendAsyncMessage("WelcomePage:setAppLocale", { locale });
    promise.then((response) => callback(response));
  }

  resolveSetAppLocale: ((response: string) => void) | null = null;

  installLangPack(
    langPack: any,
    callback: (response: string) => void = () => {},
  ) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveInstallLangPack = resolve;
    });
    this.sendAsyncMessage("WelcomePage:installLangPack", { langPack });
    promise.then((response) => callback(response));
  }

  resolveInstallLangPack: ((response: string) => void) | null = null;

  receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "WelcomePage:localeInfoResponse": {
        this.resolveGetLocaleInfo?.(message.data);
        this.resolveGetLocaleInfo = null;
        break;
      }

      case "WelcomePage:setAppLocaleResponse": {
        this.resolveSetAppLocale?.(message.data);
        this.resolveSetAppLocale = null;
        break;
      }

      case "WelcomePage:installLangPackResponse": {
        this.resolveInstallLangPack?.(message.data);
        this.resolveInstallLangPack = null;
        break;
      }

      case "AppConstants:GetConstants": {
        const window = this.contentWindow;
        if (window) {
          try {
            const constants = JSON.parse(message.data);
            Object.defineProperty(window, "AppConstants", {
              value: constants,
              writable: false,
              enumerable: true,
              configurable: true,
            });
          } catch (e) {
            console.error("Failed to parse AppConstants:", e);
          }
        }
        break;
      }
    }
  }
}
