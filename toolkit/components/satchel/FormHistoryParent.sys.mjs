/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FirefoxRelayTelemetry } from "resource://gre/modules/FirefoxRelayTelemetry.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FirefoxRelay: "resource://gre/modules/FirefoxRelay.sys.mjs",
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
});

export class FormHistoryParent extends JSWindowActorParent {
  receiveMessage({ name, data }) {
    switch (name) {
      case "FormHistory:FormSubmitEntries":
        this.#onFormSubmitEntries(data);
        break;

      case "FormHistory:AutoCompleteSearchAsync":
        return this.#onAutoCompleteSearch(data);

      case "FormHistory:RemoveEntry":
        this.#onRemoveEntry(data);
        break;

      case "PasswordManager:offerRelayIntegration": {
        FirefoxRelayTelemetry.recordRelayOfferedEvent(
          "clicked",
          data.telemetry.flowId,
          data.telemetry.scenarioName
        );
        return this.#offerRelayIntegration();
      }

      case "PasswordManager:generateRelayUsername": {
        FirefoxRelayTelemetry.recordRelayUsernameFilledEvent(
          "clicked",
          data.telemetry.flowId
        );
        return this.#generateRelayUsername();
      }
    }

    return undefined;
  }

  #onFormSubmitEntries(entries) {
    const changes = entries.map(entry => ({
      op: "bump",
      fieldname: entry.name,
      value: entry.value,
    }));

    lazy.FormHistory.update(changes);
  }

  get formOrigin() {
    return lazy.LoginHelper.getLoginOrigin(
      this.manager.documentPrincipal?.originNoSuffix
    );
  }

  async #onAutoCompleteSearch({ searchString, params, scenarioName }) {
    const formHistoryPromise = lazy.FormHistory.getAutoCompleteResults(
      searchString,
      params
    );

    const relayPromise = lazy.FirefoxRelay.autocompleteItemsAsync({
      formOrigin: this.formOrigin,
      scenarioName,
      hasInput: !!searchString.length,
    });
    const [formHistoryEntries, externalEntries] = await Promise.all([
      formHistoryPromise,
      relayPromise,
    ]);

    return { formHistoryEntries, externalEntries };
  }

  #onRemoveEntry({ inputName, value, guid }) {
    lazy.FormHistory.update({
      op: "remove",
      fieldname: inputName,
      value,
      guid,
    });
  }

  getRootBrowser() {
    return this.browsingContext.topFrameElement;
  }

  async #offerRelayIntegration() {
    const browser = this.getRootBrowser();
    return lazy.FirefoxRelay.offerRelayIntegration(browser, this.formOrigin);
  }

  async #generateRelayUsername() {
    const browser = this.getRootBrowser();
    return lazy.FirefoxRelay.generateUsername(browser, this.formOrigin);
  }
}
