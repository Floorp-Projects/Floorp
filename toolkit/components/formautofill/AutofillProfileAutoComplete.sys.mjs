/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Form Autofill content process module.
 */

import { sendFillRequestToParent } from "resource://gre/modules/FillHelpers.sys.mjs";

/* eslint-disable no-use-before-define */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FormAutofill: "resource://autofill/FormAutofill.sys.mjs",
  FormAutofillContent: "resource://autofill/FormAutofillContent.sys.mjs",
});

const autocompleteController = Cc[
  "@mozilla.org/autocomplete/controller;1"
].getService(Ci.nsIAutoCompleteController);

function getActorFromWindow(contentWindow, name = "FormAutofill") {
  // In unit tests, contentWindow isn't a real window.
  if (!contentWindow) {
    return null;
  }

  return contentWindow.windowGlobalChild
    ? contentWindow.windowGlobalChild.getActor(name)
    : null;
}

export const ProfileAutocomplete = {
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  _registered: false,

  ensureRegistered() {
    if (this._registered) {
      return;
    }

    this.log = lazy.FormAutofill.defineLogGetter(this, "ProfileAutocomplete");
    this.debug("ensureRegistered");
    this._registered = true;

    Services.obs.addObserver(this, "autocomplete-will-enter-text");

    this.debug(
      "ensureRegistered. Finished with _registered:",
      this._registered
    );
  },

  ensureUnregistered() {
    if (!this._registered) {
      return;
    }

    this.debug("ensureUnregistered");
    this._registered = false;

    Services.obs.removeObserver(this, "autocomplete-will-enter-text");
  },

  async observe(_subject, topic, _data) {
    switch (topic) {
      case "autocomplete-will-enter-text": {
        if (!lazy.FormAutofillContent.activeInput) {
          // The observer notification is for autocomplete in a different process.
          break;
        }
        await this._fillFromAutocompleteRow(
          lazy.FormAutofillContent.activeInput
        );
        break;
      }
    }
  },

  async _fillFromAutocompleteRow(focusedInput) {
    this.debug("_fillFromAutocompleteRow:", focusedInput);
    let formDetails = lazy.FormAutofillContent.activeFormDetails;
    if (!formDetails) {
      // The observer notification is for a different frame.
      return;
    }
    const actor = getActorFromWindow(focusedInput.ownerGlobal, "AutoComplete");
    if (!actor) {
      throw new Error("Invalid autocomplete selectedIndex");
    }

    const selectedIndex = actor.selectedIndex;
    const lastProfileAutoCompleteResult = actor.lastProfileAutoCompleteResult;
    const validIndex =
      selectedIndex >= 0 &&
      selectedIndex < lastProfileAutoCompleteResult?.matchCount;
    const comment = validIndex
      ? lastProfileAutoCompleteResult.getCommentAt(selectedIndex)
      : null;

    let parsedComment = JSON.parse(comment);
    if (
      selectedIndex == -1 ||
      lastProfileAutoCompleteResult?.getStyleAt(selectedIndex) != "autofill"
    ) {
      if (
        focusedInput &&
        focusedInput == autocompleteController?.input.focusedInput
      ) {
        if (parsedComment?.fillMessageName == "FormAutofill:ClearForm") {
          // The child can do this directly.
          getActorFromWindow(focusedInput.ownerGlobal)?.clearForm();
        } else {
          // Pass focusedInput as both input arguments.
          await sendFillRequestToParent(
            "FormAutofill",
            autocompleteController.input,
            comment
          );
        }
      }
    }
  },
};
