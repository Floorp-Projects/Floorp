/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["StreamRegistry"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  OS: "resource://gre/modules/osfile.jsm",

  UnsupportedError: "chrome://remote/content/cdp/Error.jsm",
});

class StreamRegistry {
  constructor() {
    // handle => stream
    this.streams = new Map();

    // Register an async shutdown blocker to ensure all open IO streams are
    // closed, and remaining temporary files removed. Needs to happen before
    // OS.File has been shutdown.
    lazy.AsyncShutdown.profileBeforeChange.addBlocker(
      "Remote Agent: Clean-up of open streams",
      async () => {
        await this.destructor();
      }
    );
  }

  async destructor() {
    for (const stream of this.streams.values()) {
      await this._discard(stream);
    }

    this.streams.clear();
  }

  async _discard(stream) {
    if (stream instanceof lazy.OS.File) {
      let fileInfo;

      // Also remove the temporary file
      try {
        fileInfo = await stream.stat();

        stream.close();
        await lazy.OS.File.remove(fileInfo.path, { ignoreAbsent: true });
      } catch (e) {
        console.error(`Failed to remove ${fileInfo?.path}: ${e.message}`);
      }
    }
  }

  /**
   * Add a new stream to the registry.
   *
   * @param {OS.File} stream
   *      Instance of the stream to add.
   *
   * @return {string}
   *     Stream handle (uuid)
   */
  add(stream) {
    let handle;

    if (stream instanceof lazy.OS.File) {
      handle = Services.uuid
        .generateUUID()
        .toString()
        .slice(1, -1);
    } else {
      // Bug 1602731 - Implement support for blob
      throw new lazy.UnsupportedError(`Unknown stream type for ${stream}`);
    }

    this.streams.set(handle, stream);

    return handle;
  }

  /**
   * Get a stream from the registry.
   *
   * @param {string} handle
   *      Handle of the stream to retrieve.
   *
   * @return {OS.File}
   *     Requested stream
   */
  get(handle) {
    const stream = this.streams.get(handle);

    if (!stream) {
      throw new TypeError(`Invalid stream handle`);
    }

    return stream;
  }

  /**
   * Remove a stream from the registry.
   *
   * @param {string} handle
   *      Handle of the stream to remove.
   *
   * @return {boolean}
   *     true if successfully removed
   */
  async remove(handle) {
    const stream = this.get(handle);
    await this._discard(stream);

    return this.streams.delete(handle);
  }
}
