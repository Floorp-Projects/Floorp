/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ExtensionUtils } from "resource://gre/modules/ExtensionUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EventDispatcher: "resource://gre/modules/Messaging.sys.mjs",
});

const { ExtensionError } = ExtensionUtils;

export class BrowsingDataDelegate {
  constructor(extension) {
    this.extension = extension;
  }

  async sendRequestForResult(type, data) {
    try {
      const result = await lazy.EventDispatcher.instance.sendRequestForResult({
        type,
        extensionId: this.extension.id,
        ...data,
      });
      return result;
    } catch (errorMessage) {
      throw new ExtensionError(errorMessage);
    }
  }

  async settings() {
    return this.sendRequestForResult("GeckoView:BrowsingData:GetSettings");
  }

  async sendClear(dataType, options) {
    const { since } = options;
    return this.sendRequestForResult("GeckoView:BrowsingData:Clear", {
      dataType,
      since,
    });
  }

  // This method returns undefined for all data types that are _not_ handled by
  // this delegate.
  handleRemoval(dataType, options) {
    switch (dataType) {
      case "downloads":
      case "formData":
      case "history":
      case "passwords":
        return this.sendClear(dataType, options);

      default:
        return undefined;
    }
  }
}
