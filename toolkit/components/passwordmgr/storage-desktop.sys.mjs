/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { LoginManagerStorage_json } from "resource://gre/modules/storage-json.sys.mjs";

export class LoginManagerStorage extends LoginManagerStorage_json {
  static #storage = null;

  static create(callback) {
    if (!LoginManagerStorage.#storage) {
      LoginManagerStorage.#storage = new LoginManagerStorage();
      LoginManagerStorage.#storage.initialize().then(callback);
    } else if (callback) {
      callback();
    }

    return LoginManagerStorage.#storage;
  }
}
