/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.ml.logLevel",
    prefix: "ML",
  });
});

ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  TranslationsParent: "resource://gre/actors/TranslationsParent.sys.mjs",
});

/**
 * @typedef {import("../../translations/translations").WasmRecord} WasmRecord
 */

/**
 * The ML engine is in its own content process. This actor handles the
 * marshalling of the data such as the engine payload.
 */
export class MLEngineParent extends JSWindowActorParent {
  /**
   * The RemoteSettingsClient that downloads the wasm binaries.
   *
   * @type {RemoteSettingsClient | null}
   */
  static #remoteClient = null;

  /** @type {Promise<WasmRecord> | null} */
  static #wasmRecord = null;

  /**
   * The following constant controls the major version for wasm downloaded from
   * Remote Settings. When a breaking change is introduced, Nightly will have these
   * numbers incremented by one, but Beta and Release will still be on the previous
   * version. Remote Settings will ship both versions of the records, and the latest
   * asset released in that version will be used. For instance, with a major version
   * of "1", assets can be downloaded for "1.0", "1.2", "1.3beta", but assets marked
   * as "2.0", "2.1", etc will not be downloaded.
   */
  static WASM_MAJOR_VERSION = 1;

  /**
   * Remote settings isn't available in tests, so provide mocked responses.
   *
   * @param {RemoteSettingsClient} remoteClient
   */
  static mockRemoteSettings(remoteClient) {
    lazy.console.log("Mocking remote settings in MLEngineParent.");
    MLEngineParent.#remoteClient = remoteClient;
    MLEngineParent.#wasmRecord = null;
  }

  /**
   * Remove anything that could have been mocked.
   */
  static removeMocks() {
    lazy.console.log("Removing mocked remote client in MLEngineParent.");
    MLEngineParent.#remoteClient = null;
    MLEngineParent.#wasmRecord = null;
  }

  /**
   * @param {Promise<ArrayBuffer>} modelPromise
   */
  static async getEngine(modelPromise) {
    // TODO - This will be expanded in a following patch:

    // eslint-disable-next-line no-unused-vars
    const wasm = await MLEngineParent.getWasmArrayBuffer();
    // eslint-disable-next-line no-unused-vars
    const model = await modelPromise;
  }

  /**
   * @param {RemoteSettingsClient} client
   */
  static async #getWasmArrayRecord(client) {
    // Load the wasm binary from remote settings, if it hasn't been already.
    lazy.console.log(`Getting remote wasm records.`);

    /** @type {WasmRecord[]} */
    const wasmRecords = await lazy.TranslationsParent.getMaxVersionRecords(
      client,
      {
        // TODO - This record needs to be created with the engine wasm payload.
        filters: { name: "inference-engine" },
        majorVersion: MLEngineParent.WASM_MAJOR_VERSION,
      }
    );

    if (wasmRecords.length === 0) {
      // The remote settings client provides an empty list of records when there is
      // an error.
      throw new Error("Unable to get the ML engine from Remote Settings.");
    }

    if (wasmRecords.length > 1) {
      MLEngineParent.reportError(
        new Error("Expected the ml engine to only have 1 record."),
        wasmRecords
      );
    }
    const [record] = wasmRecords;
    lazy.console.log(
      `Using ${record.name}@${record.release} release version ${record.version} first released on Fx${record.fx_release}`,
      record
    );
    return record;
  }

  /**
   * Download the wasm for the ML inference engine.
   *
   * @returns {Promise<ArrayBuffer>}
   */
  static async getWasmArrayBuffer() {
    const client = MLEngineParent.#getRemoteClient();

    if (!MLEngineParent.#wasmRecord) {
      // Place the records into a promise to prevent any races.
      MLEngineParent.#wasmRecord = MLEngineParent.#getWasmArrayRecord(client);
    }

    let wasmRecord;
    try {
      wasmRecord = await MLEngineParent.#wasmRecord;
      if (!wasmRecord) {
        return Promise.reject(
          "Error: Unable to get the ML engine from Remote Settings."
        );
      }
    } catch (error) {
      MLEngineParent.#wasmRecord = null;
      throw error;
    }

    /** @type {{buffer: ArrayBuffer}} */
    const { buffer } = await client.attachments.download(wasmRecord);

    return buffer;
  }

  /**
   * Lazily initializes the RemoteSettingsClient for the downloaded wasm binary data.
   *
   * @returns {RemoteSettingsClient}
   */
  static #getRemoteClient() {
    if (MLEngineParent.#remoteClient) {
      return MLEngineParent.#remoteClient;
    }

    /** @type {RemoteSettingsClient} */
    const client = lazy.RemoteSettings("ml-wasm");

    MLEngineParent.#remoteClient = client;

    client.on("sync", async ({ data: { created, updated, deleted } }) => {
      lazy.console.log(`"sync" event for ml-wasm`, {
        created,
        updated,
        deleted,
      });

      // Remove all the deleted records.
      for (const record of deleted) {
        await client.attachments.deleteDownloaded(record);
      }

      // Remove any updated records, and download the new ones.
      for (const { old: oldRecord } of updated) {
        await client.attachments.deleteDownloaded(oldRecord);
      }

      // Do nothing for the created records.
    });

    return client;
  }
}
