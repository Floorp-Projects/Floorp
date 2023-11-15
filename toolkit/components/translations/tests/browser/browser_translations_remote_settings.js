/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * @typedef {import("../translations").RemoteSettingsClient} RemoteSettingsClient
 * @typedef {import("../../translations").TranslationModelRecord} TranslationModelRecord
 */

const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);

// The full Firefox version string.
const firefoxFullVersion = AppConstants.MOZ_APP_VERSION_DISPLAY;

// The Firefox major version string (i.e. the first set of digits).
const firefoxMajorVersion = firefoxFullVersion.match(/\d+/);

// The Firefox "AlphaZero" version string.
// This is a version that is less than even the latest Nightly
// which is of the form `${firefoxMajorVersion}.a1`.
const firefoxAlphaZeroVersion = `${firefoxMajorVersion}.a0`;

/**
 * Creates a local RemoteSettingsClient for use within tests.
 *
 * @param {string} mockedKey
 * @returns {RemoteSettingsClient}
 */
async function createRemoteSettingsClient(mockedKey) {
  const client = RemoteSettings(mockedKey);
  await client.db.clear();
  await client.db.importChanges({}, Date.now());
  return client;
}

// The following test ensures the capabilities of `filter_expression` in remote settings
// to successfully discriminate against the Firefox version when retrieving records.
//
// This is used when making major breaking changes that would require particular records
// to only show up in certain versions of Firefox, such as actual code changes that no
// longer allow compatibility with given records.
//
// Some examples might be:
//
// - Modifying a wasm.js file that is no longer compatible with a previous wasm binary record.
//   In such a case, the old binary record would need to be shipped in versions with the old
//   wasm.js file, and the new binary record would need to be shipped in version with the new
//   wasm.js file.
//
// - Switching to a different library for translation or language detection.
//   Using a different library would not only mean major changes to the code, but it would
//   certainly mean that the records for the old libraries are no longer compatible.
//   We will need to ship those records only in older versions of Firefox that utilize the old
//   libraries, and ship new records in the new versions of Firefox.
add_task(async function test_filter_current_firefox_version() {
  // Create a new RemoteSettingsClient just for this test.
  let client = await createRemoteSettingsClient(
    "test_filter_current_firefox_version"
  );

  // Create a list of records that are expected to pass the filter_expression and be
  // successfully retrieved from the remote settings client.
  const expectedPresentRecords = [
    {
      name: `undefined filter expression`,
      filter_expression: undefined,
    },
    {
      name: `null filter expression`,
      filter_expression: null,
    },
    {
      name: `empty filter expression`,
      filter_expression: ``,
    },
    {
      name: `env.version == ${firefoxFullVersion}`,
      filter_expression: `env.version|versionCompare('${firefoxFullVersion}') == 0`,
    },
    {
      name: `env.version > ${firefoxAlphaZeroVersion}`,
      filter_expression: `env.version|versionCompare('${firefoxAlphaZeroVersion}') > 0`,
    },
  ];
  for (let record of expectedPresentRecords) {
    client.db.create(record);
  }

  // Create a list of records that are expected fo fail the filter_expression and be
  // absent when we retrieve the records from the remote settings client.
  const expectedAbsentRecords = [
    {
      name: `env.version < 1`,
      filter_expression: `env.version|versionCompare('1') < 0`,
    },
  ];
  for (let record of expectedAbsentRecords) {
    client.db.create(record);
  }

  const retrievedRecords = await client.get();

  // Ensure that each record that is expected to be present exists in the retrieved records.
  for (let expectedPresentRecord of expectedPresentRecords) {
    is(
      retrievedRecords.some(
        record => record.name == expectedPresentRecord.name
      ),
      true,
      `The following record was expected to be present but was not found: ${expectedPresentRecord.name}\n`
    );
  }

  // Ensure that each record that is expected to be absent does not exist in the retrieved records.
  for (let expectedAbsentRecord of expectedAbsentRecords) {
    is(
      retrievedRecords.some(record => record.name == expectedAbsentRecord.name),
      false,
      `The following record was expected to be absent but was found: ${expectedAbsentRecord.name}\n`
    );
  }

  // Ensure that the length of the retrieved records is exactly the length of the records expected to be present.
  is(
    retrievedRecords.length,
    expectedPresentRecords.length,
    `Expected ${expectedPresentRecords.length} items but got ${retrievedRecords.length} items\n`
  );
});

// The following test ensures that we are able to always retrieve the maximum
// compatible version of records. These are for version changes that do not
// require shipping different records based on a particular Firefox version,
// but rather for changes to model content or wasm runtimes that are fully
// compatible with the existing source code.
add_task(async function test_get_records_with_multiple_versions() {
  // Create a new RemoteSettingsClient just for this test.
  let client = await createRemoteSettingsClient(
    "test_get_translation_model_records"
  );

  const lookupKey = record =>
    `${record.name}${TranslationsParent.languagePairKey(
      record.fromLang,
      record.toLang
    )}`;

  // A mapping of each record name to its max version.
  const maxVersionMap = {};

  // Create a list of records that are all version 1.0
  /** @type {TranslationModelRecord[]} */
  const versionOneRecords = [
    {
      id: crypto.randomUUID(),
      name: "qualityModel.enes.bin",
      fromLang: "en",
      toLang: "es",
      fileType: "qualityModel",
      version: "1.0",
    },
    {
      id: crypto.randomUUID(),
      name: "vocab.esen.spm",
      fromLang: "en",
      toLang: "es",
      fileType: "vocab",
      version: "1.0",
    },
    {
      id: crypto.randomUUID(),
      name: "vocab.esen.spm",
      fromLang: "es",
      toLang: "en",
      fileType: "vocab",
      version: "1.0",
    },
    {
      id: crypto.randomUUID(),
      name: "lex.50.50.enes.s2t.bin",
      fromLang: "en",
      toLang: "es",
      fileType: "lex",
      version: "1.0",
    },
    {
      id: crypto.randomUUID(),
      name: "model.enes.intgemm.alphas.bin",
      fromLang: "en",
      toLang: "es",
      fileType: "model",
      version: "1.0",
    },
    {
      id: crypto.randomUUID(),
      name: "vocab.deen.spm",
      fromLang: "en",
      toLang: "de",
      fileType: "vocab",
      version: "1.0",
    },
    {
      id: crypto.randomUUID(),
      name: "lex.50.50.ende.s2t.bin",
      fromLang: "en",
      toLang: "de",
      fileType: "lex",
      version: "1.0",
    },
    {
      id: crypto.randomUUID(),
      name: "model.ende.intgemm.alphas.bin",
      fromLang: "en",
      toLang: "de",
      fileType: "model",
      version: "1.0",
    },
  ];
  versionOneRecords.reduce((map, record) => {
    map[lookupKey(record)] = record.version;
    return map;
  }, maxVersionMap);
  for (const record of versionOneRecords) {
    client.db.create(record);
  }

  // Create a list of records that are identical to some of the above, but with higher version numbers.
  const higherVersionRecords = [
    {
      id: crypto.randomUUID(),
      name: "qualityModel.enes.bin",
      fromLang: "en",
      toLang: "es",
      fileType: "qualityModel",
      version: "1.1",
    },
    {
      id: crypto.randomUUID(),
      name: "qualityModel.enes.bin",
      fromLang: "en",
      toLang: "es",
      fileType: "qualityModel",
      version: "1.2",
    },
    {
      id: crypto.randomUUID(),
      name: "vocab.esen.spm",
      fromLang: "en",
      toLang: "es",
      fileType: "vocab",
      version: "1.1",
    },
  ];
  higherVersionRecords.reduce((map, record) => {
    const key = lookupKey(record);
    if (record.version > map[key]) {
      map[key] = record.version;
    }
    return map;
  }, maxVersionMap);
  for (const record of higherVersionRecords) {
    client.db.create(record);
  }

  TranslationsParent.mockTranslationsEngine(
    client,
    await createTranslationsWasmRemoteClient()
  );

  const retrievedRecords = await TranslationsParent.getMaxVersionRecords(
    client,
    { lookupKey, majorVersion: 1 }
  );

  for (const record of retrievedRecords) {
    is(
      lookupKey(record) in maxVersionMap,
      true,
      `Expected record ${record.name} to be contained in the nameToVersionMap, but found none\n`
    );
    is(
      record.version,
      maxVersionMap[lookupKey(record)],
      `Expected record ${record.name} to be version ${
        maxVersionMap[lookupKey(record)]
      }, but found version ${record.version}\n`
    );
  }

  const expectedSize = Object.keys(maxVersionMap).length;
  is(
    retrievedRecords.length,
    expectedSize,
    `Expected retrieved records to be the same size as the name-to-version map (
      ${expectedSize}
    ), but found ${retrievedRecords.length}\n`
  );

  TranslationsParent.unmockTranslationsEngine();
});
