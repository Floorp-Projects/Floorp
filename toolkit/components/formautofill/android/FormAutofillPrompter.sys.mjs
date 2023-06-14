/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements doorhanger singleton that wraps up the PopupNotifications and handles
 * the doorhager UI for formautofill related features.
 */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CreditCard: "resource://gre/modules/GeckoViewAutocomplete.sys.mjs",
  GeckoViewAutocomplete: "resource://gre/modules/GeckoViewAutocomplete.sys.mjs",
  GeckoViewPrompter: "resource://gre/modules/GeckoViewPrompter.sys.mjs",
});

// Sync with Autocomplete.SaveOption.Hint in Autocomplete.java
const CreditCardStorageHint = {
  NONE: 0,
  GENERATED: 1 << 0,
  LOW_CONFIDENCE: 1 << 1,
};

export let FormAutofillPrompter = {
  _createMessage(creditCards) {
    let hint = CreditCardStorageHint.NONE;
    return {
      // Sync with PromptController
      type: "Autocomplete:Save:CreditCard",
      hint,
      creditCards,
    };
  },

  async promptToSaveAddress(browser, type, description) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },

  async promptToSaveCreditCard(browser, storage, record, flowId) {
    const prompt = new lazy.GeckoViewPrompter(browser.ownerGlobal);

    const duplicateRecord = (await storage.getDuplicateRecords(record).next())
      .value;
    let newCreditCard;
    if (duplicateRecord) {
      newCreditCard = { ...duplicateRecord, ...record };
    } else {
      newCreditCard = record;
    }

    prompt.asyncShowPrompt(
      this._createMessage([lazy.CreditCard.fromGecko(newCreditCard)]),
      result => {
        const selectedCreditCard = result?.selection?.value;

        if (!selectedCreditCard) {
          return;
        }

        lazy.GeckoViewAutocomplete.onCreditCardSave(selectedCreditCard);
      }
    );
  },
};
