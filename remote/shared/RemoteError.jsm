/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["RemoteError"];

/**
 * Base class for all remote protocol errors.
 */
class RemoteError extends Error {
  get isRemoteError() {
    return true;
  }

  /**
   * Convert to a serializable object. Should be implemented by subclasses.
   */
  toJSON() {
    throw new Error("Not implemented");
  }
}
