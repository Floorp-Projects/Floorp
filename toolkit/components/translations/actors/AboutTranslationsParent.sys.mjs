/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  TranslationsParent: "resource://gre/actors/TranslationsParent.sys.mjs",
});

/**
 * This parent is blank because the Translations actor handles most of the features
 * needed in AboutTranslations.
 */
export class AboutTranslationsParent extends JSWindowActorParent {
  #isDestroyed = false;

  didDestroy() {
    this.#isDestroyed = true;
  }

  async receiveMessage({ name, data }) {
    switch (name) {
      case "AboutTranslations:GetTranslationsPort": {
        const { fromLanguage, toLanguage } = data;
        const engineProcess = await lazy.TranslationsParent.getEngineProcess();
        if (this.#isDestroyed) {
          return undefined;
        }
        const { port1, port2 } = new MessageChannel();
        engineProcess.actor.startTranslation(
          fromLanguage,
          toLanguage,
          port1,
          this.browsingContext.top.embedderElement.innerWindowID
        );

        // At the time of writing, you can't return a port via the `sendQuery` API,
        // so results can't just be returned. The `sendAsyncMessage` method must be
        // invoked. Additionally, in the AboutTranslationsChild, the port must
        // be transfered to the content page with `postMessage`.
        this.sendAsyncMessage(
          "AboutTranslations:SendTranslationsPort",
          {
            fromLanguage,
            toLanguage,
            port: port2,
          },
          [port2] // Mark the port as transerable.
        );
        return undefined;
      }
      case "AboutTranslations:GetSupportedLanguages": {
        return lazy.TranslationsParent.getSupportedLanguages();
      }
      case "AboutTranslations:IsTranslationsEngineSupported": {
        return lazy.TranslationsParent.getIsTranslationsEngineSupported();
      }
      default:
        throw new Error("Unknown AboutTranslations message: " + name);
    }
  }
}
