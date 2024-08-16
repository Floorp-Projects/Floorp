/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * An actor unit test for testing RemoteSettings update behavior. This uses the
 * recommendations from:
 *
 * https://firefox-source-docs.mozilla.org/services/settings/index.html#unit-tests
 */
add_task(async function test_translations_actor_sync_update_wasm() {
  const { remoteClients, cleanup } = await setupActorTest({
    autoDownloadFromRemoteSettings: true,
    languagePairs: LANGUAGE_PAIRS,
  });

  const decoder = new TextDecoder();
  const { bergamotWasmArrayBuffer } =
    await TranslationsParent.getTranslationsEnginePayload("en", "es");

  is(
    decoder.decode(bergamotWasmArrayBuffer),
    `Mocked download: test-translation-wasm bergamot-translator ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.0`,
    `The version ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.0 model is downloaded.`
  );

  const newWasmRecord = createWasmRecord();
  const [oldWasmRecord] = await TranslationsParent.getMaxVersionRecords(
    remoteClients.translationsWasm.client,
    {
      filters: { name: "bergamot-translator" },
      majorVersion: TranslationsParent.BERGAMOT_MAJOR_VERSION,
    }
  );

  newWasmRecord.id = oldWasmRecord.id;
  newWasmRecord.version = `${TranslationsParent.BERGAMOT_MAJOR_VERSION}.1`;

  await modifyRemoteSettingsRecords(remoteClients.translationsWasm.client, {
    recordsToCreate: [newWasmRecord],
    expectedUpdatedRecordsCount: 1,
  });

  const { bergamotWasmArrayBuffer: updatedBergamotWasmArrayBuffer } =
    await TranslationsParent.getTranslationsEnginePayload("en", "es");

  is(
    decoder.decode(updatedBergamotWasmArrayBuffer),
    `Mocked download: test-translation-wasm bergamot-translator ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.1`,
    `The version ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.1 model is downloaded.`
  );

  return cleanup();
});

/**
 * An actor unit test for testing RemoteSettings delete behavior.
 */
add_task(async function test_translations_actor_sync_delete_wasm() {
  const { remoteClients, cleanup } = await setupActorTest({
    autoDownloadFromRemoteSettings: true,
    languagePairs: LANGUAGE_PAIRS,
  });

  const decoder = new TextDecoder();
  const { bergamotWasmArrayBuffer } =
    await TranslationsParent.getTranslationsEnginePayload("en", "es");

  is(
    decoder.decode(bergamotWasmArrayBuffer),
    `Mocked download: test-translation-wasm bergamot-translator ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.0`,
    `The version ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.0 model is downloaded.`
  );

  const [oldWasmRecord] = await TranslationsParent.getMaxVersionRecords(
    remoteClients.translationsWasm.client,
    {
      filters: { name: "bergamot-translator" },
      majorVersion: TranslationsParent.BERGAMOT_MAJOR_VERSION,
    }
  );

  await modifyRemoteSettingsRecords(remoteClients.translationsWasm.client, {
    recordsToDelete: [oldWasmRecord],
    expectedDeletedRecordsCount: 1,
  });

  let errorMessage;
  await TranslationsParent.getTranslationsEnginePayload("en", "es").catch(
    error => {
      errorMessage = error?.message;
    }
  );

  is(
    errorMessage,
    "Unable to get the bergamot translator from Remote Settings.",
    "The WASM was successfully removed."
  );

  return cleanup();
});

/**
 * An actor unit test for testing creating a new record has a higher minor version than an existing record of the same kind.
 */
add_task(
  async function test_translations_actor_sync_create_wasm_higher_minor_version() {
    const { remoteClients, cleanup } = await setupActorTest({
      autoDownloadFromRemoteSettings: true,
      languagePairs: LANGUAGE_PAIRS,
    });

    const decoder = new TextDecoder();
    const { bergamotWasmArrayBuffer } =
      await TranslationsParent.getTranslationsEnginePayload("en", "es");

    is(
      decoder.decode(bergamotWasmArrayBuffer),
      `Mocked download: test-translation-wasm bergamot-translator ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.0`,
      `The version ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.0 model is downloaded.`
    );

    const newWasmRecord = createWasmRecord();
    newWasmRecord.version = `${TranslationsParent.BERGAMOT_MAJOR_VERSION}.1`;

    await modifyRemoteSettingsRecords(remoteClients.translationsWasm.client, {
      recordsToCreate: [newWasmRecord],
      expectedCreatedRecordsCount: 1,
    });

    const { bergamotWasmArrayBuffer: updatedBergamotWasmArrayBuffer } =
      await TranslationsParent.getTranslationsEnginePayload("en", "es");

    is(
      decoder.decode(updatedBergamotWasmArrayBuffer),
      `Mocked download: test-translation-wasm bergamot-translator ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.1`,
      `The version ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.1 model is downloaded.`
    );

    return cleanup();
  }
);

/**
 * An actor unit test for testing creating a new record has a higher major version than an existing record of the same kind.
 */
add_task(
  async function test_translations_actor_sync_create_wasm_higher_major_version() {
    const { remoteClients, cleanup } = await setupActorTest({
      autoDownloadFromRemoteSettings: true,
      languagePairs: LANGUAGE_PAIRS,
    });

    const decoder = new TextDecoder();
    const { bergamotWasmArrayBuffer } =
      await TranslationsParent.getTranslationsEnginePayload("en", "es");

    is(
      decoder.decode(bergamotWasmArrayBuffer),
      `Mocked download: test-translation-wasm bergamot-translator ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.0`,
      `The version ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.0 model is downloaded.`
    );

    const newWasmRecord = createWasmRecord();
    newWasmRecord.version = `${
      TranslationsParent.BERGAMOT_MAJOR_VERSION + 1
    }.0`;

    await modifyRemoteSettingsRecords(remoteClients.translationsWasm.client, {
      recordsToCreate: [newWasmRecord],
      expectedCreatedRecordsCount: 1,
    });

    const { bergamotWasmArrayBuffer: updatedBergamotWasmArrayBuffer } =
      await TranslationsParent.getTranslationsEnginePayload("en", "es");

    is(
      decoder.decode(updatedBergamotWasmArrayBuffer),
      `Mocked download: test-translation-wasm bergamot-translator ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.0`,
      `The version ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.0 model is still downloaded.`
    );

    return cleanup();
  }
);

/**
 * An actor unit test for testing removing a record that has a higher minor version than another record, ensuring
 * that the models roll back to the previous version.
 */
add_task(async function test_translations_actor_sync_rollback_wasm() {
  const { remoteClients, cleanup } = await setupActorTest({
    autoDownloadFromRemoteSettings: true,
    languagePairs: LANGUAGE_PAIRS,
  });

  const decoder = new TextDecoder();
  const { bergamotWasmArrayBuffer } =
    await TranslationsParent.getTranslationsEnginePayload("en", "es");

  is(
    decoder.decode(bergamotWasmArrayBuffer),
    `Mocked download: test-translation-wasm bergamot-translator ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.0`,
    `The version ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.0 model is downloaded.`
  );

  const newWasmRecord = createWasmRecord();
  newWasmRecord.version = `${TranslationsParent.BERGAMOT_MAJOR_VERSION}.1`;

  await modifyRemoteSettingsRecords(remoteClients.translationsWasm.client, {
    recordsToCreate: [newWasmRecord],
    expectedCreatedRecordsCount: 1,
  });

  const { bergamotWasmArrayBuffer: updatedBergamotWasmArrayBuffer } =
    await TranslationsParent.getTranslationsEnginePayload("en", "es");

  is(
    decoder.decode(updatedBergamotWasmArrayBuffer),
    `Mocked download: test-translation-wasm bergamot-translator ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.1`,
    `The version ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.1 model is downloaded.`
  );

  await modifyRemoteSettingsRecords(remoteClients.translationsWasm.client, {
    recordsToDelete: [newWasmRecord],
    expectedDeletedRecordsCount: 1,
  });

  const { bergamotWasmArrayBuffer: rolledBackBergamotWasmArrayBuffer } =
    await TranslationsParent.getTranslationsEnginePayload("en", "es");

  is(
    decoder.decode(rolledBackBergamotWasmArrayBuffer),
    `Mocked download: test-translation-wasm bergamot-translator ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.0`,
    `The version ${TranslationsParent.BERGAMOT_MAJOR_VERSION}.0 model is downloaded.`
  );

  return cleanup();
});
