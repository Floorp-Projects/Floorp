/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewModule } from "resource://gre/modules/GeckoViewModule.sys.mjs";

export class GeckoViewTranslations extends GeckoViewModule {
  onInit() {
    debug`onInit`;
    this.registerListener([
      "GeckoView:Translations:Translate",
      "GeckoView:Translations:RestorePage",
    ]);
  }

  onEnable() {
    debug`onEnable`;
    this.window.addEventListener("TranslationsParent:OfferTranslation", this);
    this.window.addEventListener("TranslationsParent:LanguageState", this);
  }

  onDisable() {
    debug`onDisable`;
    this.window.removeEventListener(
      "TranslationsParent:OfferTranslation",
      this
    );
    this.window.removeEventListener("TranslationsParent:LanguageState", this);
  }

  onEvent(aEvent, aData, aCallback) {
    debug`onEvent: event=${aEvent}, data=${aData}`;
    switch (aEvent) {
      case "GeckoView:Translations:Translate":
        const { fromLanguage, toLanguage } = aData;
        try {
          this.getActor("Translations").translate(fromLanguage, toLanguage);
          aCallback.onSuccess();
        } catch (error) {
          // Bug 1853055 will add named error states.
          aCallback.onError(`Could not translate: ${error}`);
        }
        break;
      case "GeckoView:Translations:RestorePage":
        try {
          this.getActor("Translations").restorePage();
          aCallback.onSuccess();
        } catch (error) {
          // Bug 1853055 will add named error states.
          aCallback.onError(`Could not restore page: ${error}`);
        }
        break;
    }
  }

  handleEvent(aEvent) {
    debug`handleEvent: ${aEvent.type}`;
    switch (aEvent.type) {
      case "TranslationsParent:OfferTranslation":
        this.eventDispatcher.sendRequest({
          type: "GeckoView:Translations:Offer",
        });
        break;
      case "TranslationsParent:LanguageState":
        this.eventDispatcher.sendRequest({
          type: "GeckoView:Translations:StateChange",
          data: aEvent.detail,
        });
        break;
    }
  }
}
const { debug, warn } = GeckoViewTranslations.initLogging(
  "GeckoViewTranslations"
);
