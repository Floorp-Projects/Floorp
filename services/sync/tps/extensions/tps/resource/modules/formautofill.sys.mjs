/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This is a JavaScript module (JSM) to be imported via
 * ChromeUtils.import() and acts as a singleton. Only the following
 * listed symbols will exposed on import, and only when and where imported.
 */

import { Logger } from "resource://tps/logger.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  OSKeyStore: "resource://gre/modules/OSKeyStore.sys.mjs",
  formAutofillStorage: "resource://autofill/FormAutofillStorage.sys.mjs",
});

class FormAutofillBase {
  constructor(props, subStorageName, fields) {
    this._subStorageName = subStorageName;
    this._fields = fields;

    this.props = {};
    this.updateProps = null;
    if ("changes" in props) {
      this.updateProps = props.changes;
    }
    for (const field of this._fields) {
      this.props[field] = field in props ? props[field] : null;
    }
  }

  async getStorage() {
    await lazy.formAutofillStorage.initialize();
    return lazy.formAutofillStorage[this._subStorageName];
  }

  async Create() {
    const storage = await this.getStorage();
    await storage.add(this.props);
  }

  async Find() {
    const storage = await this.getStorage();
    return storage._data.find(entry =>
      this._fields.every(field => entry[field] === this.props[field])
    );
  }

  async Update() {
    const storage = await this.getStorage();
    const { guid } = await this.Find();
    await storage.update(guid, this.updateProps, true);
  }

  async Remove() {
    const storage = await this.getStorage();
    const { guid } = await this.Find();
    storage.remove(guid);
  }
}

async function DumpStorage(subStorageName) {
  await lazy.formAutofillStorage.initialize();
  Logger.logInfo(`\ndumping ${subStorageName} list\n`, true);
  const entries = lazy.formAutofillStorage[subStorageName]._data;
  for (const entry of entries) {
    Logger.logInfo(JSON.stringify(entry), true);
  }
  Logger.logInfo(`\n\nend ${subStorageName} list\n`, true);
}

const ADDRESS_FIELDS = [
  "given-name",
  "additional-name",
  "family-name",
  "organization",
  "street-address",
  "address-level2",
  "address-level1",
  "postal-code",
  "country",
  "tel",
  "email",
];

export class Address extends FormAutofillBase {
  constructor(props) {
    super(props, "addresses", ADDRESS_FIELDS);
  }
}

export async function DumpAddresses() {
  await DumpStorage("addresses");
}

const CREDIT_CARD_FIELDS = [
  "cc-name",
  "cc-number",
  "cc-exp-month",
  "cc-exp-year",
];

export class CreditCard extends FormAutofillBase {
  constructor(props) {
    super(props, "creditCards", CREDIT_CARD_FIELDS);
  }

  async Find() {
    const storage = await this.getStorage();
    await Promise.all(
      storage._data.map(
        async entry =>
          (entry["cc-number"] = await lazy.OSKeyStore.decrypt(
            entry["cc-number-encrypted"]
          ))
      )
    );
    return storage._data.find(entry => {
      return this._fields.every(field => entry[field] === this.props[field]);
    });
  }
}

export async function DumpCreditCards() {
  await DumpStorage("creditCards");
}
