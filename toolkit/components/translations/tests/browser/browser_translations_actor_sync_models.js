/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * An actor unit test for testing RemoteSettings update behavior. This uses the
 * recommendations from:
 *
 * https://firefox-source-docs.mozilla.org/services/settings/index.html#unit-tests
 */
add_task(async function test_translations_actor_sync_update() {
  const { remoteClients, cleanup } = await setupActorTest({
    autoDownloadFromRemoteSettings: true,
    languagePairs: [
      { fromLang: "en", toLang: "es" },
      { fromLang: "es", toLang: "en" },
    ],
  });

  const decoder = new TextDecoder();
  const modelsPromise = TranslationsParent.getLanguageTranslationModelFiles(
    "en",
    "es"
  );

  await remoteClients.translationModels.resolvePendingDownloads(
    FILES_PER_LANGUAGE_PAIR
  );

  const oldModels = await modelsPromise;

  is(
    decoder.decode(oldModels.model.buffer),
    "Mocked download: test-translation-models model.enes.intgemm.alphas.bin 1.0",
    "The version 1.0 model is downloaded."
  );

  const newModelRecords = createRecordsForLanguagePair("en", "es");
  for (const newModelRecord of newModelRecords) {
    newModelRecord.version = "1.1";
  }

  info('Emitting a remote client "sync" event with an updated record.');
  await remoteClients.translationModels.client.emit("sync", {
    data: {
      created: [],
      updated: newModelRecords.map(newRecord => ({
        old: oldModels[newRecord.fileType].record,
        new: newRecord,
      })),
      deleted: [],
    },
  });

  const updatedModelsPromise =
    TranslationsParent.getLanguageTranslationModelFiles("en", "es");

  await remoteClients.translationModels.resolvePendingDownloads(
    FILES_PER_LANGUAGE_PAIR
  );

  const { model: updatedModel } = await updatedModelsPromise;

  is(
    decoder.decode(updatedModel.buffer),
    "Mocked download: test-translation-models model.enes.intgemm.alphas.bin 1.1",
    "The version 1.1 model is downloaded."
  );

  return cleanup();
});

/**
 * An actor unit test for testing RemoteSettings delete behavior.
 */
add_task(async function test_translations_actor_sync_delete() {
  const { remoteClients, cleanup } = await setupActorTest({
    autoDownloadFromRemoteSettings: true,
    languagePairs: [
      { fromLang: "en", toLang: "es" },
      { fromLang: "es", toLang: "en" },
    ],
  });

  const decoder = new TextDecoder();
  const modelsPromise = TranslationsParent.getLanguageTranslationModelFiles(
    "en",
    "es"
  );

  await remoteClients.translationModels.resolvePendingDownloads(
    FILES_PER_LANGUAGE_PAIR
  );

  const { model } = await modelsPromise;

  is(
    decoder.decode(model.buffer),
    "Mocked download: test-translation-models model.enes.intgemm.alphas.bin 1.0",
    "The version 1.0 model is downloaded."
  );

  info('Emitting a remote client "sync" event with a deleted record.');
  await remoteClients.translationModels.client.emit("sync", {
    data: {
      created: [],
      updated: [],
      deleted: [model.record],
    },
  });

  let errorMessage;
  await TranslationsParent.getLanguageTranslationModelFiles("en", "es").catch(
    error => {
      errorMessage = error?.message;
    }
  );

  is(
    errorMessage,
    'No model file was found for "en" to "es."',
    "The model was successfully removed."
  );

  return cleanup();
});

/**
 * An actor unit test for testing RemoteSettings creation behavior.
 */
add_task(async function test_translations_actor_sync_create() {
  const { remoteClients, cleanup } = await setupActorTest({
    autoDownloadFromRemoteSettings: true,
    languagePairs: [
      { fromLang: "en", toLang: "es" },
      { fromLang: "es", toLang: "en" },
    ],
  });

  const decoder = new TextDecoder();
  const modelsPromise = TranslationsParent.getLanguageTranslationModelFiles(
    "en",
    "es"
  );

  await remoteClients.translationModels.resolvePendingDownloads(
    FILES_PER_LANGUAGE_PAIR
  );

  is(
    decoder.decode((await modelsPromise).model.buffer),
    "Mocked download: test-translation-models model.enes.intgemm.alphas.bin 1.0",
    "The version 1.0 model is downloaded."
  );

  info('Emitting a remote client "sync" event with new records.');
  await remoteClients.translationModels.client.emit("sync", {
    data: {
      created: createRecordsForLanguagePair("en", "fr"),
      updated: [],
      deleted: [],
    },
  });

  const updatedModelsPromise =
    TranslationsParent.getLanguageTranslationModelFiles("en", "fr");

  await remoteClients.translationModels.resolvePendingDownloads(
    FILES_PER_LANGUAGE_PAIR
  );

  const { vocab, lex, model } = await updatedModelsPromise;

  is(
    decoder.decode(vocab.buffer),
    "Mocked download: test-translation-models vocab.enfr.spm 1.0",
    "The en to fr vocab is downloaded."
  );
  is(
    decoder.decode(lex.buffer),
    "Mocked download: test-translation-models lex.50.50.enfr.s2t.bin 1.0",
    "The en to fr lex is downloaded."
  );
  is(
    decoder.decode(model.buffer),
    "Mocked download: test-translation-models model.enfr.intgemm.alphas.bin 1.0",
    "The en to fr model is downloaded."
  );

  return cleanup();
});
