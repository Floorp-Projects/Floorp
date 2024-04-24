/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { DataSourceBase } from "resource://gre/modules/megalist/aggregator/datasources/DataSourceBase.sys.mjs";
import { CreditCardRecord } from "resource://gre/modules/shared/CreditCardRecord.sys.mjs";
import { formAutofillStorage } from "resource://autofill/FormAutofillStorage.sys.mjs";
import { OSKeyStore } from "resource://gre/modules/OSKeyStore.sys.mjs";

async function decryptCard(card) {
  if (card["cc-number-encrypted"] && !card["cc-number-decrypted"]) {
    try {
      card["cc-number-decrypted"] = await OSKeyStore.decrypt(
        card["cc-number-encrypted"],
        false
      );
      card["cc-number"] = card["cc-number-decrypted"];
    } catch (e) {
      console.error(e);
    }
  }
}

async function updateCard(card, field, value) {
  try {
    await decryptCard(card);
    const newCard = {
      ...card,
      [field]: value ?? "",
    };
    formAutofillStorage.INTERNAL_FIELDS.forEach(name => delete newCard[name]);
    formAutofillStorage.creditCards.VALID_COMPUTED_FIELDS.forEach(
      name => delete newCard[name]
    );
    delete newCard["cc-number-decrypted"];
    CreditCardRecord.normalizeFields(newCard);

    if (card.guid) {
      await formAutofillStorage.creditCards.update(card.guid, newCard);
    } else {
      await formAutofillStorage.creditCards.add(newCard);
    }
  } catch (error) {
    //todo
    console.error("failed to modify credit card", error);
    return false;
  }

  return true;
}

/**
 * Data source for Bank Cards.
 *
 * Each card is represented by 3 lines: card number, expiration date and holder name.
 *
 * Protypes are used to reduce memory need because for different records
 * similar lines will differ in values only.
 */
export class BankCardDataSource extends DataSourceBase {
  #cardNumberPrototype;
  #expirationPrototype;
  #holderNamePrototype;
  #cardsDisabledMessage;
  #enabled;
  #header;

  constructor(...args) {
    super(...args);
    // Wait for Fluent to provide strings before loading data
    this.localizeStrings({
      headerLabel: "payments-section-label",
      numberLabel: "card-number-label",
      expirationLabel: "card-expiration-label",
      holderLabel: "card-holder-label",
      cardsDisabled: "payments-disabled",
    }).then(strings => {
      const copyCommand = { id: "Copy", label: "command-copy" };
      const editCommand = { id: "Edit", label: "command-edit", verify: true };
      const deleteCommand = {
        id: "Delete",
        label: "command-delete",
        verify: true,
      };
      this.#cardsDisabledMessage = strings.cardsDisabled;
      this.#header = this.createHeaderLine(strings.headerLabel);
      this.#header.commands.push({
        id: "Create",
        label: "payments-command-create",
      });
      this.#cardNumberPrototype = this.prototypeDataLine({
        label: { value: strings.numberLabel },
        concealed: { value: true, writable: true },
        start: { value: true },
        value: {
          async get() {
            if (this.isEditing()) {
              return this.editingValue;
            }

            if (this.concealed) {
              return (
                "••••••••" +
                this.record["cc-number"].replaceAll("*", "").substr(-4)
              );
            }

            await decryptCard(this.record);
            return this.record["cc-number-decrypted"];
          },
        },
        valueIcon: {
          get() {
            const typeToImage = {
              amex: "third-party/cc-logo-amex.png",
              cartebancaire: "third-party/cc-logo-cartebancaire.png",
              diners: "third-party/cc-logo-diners.svg",
              discover: "third-party/cc-logo-discover.png",
              jcb: "third-party/cc-logo-jcb.svg",
              mastercard: "third-party/cc-logo-mastercard.svg",
              mir: "third-party/cc-logo-mir.svg",
              unionpay: "third-party/cc-logo-unionpay.svg",
              visa: "third-party/cc-logo-visa.svg",
            };
            return (
              "chrome://formautofill/content/" +
              (typeToImage[this.record["cc-type"]] ??
                "icon-credit-card-generic.svg")
            );
          },
        },
        commands: {
          *value() {
            if (this.concealed) {
              yield { id: "Reveal", label: "command-reveal", verify: true };
            } else {
              yield { id: "Conceal", label: "command-conceal" };
            }
            yield { ...copyCommand, verify: true };
            yield editCommand;
            yield "-";
            yield deleteCommand;
          },
        },
        executeReveal: {
          value() {
            this.concealed = false;
            this.refreshOnScreen();
          },
        },
        executeConceal: {
          value() {
            this.concealed = true;
            this.refreshOnScreen();
          },
        },
        executeCopy: {
          async value() {
            await decryptCard(this.record);
            this.copyToClipboard(this.record["cc-number-decrypted"]);
          },
        },
        executeEdit: {
          async value() {
            await decryptCard(this.record);
            this.editingValue = this.record["cc-number-decrypted"] ?? "";
            this.refreshOnScreen();
          },
        },
        executeSave: {
          async value(value) {
            if (updateCard(this.record, "cc-number", value)) {
              this.executeCancel();
            }
          },
        },
      });
      this.#expirationPrototype = this.prototypeDataLine({
        label: { value: strings.expirationLabel },
        value: {
          get() {
            return `${this.record["cc-exp-month"]}/${this.record["cc-exp-year"]}`;
          },
        },
        commands: {
          value: [copyCommand, editCommand, "-", deleteCommand],
        },
      });
      this.#holderNamePrototype = this.prototypeDataLine({
        label: { value: strings.holderLabel },
        end: { value: true },
        value: {
          get() {
            return this.editingValue ?? this.record["cc-name"];
          },
        },
        commands: {
          value: [copyCommand, editCommand, "-", deleteCommand],
        },
        executeEdit: {
          value() {
            this.editingValue = this.record["cc-name"] ?? "";
            this.refreshOnScreen();
          },
        },
        executeSave: {
          async value(value) {
            if (updateCard(this.record, "cc-name", value)) {
              this.executeCancel();
            }
          },
        },
      });

      Services.obs.addObserver(this, "formautofill-storage-changed");
      Services.prefs.addObserver(
        "extensions.formautofill.creditCards.enabled",
        this
      );
      this.#reloadDataSource();
    });
  }

  /**
   * Enumerate all the lines provided by this data source.
   *
   * @param {string} searchText used to filter data
   */
  *enumerateLines(searchText) {
    if (this.#enabled === undefined) {
      // Async Fluent API makes it possible to have data source waiting
      // for the localized strings, which can be detected by undefined in #enabled.
      return;
    }

    yield this.#header;
    if (this.#header.collapsed || !this.#enabled) {
      return;
    }

    const stats = { count: 0, total: 0 };
    searchText = searchText.toUpperCase();
    yield* this.enumerateLinesForMatchingRecords(
      searchText,
      stats,
      card =>
        (card["cc-number-decrypted"] || card["cc-number"])
          .toUpperCase()
          .includes(searchText) ||
        `${card["cc-exp-month"]}/${card["cc-exp-year"]}`
          .toUpperCase()
          .includes(searchText) ||
        card["cc-name"]?.toUpperCase().includes(searchText)
    );

    this.formatMessages({
      id:
        stats.count == stats.total
          ? "payments-count"
          : "payments-filtered-count",
      args: stats,
    }).then(([headerLabel]) => {
      this.#header.value = headerLabel;
    });
  }

  /**
   * Sync lines array with the actual data source.
   * This function reads all cards from the storage, adds or updates lines and
   * removes lines for the removed cards.
   */
  async #reloadDataSource() {
    this.#enabled = Services.prefs.getBoolPref(
      "extensions.formautofill.creditCards.enabled"
    );
    if (!this.#enabled) {
      this.#reloadEmptyDataSource();
      return;
    }

    await formAutofillStorage.initialize();
    const cards = await formAutofillStorage.creditCards.getAll();
    this.beforeReloadingDataSource();
    cards.forEach(card => {
      const lineId = `${card["cc-name"]}:${card.guid}`;

      this.addOrUpdateLine(card, lineId + "0", this.#cardNumberPrototype);
      this.addOrUpdateLine(card, lineId + "1", this.#expirationPrototype);
      this.addOrUpdateLine(card, lineId + "2", this.#holderNamePrototype);
    });
    this.afterReloadingDataSource();
  }

  #reloadEmptyDataSource() {
    this.lines.length = 0;
    //todo: user can enable credit cards by activating header line
    this.#header.value = this.#cardsDisabledMessage;
    this.refreshAllLinesOnScreen();
  }

  observe(_subj, topic, message) {
    if (
      topic == "formautofill-storage-changed" ||
      message == "extensions.formautofill.creditCards.enabled"
    ) {
      this.#reloadDataSource();
    }
  }
}
