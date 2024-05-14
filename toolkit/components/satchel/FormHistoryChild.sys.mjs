/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CreditCard: "resource://gre/modules/CreditCard.sys.mjs",
  FormHistoryAutoCompleteResult:
    "resource://gre/modules/FormHistoryAutoComplete.sys.mjs",
  FormScenarios: "resource://gre/modules/FormScenarios.sys.mjs",
  GenericAutocompleteItem: "resource://gre/modules/FillHelpers.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(lazy, "gDebug", "browser.formfill.debug");
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gEnabled",
  "browser.formfill.enable"
);

function log(message) {
  if (!lazy.gDebug) {
    return;
  }
  Services.console.logStringMessage("satchelFormListener: " + message);
}

export class FormHistoryChild extends JSWindowActorChild {
  handleEvent(event) {
    switch (event.type) {
      case "DOMFormBeforeSubmit":
        this.#onDOMFormBeforeSubmit(event.target);
        break;
      default:
        throw new Error("Unexpected event");
    }
  }

  static getInputName(input) {
    return input.name || input.id;
  }

  #onDOMFormBeforeSubmit(form) {
    if (
      !lazy.gEnabled ||
      lazy.PrivateBrowsingUtils.isContentWindowPrivate(form.ownerGlobal)
    ) {
      return;
    }

    log("Form submit observer notified.");

    if (form.getAttribute("autocomplete")?.toLowerCase() == "off") {
      return;
    }

    const entries = [];
    for (const input of form.elements) {
      if (!HTMLInputElement.isInstance(input)) {
        continue;
      }

      // Only use inputs that hold text values (not including type="password")
      if (!input.mozIsTextField(true)) {
        continue;
      }

      // Don't save fields that were previously type=password such as on sites
      // that allow the user to toggle password visibility.
      if (input.hasBeenTypePassword) {
        continue;
      }

      // Bug 1780571, Bug 394612: If Login Manager marked this input, don't save it.
      // The login manager will deal with remembering it.
      if (this.manager.getActor("LoginManager")?.isLoginManagerField(input)) {
        continue;
      }

      // Don't save values when @autocomplete is "off" or has a sensitive field name.
      const autocompleteInfo = input.getAutocompleteInfo();
      if (autocompleteInfo?.canAutomaticallyPersist === false) {
        continue;
      }

      const value = input.lastInteractiveValue?.trim();

      // Only save user entered values even if they match the default value.
      // Any script input is ignored.
      // See Bug 1642570 for details.
      if (!value) {
        continue;
      }

      // Save only when user input was last.
      if (value != input.value.trim()) {
        continue;
      }

      // Don't save credit card numbers.
      if (lazy.CreditCard.isValidNumber(value)) {
        log("skipping saving a credit card number");
        continue;
      }

      const name = FormHistoryChild.getInputName(input);
      if (!name) {
        continue;
      }

      if (name == "searchbar-history") {
        log('addEntry for input name "' + name + '" is denied');
        continue;
      }

      // Limit stored data to 200 characters.
      if (name.length > 200 || value.length > 200) {
        log("skipping input that has a name/value too large");
        continue;
      }

      entries.push({ name, value });

      // Limit number of fields stored per form.
      if (entries.length >= 100) {
        log("not saving any more entries for this form.");
        break;
      }
    }

    if (entries.length) {
      log("sending entries to parent process for form " + form.id);
      this.sendAsyncMessage("FormHistory:FormSubmitEntries", entries);
    }
  }

  get actorName() {
    return "FormHistory";
  }

  /**
   * Get the search options when searching for autocomplete entries in the parent
   *
   * @param {HTMLInputElement} input - The input element to search for autocompelte entries
   * @returns {object} the search options for the input
   */
  getAutoCompleteSearchOption(input) {
    const inputName = FormHistoryChild.getInputName(input);
    const scenarioName = lazy.FormScenarios.detect({ input }).signUpForm
      ? "SignUpFormScenario"
      : "";

    return { inputName, scenarioName };
  }

  /**
   * Ask the provider whether it might have autocomplete entry to show
   * for the given input.
   *
   * @param {HTMLInputElement} input - The input element to search for autocompelte entries
   * @returns {boolean} true if we shold search for autocomplete entries
   */
  shouldSearchForAutoComplete(input) {
    if (!lazy.gEnabled) {
      return false;
    }

    const inputName = FormHistoryChild.getInputName(input);
    // Don't allow form inputs (aField != null) to get results from
    // search bar history.
    if (inputName == "searchbar-history") {
      log(`autoCompleteSearch for input name "${inputName}" is denied`);
      return false;
    }

    if (input.autocomplete == "off" || input.form?.autocomplete == "off") {
      log("autoCompleteSearch not allowed due to autcomplete=off");
      return false;
    }

    return true;
  }

  /**
   * Convert the search result to autocomplete results
   *
   * @param {string} searchString - The string to search for
   * @param {HTMLInputElement} input - The input element to search for autocompelte entries
   * @param {Array<object>} records - autocomplete records
   * @returns {AutocompleteResult}
   */
  searchResultToAutoCompleteResult(searchString, input, records) {
    const inputName = FormHistoryChild.getInputName(input);
    const acResult = new lazy.FormHistoryAutoCompleteResult(
      input,
      [],
      inputName,
      searchString
    );

    acResult.fixedEntries = this.getDataListSuggestions(input);
    if (!records) {
      return acResult;
    }

    const entries = records.formHistoryEntries;
    const externalEntries = records.externalEntries;

    if (input?.maxLength > -1) {
      acResult.entries = entries.filter(
        el => el.text.length <= input.maxLength
      );
    } else {
      acResult.entries = entries;
    }

    acResult.externalEntries.push(
      ...externalEntries.map(
        entry =>
          new lazy.GenericAutocompleteItem(
            entry.image,
            entry.label,
            entry.secondary,
            entry.fillMessageName,
            entry.fillMessageData
          )
      )
    );

    acResult.removeDuplicateHistoryEntries();
    return acResult;
  }

  #isTextControl(input) {
    return [
      "text",
      "email",
      "search",
      "tel",
      "url",
      "number",
      "month",
      "week",
      "password",
    ].includes(input.type);
  }

  getDataListSuggestions(input) {
    const items = [];

    if (!this.#isTextControl(input) || !input.list) {
      return items;
    }

    const upperFieldValue = input.value.toUpperCase();

    for (const option of input.list.options) {
      const label = option.label || option.text || option.value || "";

      if (!label.toUpperCase().includes(upperFieldValue)) {
        continue;
      }

      items.push({
        label,
        value: option.value,
      });
    }

    return items;
  }
}
