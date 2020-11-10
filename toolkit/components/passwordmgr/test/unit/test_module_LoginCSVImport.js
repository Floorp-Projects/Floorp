/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests the LoginCSVImport module.
 */

"use strict";

const { LoginCSVImport } = ChromeUtils.import(
  "resource://gre/modules/LoginCSVImport.jsm"
);
const { LoginExport } = ChromeUtils.import(
  "resource://gre/modules/LoginExport.jsm"
);
const { TelemetryTestUtils: TTU } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

// Enable the collection (during test) for all products so even products
// that don't collect the data will be able to run the test without failure.
Services.prefs.setBoolPref(
  "toolkit.telemetry.testing.overrideProductsCheck",
  true
);

/**
 * Given an array of strings it creates a temporary CSV file that has them as content.
 *
 * @param {string[]} csvLines
 *        The lines that make up the CSV file.
 * @returns {string} The path to the CSV file that was created.
 */
async function setupCsv(csvLines) {
  // Cleanup state.
  TTU.getAndClearKeyedHistogram("FX_MIGRATION_LOGINS_QUANTITY");
  TTU.getAndClearKeyedHistogram("FX_MIGRATION_LOGINS_IMPORT_MS");
  TTU.getAndClearKeyedHistogram("FX_MIGRATION_LOGINS_JANK_MS");
  Services.logins.removeAllLogins();

  let tmpFile = await LoginTestUtils.file.setupCsvFileWithLines(csvLines);
  return tmpFile.path;
}

function checkMetaInfo(
  actual,
  expected,
  props = ["timesUsed", "timeCreated", "timePasswordChanged", "timeLastUsed"]
) {
  for (let prop of props) {
    // This will throw if not equal.
    equal(actual[prop], expected[prop], `Check ${prop}`);
  }
  return true;
}

function checkLoginNewlyCreated(login) {
  // These will throw if not equal.
  LoginTestUtils.assertTimeIsAboutNow(login.timeCreated);
  LoginTestUtils.assertTimeIsAboutNow(login.timePasswordChanged);
  LoginTestUtils.assertTimeIsAboutNow(login.timeLastUsed);
  return true;
}

/**
 * Ensure that an import fails if there is no username column. We don't want
 */
add_task(async function test_import_tsv() {
  let csvFilePath = await setupCsv([
    "url\tusernameTypo\tpassword\thttpRealm\tformActionOrigin\tguid\ttimeCreated\ttimeLastUsed\ttimePasswordChanged",
    "https://example.com\tjoe@example.com\tqwerty\tMy realm\t\t{5ec0d12f-e194-4279-ae1b-d7d281bb46f0}\t1589617814635\t1589710449871\t1589617846802",
  ]);

  await Assert.rejects(
    LoginCSVImport.importFromCSV(csvFilePath),
    /must contain origin, username, and password columns/,
    "Ensure non-CSV throws"
  );

  LoginTestUtils.checkLogins(
    [],
    "Check that no login was added without finding columns"
  );
});

/**
 * Ensure that an import fails if there is no username column. We don't want
 * to accidentally import duplicates due to a name mismatch for the username column.
 */
add_task(async function test_import_lacking_username_column() {
  let csvFilePath = await setupCsv([
    "url,usernameTypo,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
    `https://example.com,joe@example.com,qwerty,My realm,,{5ec0d12f-e194-4279-ae1b-d7d281bb46f0},1589617814635,1589710449871,1589617846802`,
  ]);

  await Assert.rejects(
    LoginCSVImport.importFromCSV(csvFilePath),
    /must contain origin, username, and password columns/,
    "Ensure missing username throws"
  );

  LoginTestUtils.checkLogins(
    [],
    "Check that no login was added without finding a username column"
  );
});

/**
 * Ensure that an import fails if there are two headings that map to one login field.
 */
add_task(async function test_import_with_duplicate_columns() {
  // Two origin columns (url & login_uri).
  // One row has different values and the other has the same.
  let csvFilePath = await setupCsv([
    "url,login_uri,username,login_password",
    "https://example.com/path,https://example.org,john@example.com,azerty",
    "https://mozilla.org,https://mozilla.org,jdoe@example.com,qwerty",
  ]);

  await LoginCSVImport.importFromCSV(csvFilePath);

  LoginTestUtils.checkLogins(
    [
      TestData.formLogin({
        formActionOrigin: "",
        httpRealm: null,
        origin: "https://mozilla.org",
        password: "qwerty",
        passwordField: "",
        timesUsed: 1,
        username: "jdoe@example.com",
        usernameField: "",
      }),
    ],
    "Check that no login was added with duplicate columns of differing values"
  );
});

/**
 * Ensure that an import doesn't throw with only a header row.
 */
add_task(async function test_import_only_header_row() {
  let csvFilePath = await setupCsv([
    "url,usernameTypo,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
  ]);

  // Shouldn't throw
  await LoginCSVImport.importFromCSV(csvFilePath);

  LoginTestUtils.checkLogins(
    [],
    "Check that no login was added without non-header rows."
  );
});

/**
 * Ensure that import is allowed with only origin, username, password and that
 * one can mix and match column naming between conventions from different
 * password managers (so that we better support new/unknown password managers).
 */
add_task(async function test_import_minimal_with_mixed_naming() {
  let csvFilePath = await setupCsv([
    "url,username,login_password",
    "ftp://example.com,john@example.com,azerty",
  ]);

  await LoginCSVImport.importFromCSV(csvFilePath);
  LoginTestUtils.checkLogins(
    [
      TestData.formLogin({
        formActionOrigin: "",
        httpRealm: null,
        origin: "ftp://example.com",
        password: "azerty",
        passwordField: "",
        timesUsed: 1,
        username: "john@example.com",
        usernameField: "",
      }),
    ],
    "Check that a new login was added with the correct fields",
    (a, e) =>
      a.equals(e) &&
      checkMetaInfo(a, e, ["timesUsed"]) &&
      checkLoginNewlyCreated(a)
  );
});

/**
 * Imports login data from the latest Firefox CSV file for various logins from
 * LoginTestUtils.testData.loginList().
 */
add_task(async function test_import_from_firefox_various_latest() {
  await setupCsv([]);
  info("Populate the login list for export");
  let logins = LoginTestUtils.testData.loginList();
  for (let loginInfo of logins) {
    Services.logins.addLogin(loginInfo);
  }

  let tmpFilePath = FileTestUtils.getTempFile("logins.csv").path;
  await LoginExport.exportAsCSV(tmpFilePath);

  await LoginCSVImport.importFromCSV(tmpFilePath);

  LoginTestUtils.checkLogins(
    logins,
    "Check that all of LoginTestUtils.testData.loginList can be re-imported"
  );
});

/**
 * Imports login data from a Firefox CSV file without quotes.
 */
add_task(async function test_import_from_firefox_auth() {
  let csvFilePath = await setupCsv([
    "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
    `https://example.com:8080,joe@example.com,qwerty,My realm,"",{5ec0d12f-e194-4279-ae1b-d7d281bb46f0},1589617814635,1589710449871,1589617846802`,
  ]);

  await LoginCSVImport.importFromCSV(csvFilePath);

  LoginTestUtils.checkLogins(
    [
      TestData.authLogin({
        formActionOrigin: null,
        guid: "{5ec0d12f-e194-4279-ae1b-d7d281bb46f0}",
        httpRealm: "My realm",
        origin: "https://example.com:8080",
        password: "qwerty",
        passwordField: "",
        timeCreated: 1589617814635,
        timeLastUsed: 1589710449871,
        timePasswordChanged: 1589617846802,
        timesUsed: 1,
        username: "joe@example.com",
        usernameField: "",
      }),
    ],
    "Check that a new login was added with the correct fields",
    (a, e) => a.equals(e) && checkMetaInfo(a, e)
  );
});

/**
 * Imports login data from a Firefox CSV file with quotes.
 */
add_task(async function test_import_from_firefox_auth_with_quotes() {
  let csvFilePath = await setupCsv([
    '"url","username","password","httpRealm","formActionOrigin","guid","timeCreated","timeLastUsed","timePasswordChanged"',
    '"https://example.com","joe@example.com","qwerty2","My realm",,"{5ec0d12f-e194-4279-ae1b-d7d281bb46f0}","1589617814635","1589710449871","1589617846802"',
  ]);

  await LoginCSVImport.importFromCSV(csvFilePath);

  LoginTestUtils.checkLogins(
    [
      TestData.authLogin({
        formActionOrigin: null,
        httpRealm: "My realm",
        origin: "https://example.com",
        password: "qwerty2",
        passwordField: "",
        timeCreated: 1589617814635,
        timeLastUsed: 1589710449871,
        timePasswordChanged: 1589617846802,
        timesUsed: 1,
        username: "joe@example.com",
        usernameField: "",
      }),
    ],
    "Check that a new login was added with the correct fields",
    (a, e) => a.equals(e) && checkMetaInfo(a, e)
  );
});

/**
 * Imports login data from a Firefox CSV file where only cells containing a comma are quoted.
 */
add_task(async function test_import_from_firefox_auth_some_quoted_fields() {
  let csvFilePath = await setupCsv([
    "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
    'https://example.com,joe@example.com,"one,two,tree","My realm",,{5ec0d12f-e194-4279-ae1b-d7d281bb46f0},1589617814635,1589710449871,1589617846802',
  ]);

  await LoginCSVImport.importFromCSV(csvFilePath);

  LoginTestUtils.checkLogins(
    [
      TestData.authLogin({
        formActionOrigin: null,
        httpRealm: "My realm",
        origin: "https://example.com",
        password: "one,two,tree",
        passwordField: "",
        timeCreated: 1589617814635,
        timePasswordChanged: 1589617846802,
        timeLastUsed: 1589710449871,
        timesUsed: 1,
        username: "joe@example.com",
        usernameField: "",
      }),
    ],
    "Check that a new login was added with the correct fields",
    (a, e) => a.equals(e) && checkMetaInfo(a, e)
  );
});

/**
 * Imports login data from a Firefox CSV file with an empty formActionOrigin and null httpRealm
 */
add_task(async function test_import_from_firefox_form_empty_formActionOrigin() {
  let csvFilePath = await setupCsv([
    "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
    "https://example.com,joe@example.com,s3cret1,,,{5ec0d12f-e194-4279-ae1b-d7d281bb46f0},1589617814636,1589710449872,1589617846803",
  ]);

  await LoginCSVImport.importFromCSV(csvFilePath);

  LoginTestUtils.checkLogins(
    [
      TestData.formLogin({
        formActionOrigin: "",
        httpRealm: null,
        origin: "https://example.com",
        password: "s3cret1",
        passwordField: "",
        timeCreated: 1589617814636,
        timePasswordChanged: 1589617846803,
        timeLastUsed: 1589710449872,
        timesUsed: 1,
        username: "joe@example.com",
        usernameField: "",
      }),
    ],
    "Check that a new login was added with the correct fields",
    (a, e) => a.equals(e) && checkMetaInfo(a, e)
  );
});

/**
 * Imports login data from a Firefox CSV file with a non-empty formActionOrigin and null httpRealm.
 */
add_task(async function test_import_from_firefox_form_with_formActionOrigin() {
  let csvFilePath = await setupCsv([
    "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
    "http://example.com,joe@example.com,s3cret1,,https://other.example.org,{5ec0d12f-e194-4279-ae1b-d7d281bb46f1},1589617814635,1589710449871,1589617846802",
  ]);

  await LoginCSVImport.importFromCSV(csvFilePath);

  LoginTestUtils.checkLogins(
    [
      TestData.formLogin({
        formActionOrigin: "https://other.example.org",
        httpRealm: null,
        origin: "http://example.com",
        password: "s3cret1",
        passwordField: "",
        timeCreated: 1589617814635,
        timePasswordChanged: 1589617846802,
        timeLastUsed: 1589710449871,
        timesUsed: 1,
        username: "joe@example.com",
        usernameField: "",
      }),
    ],
    "Check that a new login was added with the correct fields",
    (a, e) => a.equals(e) && checkMetaInfo(a, e)
  );
});

/**
 * Imports login data from a Bitwarden CSV file.
 * `name` is ignored until bug 1433770.
 */
add_task(async function test_import_from_bitwarden_csv() {
  let csvFilePath = await setupCsv([
    "folder,favorite,type,name,notes,fields,login_uri,login_username,login_password,login_totp",
    `,,note,jane's note,"secret note, ignore me!",,,,,`,
    ",,login,example.com,,,https://example.com/login,jane@example.com,secret_password",
  ]);

  await LoginCSVImport.importFromCSV(csvFilePath);

  LoginTestUtils.checkLogins(
    [
      TestData.formLogin({
        formActionOrigin: "",
        httpRealm: null,
        origin: "https://example.com",
        password: "secret_password",
        passwordField: "",
        timesUsed: 1,
        username: "jane@example.com",
        usernameField: "",
      }),
    ],
    "Check that a new Bitwarden login was added with the correct fields",
    (a, e) =>
      a.equals(e) &&
      checkMetaInfo(a, e, ["timesUsed"]) &&
      checkLoginNewlyCreated(a)
  );
});

/**
 * Imports login data from a Chrome CSV file.
 * `name` is ignored until bug 1433770.
 */
add_task(async function test_import_from_chrome_csv() {
  let csvFilePath = await setupCsv([
    "name,url,username,password",
    "example.com,https://example.com/login,jane@example.com,secret_chrome_password",
  ]);

  await LoginCSVImport.importFromCSV(csvFilePath);

  LoginTestUtils.checkLogins(
    [
      TestData.formLogin({
        formActionOrigin: "",
        httpRealm: null,
        origin: "https://example.com",
        password: "secret_chrome_password",
        passwordField: "",
        timesUsed: 1,
        username: "jane@example.com",
        usernameField: "",
      }),
    ],
    "Check that a new Chrome login was added with the correct fields",
    (a, e) =>
      a.equals(e) &&
      checkMetaInfo(a, e, ["timesUsed"]) &&
      checkLoginNewlyCreated(a)
  );
});
