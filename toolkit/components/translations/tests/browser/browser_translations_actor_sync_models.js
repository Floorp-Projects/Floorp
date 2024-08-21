/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * An actor unit test for testing RemoteSettings update behavior. This uses the
 * recommendations from:
 *
 * https://firefox-source-docs.mozilla.org/services/settings/index.html#unit-tests
 */
add_task(async function test_translations_actor_sync_update_models() {
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

  const oldModels = await modelsPromise;

  is(
    decoder.decode(oldModels.model.buffer),
    `Mocked download: test-translation-models model.enes.intgemm.alphas.bin ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0`,
    `The version ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0 model is downloaded.`
  );

  const recordsToCreate = createRecordsForLanguagePair("en", "es");
  for (const newModelRecord of recordsToCreate) {
    newModelRecord.id = oldModels[newModelRecord.fileType].record.id;
    newModelRecord.version = `${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.1`;
  }

  await modifyRemoteSettingsRecords(remoteClients.translationModels.client, {
    recordsToCreate,
    expectedUpdatedRecordsCount: 3,
  });

  const updatedModelsPromise =
    TranslationsParent.getLanguageTranslationModelFiles("en", "es");

  const { model: updatedModel } = await updatedModelsPromise;

  is(
    decoder.decode(updatedModel.buffer),
    `Mocked download: test-translation-models model.enes.intgemm.alphas.bin ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.1`,
    `The version ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.1 model is downloaded.`
  );

  return cleanup();
});

/**
 * An actor unit test for testing RemoteSettings delete behavior.
 */
add_task(async function test_translations_actor_sync_delete_models() {
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

  const { model } = await modelsPromise;

  is(
    decoder.decode(model.buffer),
    `Mocked download: test-translation-models model.enes.intgemm.alphas.bin ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0`,
    `The version ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0 model is downloaded.`
  );

  info(
    `Removing record ${model.record.name} from mocked Remote Settings database.`
  );
  await modifyRemoteSettingsRecords(remoteClients.translationModels.client, {
    recordsToDelete: [model.record],
    expectedDeletedRecordsCount: 1,
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
add_task(async function test_translations_actor_sync_create_models() {
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

  is(
    decoder.decode((await modelsPromise).model.buffer),
    `Mocked download: test-translation-models model.enes.intgemm.alphas.bin ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0`,
    `The version ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0 model is downloaded.`
  );

  const recordsToCreate = createRecordsForLanguagePair("en", "fr");

  await modifyRemoteSettingsRecords(remoteClients.translationModels.client, {
    recordsToCreate,
    expectedCreatedRecordsCount: 3,
  });

  const updatedModelsPromise =
    TranslationsParent.getLanguageTranslationModelFiles("en", "fr");

  const { vocab, lex, model } = await updatedModelsPromise;

  is(
    decoder.decode(vocab.buffer),
    `Mocked download: test-translation-models vocab.enfr.spm ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0`,
    "The en to fr vocab is downloaded."
  );
  is(
    decoder.decode(lex.buffer),
    `Mocked download: test-translation-models lex.50.50.enfr.s2t.bin ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0`,
    "The en to fr lex is downloaded."
  );
  is(
    decoder.decode(model.buffer),
    `Mocked download: test-translation-models model.enfr.intgemm.alphas.bin ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0`,
    "The en to fr model is downloaded."
  );

  return cleanup();
});

/**
 * An actor unit test for testing creating a new record has a higher minor version than an existing record of the same kind.
 */
add_task(
  async function test_translations_actor_sync_create_models_higher_minor_version() {
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

    is(
      decoder.decode((await modelsPromise).model.buffer),
      `Mocked download: test-translation-models model.enes.intgemm.alphas.bin ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0`,
      `The version ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0 model is downloaded.`
    );

    const recordsToCreate = createRecordsForLanguagePair("en", "es");
    for (const newModelRecord of recordsToCreate) {
      newModelRecord.version = `${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.1`;
    }

    await modifyRemoteSettingsRecords(remoteClients.translationModels.client, {
      recordsToCreate,
      expectedCreatedRecordsCount: 3,
    });

    const updatedModelsPromise =
      TranslationsParent.getLanguageTranslationModelFiles("en", "es");

    const { vocab, lex, model } = await updatedModelsPromise;

    is(
      decoder.decode(vocab.buffer),
      `Mocked download: test-translation-models vocab.enes.spm ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.1`,
      `The ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.1 vocab is downloaded.`
    );
    is(
      decoder.decode(lex.buffer),
      `Mocked download: test-translation-models lex.50.50.enes.s2t.bin ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.1`,
      `The ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.1 lex is downloaded.`
    );
    is(
      decoder.decode(model.buffer),
      `Mocked download: test-translation-models model.enes.intgemm.alphas.bin ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.1`,
      `The ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.1 model is downloaded.`
    );

    return cleanup();
  }
);

/**
 * An actor unit test for testing creating a new record has a higher major version than an existing record of the same kind.
 */
add_task(
  async function test_translations_actor_sync_create_models_higher_major_version() {
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

    is(
      decoder.decode((await modelsPromise).model.buffer),
      `Mocked download: test-translation-models model.enes.intgemm.alphas.bin ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0`,
      `The version ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0 model is downloaded.`
    );

    const recordsToCreate = createRecordsForLanguagePair("en", "es");
    for (const newModelRecord of recordsToCreate) {
      newModelRecord.version = "2.0";
    }

    await modifyRemoteSettingsRecords(remoteClients.translationModels.client, {
      recordsToCreate,
      expectedCreatedRecordsCount: 3,
    });

    const updatedModelsPromise =
      TranslationsParent.getLanguageTranslationModelFiles("en", "es");

    const { vocab, lex, model } = await updatedModelsPromise;

    is(
      decoder.decode(vocab.buffer),
      `Mocked download: test-translation-models vocab.enes.spm ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0`,
      `The ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0 vocab is downloaded.`
    );
    is(
      decoder.decode(lex.buffer),
      `Mocked download: test-translation-models lex.50.50.enes.s2t.bin ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0`,
      `The ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0 lex is downloaded.`
    );
    is(
      decoder.decode(model.buffer),
      `Mocked download: test-translation-models model.enes.intgemm.alphas.bin ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0`,
      `The ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0 model is downloaded.`
    );

    return cleanup();
  }
);

/**
 * An actor unit test for testing removing a record that has a higher minor version than another record, ensuring
 * that the models roll back to the previous version.
 */
add_task(async function test_translations_actor_sync_rollback_models() {
  const { remoteClients, cleanup } = await setupActorTest({
    autoDownloadFromRemoteSettings: true,
    languagePairs: [
      { fromLang: "en", toLang: "es" },
      { fromLang: "es", toLang: "en" },
    ],
  });

  const newRecords = createRecordsForLanguagePair("en", "es");
  for (const newModelRecord of newRecords) {
    newModelRecord.version = `${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.1`;
  }

  await modifyRemoteSettingsRecords(remoteClients.translationModels.client, {
    recordsToCreate: newRecords,
    expectedCreatedRecordsCount: 3,
  });

  const decoder = new TextDecoder();
  const modelsPromise = TranslationsParent.getLanguageTranslationModelFiles(
    "en",
    "es"
  );

  is(
    decoder.decode((await modelsPromise).model.buffer),
    `Mocked download: test-translation-models model.enes.intgemm.alphas.bin ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.1`,
    `The version ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.1 model is downloaded.`
  );

  await modifyRemoteSettingsRecords(remoteClients.translationModels.client, {
    recordsToDelete: newRecords,
    expectedDeletedRecordsCount: 3,
  });

  const rolledBackModelsPromise =
    TranslationsParent.getLanguageTranslationModelFiles("en", "es");

  const { vocab, lex, model } = await rolledBackModelsPromise;

  is(
    decoder.decode(vocab.buffer),
    `Mocked download: test-translation-models vocab.enes.spm ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0`,
    `The ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0 vocab is downloaded.`
  );
  is(
    decoder.decode(lex.buffer),
    `Mocked download: test-translation-models lex.50.50.enes.s2t.bin ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0`,
    `The ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0 lex is downloaded.`
  );
  is(
    decoder.decode(model.buffer),
    `Mocked download: test-translation-models model.enes.intgemm.alphas.bin ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0`,
    `The ${TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION}.0 model is downloaded.`
  );

  return cleanup();
});

add_task(async function test_translations_parent_download_size() {
  const { cleanup } = await setupActorTest({
    languagePairs: [
      { fromLang: "en", toLang: "es" },
      { fromLang: "es", toLang: "en" },
      { fromLang: "en", toLang: "de" },
      { fromLang: "de", toLang: "en" },
    ],
  });

  const directSize =
    await TranslationsParent.getExpectedTranslationDownloadSize("en", "es");
  // Includes model, lex, and vocab files (x3), each mocked at 123 bytes.
  is(
    directSize,
    3 * 123,
    "Returned the expected download size for a direct translation."
  );

  const pivotSize = await TranslationsParent.getExpectedTranslationDownloadSize(
    "es",
    "de"
  );
  // Includes a pivot (x2), model, lex, and vocab files (x3), each mocked at 123 bytes.
  is(
    pivotSize,
    2 * 3 * 123,
    "Returned the expected download size for a pivot."
  );

  const notApplicableSize =
    await TranslationsParent.getExpectedTranslationDownloadSize(
      "unknown",
      "unknown"
    );
  is(
    notApplicableSize,
    0,
    "Returned the expected download size for an unknown or not applicable model."
  );
  return cleanup();
});
