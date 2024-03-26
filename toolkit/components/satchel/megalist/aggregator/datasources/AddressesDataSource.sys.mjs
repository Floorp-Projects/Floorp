/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { DataSourceBase } from "resource://gre/modules/megalist/aggregator/datasources/DataSourceBase.sys.mjs";
import { formAutofillStorage } from "resource://autofill/FormAutofillStorage.sys.mjs";

async function updateAddress(address, field, value) {
  try {
    const newAddress = {
      ...address,
      [field]: value ?? "",
    };

    formAutofillStorage.INTERNAL_FIELDS.forEach(
      name => delete newAddress[name]
    );
    formAutofillStorage.addresses.VALID_COMPUTED_FIELDS.forEach(
      name => delete newAddress[name]
    );

    if (address.guid) {
      await formAutofillStorage.addresses.update(address.guid, newAddress);
    } else {
      await formAutofillStorage.addresses.add(newAddress);
    }
  } catch (error) {
    //todo
    console.error("failed to modify address", error);
    return false;
  }

  return true;
}

/**
 * Data source for Addresses.
 *
 */

export class AddressesDataSource extends DataSourceBase {
  #namePrototype;
  #organizationPrototype;
  #streetAddressPrototype;
  #addressLevelOnePrototype;
  #addressLevelTwoPrototype;
  #addressLevelThreePrototype;
  #postalCodePrototype;
  #countryPrototype;
  #phonePrototype;
  #emailPrototype;

  #addressesDisabledMessage;
  #enabled;
  #header;

  constructor(...args) {
    super(...args);
    this.formatMessages(
      "addresses-section-label",
      "address-name-label",
      "address-phone-label",
      "address-email-label",
      "command-copy",
      "addresses-disabled",
      "command-delete",
      "command-edit",
      "addresses-command-create"
    ).then(
      ([
        headerLabel,
        nameLabel,
        phoneLabel,
        emailLabel,
        copyLabel,
        addressesDisabled,
        deleteLabel,
        editLabel,
        createLabel,
      ]) => {
        const copyCommand = { id: "Copy", label: copyLabel };
        const editCommand = { id: "Edit", label: editLabel };
        const deleteCommand = { id: "Delete", label: deleteLabel };
        this.#addressesDisabledMessage = addressesDisabled;
        this.#header = this.createHeaderLine(headerLabel);
        this.#header.commands.push({ id: "Create", label: createLabel });

        let self = this;

        function prototypeLine(label, key, options = {}) {
          return self.prototypeDataLine({
            label: { value: label },
            value: {
              get() {
                return this.editingValue ?? this.record[key];
              },
            },
            commands: {
              value: [copyCommand, editCommand, "-", deleteCommand],
            },
            executeEdit: {
              value() {
                this.editingValue = this.record[key] ?? "";
                this.refreshOnScreen();
              },
            },
            executeSave: {
              async value(value) {
                if (await updateAddress(this.record, key, value)) {
                  this.executeCancel();
                }
              },
            },
            ...options,
          });
        }

        this.#namePrototype = prototypeLine(nameLabel, "name", {
          start: { value: true },
        });
        this.#organizationPrototype = prototypeLine(
          "Organization",
          "organization"
        );
        this.#streetAddressPrototype = prototypeLine(
          "Street Address",
          "street-address"
        );
        this.#addressLevelThreePrototype = prototypeLine(
          "Neighbourhood",
          "address-level3"
        );
        this.#addressLevelTwoPrototype = prototypeLine(
          "City",
          "address-level2"
        );
        this.#addressLevelOnePrototype = prototypeLine(
          "Province",
          "address-level1"
        );
        this.#postalCodePrototype = prototypeLine("Postal Code", "postal-code");
        this.#countryPrototype = prototypeLine("Country", "country");
        this.#phonePrototype = prototypeLine(phoneLabel, "tel");
        this.#emailPrototype = prototypeLine(emailLabel, "email", {
          end: { value: true },
        });

        Services.obs.addObserver(this, "formautofill-storage-changed");
        Services.prefs.addObserver(
          "extensions.formautofill.addresses.enabled",
          this
        );
        this.#reloadDataSource();
      }
    );
  }

  async #reloadDataSource() {
    this.#enabled = Services.prefs.getBoolPref(
      "extensions.formautofill.addresses.enabled"
    );
    if (!this.#enabled) {
      this.#reloadEmptyDataSource();
      return;
    }

    await formAutofillStorage.initialize();
    const addresses = await formAutofillStorage.addresses.getAll();
    this.beforeReloadingDataSource();
    addresses.forEach(address => {
      const lineId = `${address.name}:${address.tel}`;

      this.addOrUpdateLine(address, lineId + "0", this.#namePrototype);
      this.addOrUpdateLine(address, lineId + "1", this.#organizationPrototype);
      this.addOrUpdateLine(address, lineId + "2", this.#streetAddressPrototype);
      this.addOrUpdateLine(
        address,
        lineId + "3",
        this.#addressLevelThreePrototype
      );
      this.addOrUpdateLine(
        address,
        lineId + "4",
        this.#addressLevelTwoPrototype
      );
      this.addOrUpdateLine(
        address,
        lineId + "5",
        this.#addressLevelOnePrototype
      );
      this.addOrUpdateLine(address, lineId + "6", this.#postalCodePrototype);
      this.addOrUpdateLine(address, lineId + "7", this.#countryPrototype);
      this.addOrUpdateLine(address, lineId + "8", this.#phonePrototype);
      this.addOrUpdateLine(address, lineId + "9", this.#emailPrototype);
    });
    this.afterReloadingDataSource();
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

    const stats = { total: 0, count: 0 };
    searchText = searchText.toUpperCase();
    yield* this.enumerateLinesForMatchingRecords(searchText, stats, address =>
      [
        "name",
        "organization",
        "street-address",
        "address-level3",
        "address-level2",
        "address-level1",
        "postal-code",
        "country",
        "tel",
        "email",
      ].some(key => address[key]?.toUpperCase().includes(searchText))
    );

    this.formatMessages({
      id:
        stats.count == stats.total
          ? "addresses-count"
          : "addresses-filtered-count",
      args: stats,
    }).then(([headerLabel]) => {
      this.#header.value = headerLabel;
    });
  }

  #reloadEmptyDataSource() {
    this.lines.length = 0;
    this.#header.value = this.#addressesDisabledMessage;
    this.refreshAllLinesOnScreen();
  }

  observe(_subj, topic, message) {
    if (
      topic == "formautofill-storage-changed" ||
      message == "extensions.formautofill.addresses.enabled"
    ) {
      this.#reloadDataSource();
    }
  }
}
