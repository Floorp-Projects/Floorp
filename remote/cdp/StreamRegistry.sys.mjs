/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  generateUUID: "chrome://remote/content/shared/UUID.sys.mjs",
  UnsupportedError: "chrome://remote/content/cdp/Error.sys.mjs",
});

export class Stream {
  #path;
  #offset;
  #length;

  constructor(path) {
    this.#path = path;
    this.#offset = 0;
    this.#length = null;
  }

  async destroy() {
    await IOUtils.remove(this.#path);
  }

  async seek(seekTo) {
    // To keep compatibility with Chrome clip invalid offsets
    this.#offset = Math.max(0, Math.min(seekTo, await this.length()));
  }

  async readBytes(count) {
    const bytes = await IOUtils.read(this.#path, {
      offset: this.#offset,
      maxBytes: count,
    });
    this.#offset += bytes.length;
    return bytes;
  }

  async available() {
    const length = await this.length();
    return length - this.#offset;
  }

  async length() {
    if (this.#length === null) {
      const info = await IOUtils.stat(this.#path);
      this.#length = info.size;
    }

    return this.#length;
  }

  get path() {
    return this.#path;
  }
}

export class StreamRegistry {
  constructor() {
    // handle => stream
    this.streams = new Map();

    // Register an async shutdown blocker to ensure all open IO streams are
    // closed, and remaining temporary files removed. Needs to happen before
    // IOUtils has been shutdown.
    IOUtils.profileBeforeChange.addBlocker(
      "Remote Agent: Clean-up of open streams",
      async () => {
        await this.destructor();
      }
    );
  }

  async destructor() {
    for (const stream of this.streams.values()) {
      await stream.destroy();
    }

    this.streams.clear();
  }

  /**
   * Add a new stream to the registry.
   *
   * @param {Stream} stream
   *      The stream to use.
   *
   * @returns {string}
   *     Stream handle (uuid)
   */
  add(stream) {
    if (!(stream instanceof Stream)) {
      // Bug 1602731 - Implement support for blob
      throw new lazy.UnsupportedError(`Unknown stream type for ${stream}`);
    }

    const handle = lazy.generateUUID();

    this.streams.set(handle, stream);
    return handle;
  }

  /**
   * Get a stream from the registry.
   *
   * @param {string} handle
   *      Handle of the stream to retrieve.
   *
   * @returns {Stream}
   *      The requested stream.
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
   * @returns {boolean}
   *     true if successfully removed
   */
  async remove(handle) {
    const stream = this.get(handle);
    await stream.destroy();

    return this.streams.delete(handle);
  }
}
