/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["FormHistoryChild"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "CreditCard",
  "resource://gre/modules/CreditCard.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

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
  dump("satchelFormListener: " + message + "\n");
  Services.console.logStringMessage("satchelFormListener: " + message);
}

class FormHistoryChild extends JSWindowActorChild {
  handleEvent(event) {
    switch (event.type) {
      case "DOMFormBeforeSubmit": {
        this.onDOMFormBeforeSubmit(event);
        break;
      }
      default: {
        throw new Error("Unexpected event");
      }
    }
  }

  onDOMFormBeforeSubmit(event) {
    let form = event.target;
    if (
      !lazy.gEnabled ||
      lazy.PrivateBrowsingUtils.isContentWindowPrivate(form.ownerGlobal)
    ) {
      return;
    }

    log("Form submit observer notified.");

    if (
      form.hasAttribute("autocomplete") &&
      form.getAttribute("autocomplete").toLowerCase() == "off"
    ) {
      return;
    }

    let entries = [];
    for (let input of form.elements) {
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

      // Bug 394612: If Login Manager marked this input, don't save it.
      // The login manager will deal with remembering it.

      // Don't save values when @autocomplete is "off" or has a sensitive field name.
      let autocompleteInfo = input.getAutocompleteInfo();
      if (autocompleteInfo && !autocompleteInfo.canAutomaticallyPersist) {
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

      let name = input.name || input.id;
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

      // Limit number of fields stored per form.
      if (entries.length >= 100) {
        log("not saving any more entries for this form.");
        break;
      }

      entries.push({ name, value });
    }

    if (entries.length) {
      log("sending entries to parent process for form " + form.id);
      this.sendAsyncMessage("FormHistory:FormSubmitEntries", entries);
    }
  }
}
