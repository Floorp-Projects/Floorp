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
        defineAs: "NRGetLocaleInfo",
      });

      Cu.exportFunction(this.setAppLocale.bind(this), window, {
        defineAs: "NRSetAppLocale",
      });

      Cu.exportFunction(this.installLangPack.bind(this), window, {
        defineAs: "NRInstallLangPack",
      });

      Cu.exportFunction(this.getNativeNames.bind(this), window, {
        defineAs: "NRGetNativeNames",
      });
      Cu.exportFunction(this.setDefaultBrowser.bind(this), window, {
        defineAs: "NRSetDefaultBrowser",
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

  getNativeNames(
    langCodes: string[],
    callback: (localeInfo: string) => void = () => {},
  ) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetNativeNames = resolve;
    });
    this.sendAsyncMessage("WelcomePage:getNativeNames", { langCodes });
    promise.then((localeInfo) => callback(localeInfo));
  }

  resolveGetNativeNames: ((localeInfo: string) => void) | null = null;

  setDefaultBrowser(callback: (response: string) => void = () => {}) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveSetDefaultBrowser = resolve;
    });
    this.sendAsyncMessage("WelcomePage:setDefaultBrowser");
    promise.then((response) => callback(response));
  }

  resolveSetDefaultBrowser: ((response: string) => void) | null = null;

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

      case "WelcomePage:getNativeNamesResponse": {
        this.resolveGetNativeNames?.(message.data);
        this.resolveGetNativeNames = null;
        break;
      }

      case "WelcomePage:setDefaultBrowserResponse": {
        this.resolveSetDefaultBrowser?.(message.data);
        this.resolveSetDefaultBrowser = null;
        break;
      }
    }
  }
}
