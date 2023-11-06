/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  TranslationsParent: "resource://gre/actors/TranslationsParent.sys.mjs",
});

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

// Runtime functionality
export const GeckoViewTranslationsSettings = {
  /* eslint-disable complexity */
  async onEvent(aEvent, aData, aCallback) {
    debug`onEvent ${aEvent} ${aData}`;

    switch (aEvent) {
      case "GeckoView:Translations:IsTranslationEngineSupported": {
        lazy.TranslationsParent.getIsTranslationsEngineSupported().then(
          function (value) {
            aCallback.onSuccess(value);
          },
          function (error) {
            // Bug 1853055 will add named error states.
            aCallback.onError(
              `An issue occurred while checking the translations engine: ${error}`
            );
          }
        );
        return;
      }
      case "GeckoView:Translations:PreferredLanguages": {
        aCallback.onSuccess({
          preferredLanguages: lazy.TranslationsParent.getPreferredLanguages(),
        });
        return;
      }
      case "GeckoView:Translations:ManageModel": {
        const { language, operation, operationLevel } = aData;
        if (operation === "delete") {
          if (operationLevel === "all") {
            lazy.TranslationsParent.deleteAllLanguageFiles().then(
              function (value) {
                aCallback.onSuccess();
              },
              function (error) {
                // Bug 1853055 will add named error states.
                aCallback.onError(
                  `An issue occurred while deleting all language files: ${error}`
                );
              }
            );
            return;
          }
          if (operationLevel === "language") {
            if (language == null) {
              aCallback.onError(
                `A specified language is required language level operations.`
              );
              return;
            }
            lazy.TranslationsParent.deleteLanguageFiles(language).then(
              function (value) {
                aCallback.onSuccess();
              },
              function (error) {
                // Bug 1853055 will add named error states.
                aCallback.onError(
                  `An issue occurred while deleting a language file: ${error}`
                );
              }
            );
            return;
          }
        }
        if (operation === "download") {
          if (operationLevel === "all") {
            lazy.TranslationsParent.downloadAllFiles().then(
              function (value) {
                aCallback.onSuccess();
              },
              function (error) {
                // Bug 1853055 will add named error states.
                aCallback.onError(
                  `An issue occurred while downloading all language files: ${error}`
                );
              }
            );
            return;
          }
          if (operationLevel === "language") {
            if (language == null) {
              aCallback.onError(
                `A specified language is required language level operations.`
              );
              return;
            }
            lazy.TranslationsParent.downloadLanguageFiles(language).then(
              function (value) {
                aCallback.onSuccess();
              },
              function (error) {
                // Bug 1853055 will add named error states.
                aCallback.onError(
                  `An issue occurred while downloading a language files: ${error}`
                );
              }
            );
          }
        }
        break;
      }
      case "GeckoView:Translations:TranslationInformation": {
        if (
          Cu.isInAutomation &&
          Services.prefs.getBoolPref(
            "browser.translations.geckoview.enableAllTestMocks",
            false
          )
        ) {
          const mockResult = {
            languagePairs: [
              { fromLang: "en", toLang: "es" },
              { fromLang: "es", toLang: "en" },
            ],
            fromLanguages: [
              { langTag: "en", displayName: "English" },
              { langTag: "es", displayName: "Spanish" },
            ],
            toLanguages: [
              { langTag: "en", displayName: "English" },
              { langTag: "es", displayName: "Spanish" },
            ],
          };
          aCallback.onSuccess(mockResult);
          return;
        }

        lazy.TranslationsParent.getSupportedLanguages().then(
          function (value) {
            aCallback.onSuccess(value);
          },
          function (error) {
            // Bug 1853055 will add named error states.
            aCallback.onError(
              `Could not retrieve requested information: ${error}`
            );
          }
        );
        break;
      }
      case "GeckoView:Translations:ModelInformation": {
        if (
          Cu.isInAutomation &&
          Services.prefs.getBoolPref(
            "browser.translations.geckoview.enableAllTestMocks",
            false
          )
        ) {
          const mockResult = {
            models: [
              {
                langTag: "es",
                displayName: "Spanish",
                isDownloaded: false,
                size: 12345,
              },
              {
                langTag: "de",
                displayName: "German",
                isDownloaded: false,
                size: 12345,
              },
            ],
          };
          aCallback.onSuccess(mockResult);
          return;
        }

        // Helper function to process remote server records size and download state for GV use
        async function _processLanguageModelData(language, remoteRecords) {
          // Aggregate size of downloads, e.g., one language has many model binary files
          var size = 0;
          remoteRecords.forEach(item => {
            size += parseInt(item.attachment.size);
          });
          // Check if required files are downloaded
          var isDownloaded =
            await lazy.TranslationsParent.hasAllFilesForLanguage(
              language.langTag
            );
          var model = {
            langTag: language.langTag,
            displayName: language.displayName,
            isDownloaded,
            size,
          };
          return model;
        }

        // Main call to toolkit
        lazy.TranslationsParent.getSupportedLanguages().then(
          // Retrieve supported languages
          async function (supportedLanguages) {
            // Get language display information,
            const languageList =
              lazy.TranslationsParent.getLanguageList(supportedLanguages);
            var results = [];
            // For each language, process the related remote server model records
            languageList.forEach(language => {
              const recordsResult =
                lazy.TranslationsParent.getRecordsForTranslatingToAndFromAppLanguage(
                  language.langTag,
                  false
                ).then(
                  async function (records) {
                    return _processLanguageModelData(language, records);
                  },
                  function (recordError) {
                    aCallback.onError(
                      `An issue occurred while aggregating information: ${recordError}`
                    );
                  },
                  language
                );
              results.push(recordsResult);
            });
            // Aggregate records
            Promise.all(results).then(models => {
              var response = [];
              models.forEach(item => {
                response.push(item);
              });
              aCallback.onSuccess({ models: response });
            });
          },
          function (languageError) {
            aCallback.onError(
              `An issue occurred while retrieving the supported languages: ${languageError}`
            );
          }
        );
        break;
      }
    }
  },
};

const { debug, warn } = GeckoViewTranslations.initLogging(
  "GeckoViewTranslations"
);
