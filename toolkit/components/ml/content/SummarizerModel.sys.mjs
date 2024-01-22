/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * @typedef {object} LazyImports
 * @property {typeof import("../actors/MLEngineParent.sys.mjs").MLEngineParent} MLEngineParent
 */

/** @type {LazyImports} */
const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  TranslationsParent: "resource://gre/actors/TranslationsParent.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.ml.logLevel",
    prefix: "ML",
  });
});

export class SummarizerModel {
  /**
   * The RemoteSettingsClient that downloads the summarizer model.
   *
   * @type {RemoteSettingsClient | null}
   */
  static #remoteClient = null;

  /** @type {Promise<WasmRecord> | null} */
  static #modelRecord = null;

  /**
   * The following constant controls the major version for wasm downloaded from
   * Remote Settings. When a breaking change is introduced, Nightly will have these
   * numbers incremented by one, but Beta and Release will still be on the previous
   * version. Remote Settings will ship both versions of the records, and the latest
   * asset released in that version will be used. For instance, with a major version
   * of "1", assets can be downloaded for "1.0", "1.2", "1.3beta", but assets marked
   * as "2.0", "2.1", etc will not be downloaded.
   */
  static MODEL_MAJOR_VERSION = 1;

  /**
   * Remote settings isn't available in tests, so provide mocked responses.
   */
  static mockRemoteSettings(remoteClient) {
    lazy.console.log("Mocking remote client in SummarizerModel.");
    SummarizerModel.#remoteClient = remoteClient;
    SummarizerModel.#modelRecord = null;
  }

  /**
   * Remove anything that could have been mocked.
   */
  static removeMocks() {
    lazy.console.log("Removing mocked remote client in SummarizerModel.");
    SummarizerModel.#remoteClient = null;
    SummarizerModel.#modelRecord = null;
  }
  /**
   * Download or load the model from remote settings.
   *
   * @returns {Promise<ArrayBuffer>}
   */
  static async getModel() {
    const client = SummarizerModel.#getRemoteClient();

    if (!SummarizerModel.#modelRecord) {
      // Place the records into a promise to prevent any races.
      SummarizerModel.#modelRecord = (async () => {
        // Load the wasm binary from remote settings, if it hasn't been already.
        lazy.console.log(`Getting the summarizer model record.`);

        // TODO - The getMaxVersionRecords should eventually migrated to some kind of
        // shared utility.
        const { getMaxVersionRecords } = lazy.TranslationsParent;

        /** @type {WasmRecord[]} */
        const wasmRecords = await getMaxVersionRecords(client, {
          // TODO - This record needs to be created with the engine wasm payload.
          filters: { name: "summarizer-model" },
          majorVersion: SummarizerModel.MODEL_MAJOR_VERSION,
        });

        if (wasmRecords.length === 0) {
          // The remote settings client provides an empty list of records when there is
          // an error.
          throw new Error("Unable to get the models from Remote Settings.");
        }

        if (wasmRecords.length > 1) {
          SummarizerModel.reportError(
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
      })();
    }

    try {
      /** @type {{buffer: ArrayBuffer}} */
      const { buffer } = await client.attachments.download(
        await SummarizerModel.#modelRecord
      );

      return buffer;
    } catch (error) {
      SummarizerModel.#modelRecord = null;
      throw error;
    }
  }

  /**
   * Lazily initializes the RemoteSettingsClient.
   *
   * @returns {RemoteSettingsClient}
   */
  static #getRemoteClient() {
    if (SummarizerModel.#remoteClient) {
      return SummarizerModel.#remoteClient;
    }

    /** @type {RemoteSettingsClient} */
    const client = lazy.RemoteSettings("ml-model");

    SummarizerModel.#remoteClient = client;

    client.on("sync", async ({ data: { created, updated, deleted } }) => {
      lazy.console.log(`"sync" event for ml-model`, {
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
