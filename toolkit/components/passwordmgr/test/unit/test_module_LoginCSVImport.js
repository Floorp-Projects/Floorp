/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests the LoginCSVImport module.
 */

"use strict";

const {
  LoginCSVImport,
  ImportFailedException,
  ImportFailedErrorType,
} = ChromeUtils.import("resource://gre/modules/LoginCSVImport.jsm");
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
 * @param {string} extension
 *        Optional parameter. Either 'csv' or 'tsv'. Default is 'csv'.
 * @returns {string} The path to the CSV file that was created.
 */
async function setupCsv(csvLines, extension) {
  // Cleanup state.
  TTU.getAndClearHistogram("PWMGR_IMPORT_LOGINS_FROM_FILE_CATEGORICAL");
  TTU.getAndClearHistogram("PWMGR_IMPORT_LOGINS_FROM_FILE_MS");
  TTU.getAndClearHistogram("PWMGR_IMPORT_LOGINS_FROM_FILE_JANK_MS");
  Services.logins.removeAllUserFacingLogins();
  let tmpFile = await LoginTestUtils.file.setupCsvFileWithLines(
    csvLines,
    extension
  );
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
 * Ensure that an import works with TSV.
 */
add_task(async function test_import_tsv() {
  let tsvFilePath = await setupCsv(
    [
      "url\tusername\tpassword\thttpRealm\tformActionOrigin\tguid\ttimeCreated\ttimeLastUsed\ttimePasswordChanged",
      `https://example.com:8080\tjoe@example.com\tqwerty\tMy realm\t""\t{5ec0d12f-e194-4279-ae1b-d7d281bb46f0}\t1589617814635\t1589710449871\t1589617846802`,
    ],
    "tsv"
  );

  await LoginCSVImport.importFromCSV(tsvFilePath);

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
 * Ensure that an import fails if there is no username column in a TSV file.
 */
add_task(async function test_import_tsv_with_missing_columns() {
  let csvFilePath = await setupCsv(
    [
      "url\tusernameTypo\tpassword\thttpRealm\tformActionOrigin\tguid\ttimeCreated\ttimeLastUsed\ttimePasswordChanged",
      "https://example.com\tkramer@example.com\tqwerty\tMy realm\t\t{5ec0d12f-e194-4279-ae1b-d7d281bb46f7}\t1589617814635\t1589710449871\t1589617846802",
    ],
    "tsv"
  );

  await Assert.rejects(
    LoginCSVImport.importFromCSV(csvFilePath),
    /FILE_FORMAT_ERROR/,
    "Ensure missing username throws"
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
    /FILE_FORMAT_ERROR/,
    "Ensure missing username throws"
  );

  LoginTestUtils.checkLogins(
    [],
    "Check that no login was added without finding a username column"
  );
});

/**
 * Ensure that an import fails if there are two columns that map to one login field.
 */
add_task(async function test_import_with_duplicate_fields() {
  // Two origin columns (url & login_uri).
  // One row has different values and the other has the same.
  let csvFilePath = await setupCsv([
    "url,login_uri,username,login_password",
    "https://example.com/path,https://example.com,john@example.com,azerty",
    "https://mozilla.org,https://mozilla.org,jdoe@example.com,qwerty",
  ]);

  await Assert.rejects(
    LoginCSVImport.importFromCSV(csvFilePath),
    /CONFLICTING_VALUES_ERROR/,
    "Check that the errorType is file format error"
  );

  LoginTestUtils.checkLogins(
    [],
    "Check that no login was added from a file with duplicated columns"
  );
});

/**
 * Ensure that an import fails if there are two identical columns.
 */
add_task(async function test_import_with_duplicate_columns() {
  let csvFilePath = await setupCsv([
    "url,username,password,password",
    "https://example.com/path,john@example.com,azerty,12345",
  ]);

  await Assert.rejects(
    LoginCSVImport.importFromCSV(csvFilePath),
    /CONFLICTING_VALUES_ERROR/,
    "Check that the errorType is file format error"
  );

  LoginTestUtils.checkLogins(
    [],
    "Check that no login was added from a file with duplicated columns"
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
    "http://example.com,joe@example.com,s3cret1,,https://other.example.com,{5ec0d12f-e194-4279-ae1b-d7d281bb46f1},1589617814635,1589710449871,1589617846802",
  ]);

  await LoginCSVImport.importFromCSV(csvFilePath);

  LoginTestUtils.checkLogins(
    [
      TestData.formLogin({
        formActionOrigin: "https://other.example.com",
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

/**
 * Imports login data with an item without the username.
 */
add_task(async function test_import_login_without_username() {
  let csvFilePath = await setupCsv([
    "url,username,password",
    "https://example.com/login,,secret_password",
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
        username: "",
        usernameField: "",
      }),
    ],
    "Check that a Login is added without an username",
    (a, e) =>
      a.equals(e) &&
      checkMetaInfo(a, e, ["timesUsed"]) &&
      checkLoginNewlyCreated(a)
  );
});

/**
 * Imports login data from a KeepassXC CSV file.
 * `Title` is ignored until bug 1433770.
 */
add_task(async function test_import_from_keepassxc_csv() {
  let csvFilePath = await setupCsv([
    `"Group","Title","Username","Password","URL","Notes"`,
    `"NewDatabase/Internet","Amazing","test@example.com","<password>","https://example.org",""`,
  ]);

  await LoginCSVImport.importFromCSV(csvFilePath);

  LoginTestUtils.checkLogins(
    [
      TestData.formLogin({
        formActionOrigin: "",
        httpRealm: null,
        origin: "https://example.org",
        password: "<password>",
        passwordField: "",
        timesUsed: 1,
        username: "test@example.com",
        usernameField: "",
      }),
    ],
    "Check that a new KeepassXC login was added with the correct fields",
    (a, e) =>
      a.equals(e) &&
      checkMetaInfo(a, e, ["timesUsed"]) &&
      checkLoginNewlyCreated(a)
  );
});

/**
 * Imports login data summary contains added logins.
 */
add_task(async function test_import_summary_contains_added_login() {
  let csvFilePath = await setupCsv([
    "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
    "https://added.example.com,jane@example.com,added_passwordd,My realm,,{5ec0d12f-e194-4279-ae1b-d7d281bb0003},1589617814635,1589710449871,1589617846802",
  ]);

  let [added] = await LoginCSVImport.importFromCSV(csvFilePath);

  equal(added.result, "added", `Check that the login was added`);
});

/**
 * Imports login data summary contains modified logins without guid.
 */
add_task(async function test_import_summary_modified_login_without_guid() {
  let initialDataFile = await setupCsv([
    "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
    "https://modifiedwithoutguid.example.com,gini@example.com,initial_password,My realm,,,1589617814635,1589710449871,1589617846802",
  ]);
  await LoginCSVImport.importFromCSV(initialDataFile);

  let csvFile = await LoginTestUtils.file.setupCsvFileWithLines([
    "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
    "https://modifiedwithoutguid.example.com,gini@example.com,modified_password,My realm,,,1589617814635,1589710449871,1589617846999",
  ]);

  let [modifiedWithoutGuid] = await LoginCSVImport.importFromCSV(csvFile.path);

  equal(
    modifiedWithoutGuid.result,
    "modified",
    `Check that the login was modified when there was no guid data`
  );
  LoginTestUtils.checkLogins(
    [
      TestData.authLogin({
        formActionOrigin: null,
        guid: null,
        httpRealm: "My realm",
        origin: "https://modifiedwithoutguid.example.com",
        password: "modified_password",
        passwordField: "",
        timeCreated: 1589617814635,
        timeLastUsed: 1589710449871,
        timePasswordChanged: 1589617846999,
        timesUsed: 1,
        username: "gini@example.com",
        usernameField: "",
      }),
    ],
    "Check that logins were updated with the correct fields",
    (a, e) => a.equals(e) && checkMetaInfo(a, e)
  );
});

/**
 * Imports login data summary contains modified logins with guid.
 */
add_task(async function test_import_summary_modified_login_with_guid() {
  let initialDataFile = await setupCsv([
    "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
    "https://modifiedwithguid.example.com,jane@example.com,initial_password,My realm,,{5ec0d12f-e194-4279-ae1b-d7d281bb0001},1589617814635,1589710449871,1589617846802",
  ]);
  await LoginCSVImport.importFromCSV(initialDataFile);

  let csvFile = await LoginTestUtils.file.setupCsvFileWithLines([
    "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
    "https://modified.example.com,jane@example.com,modified_password,My realm,,{5ec0d12f-e194-4279-ae1b-d7d281bb0001},1589617814635,1589710449871,1589617846999",
  ]);

  let [modifiedWithGuid] = await LoginCSVImport.importFromCSV(csvFile.path);

  equal(
    modifiedWithGuid.result,
    "modified",
    `Check that the login was modified when it had the same guid`
  );
  LoginTestUtils.checkLogins(
    [
      TestData.authLogin({
        formActionOrigin: null,
        guid: "{5ec0d12f-e194-4279-ae1b-d7d281bb0001}",
        httpRealm: "My realm",
        origin: "https://modified.example.com",
        password: "modified_password",
        passwordField: "",
        timeCreated: 1589617814635,
        timeLastUsed: 1589710449871,
        timePasswordChanged: 1589617846999,
        timesUsed: 1,
        username: "jane@example.com",
        usernameField: "",
      }),
    ],
    "Check that logins were updated with the correct fields",
    (a, e) => a.equals(e) && checkMetaInfo(a, e)
  );
});

/**
 * Imports login data summary contains unchanged logins.
 */
add_task(async function test_import_summary_contains_unchanged_login() {
  let initialDataFile = await setupCsv([
    "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
    "https://nochange.example.com,jane@example.com,nochange_password,My realm,,{5ec0d12f-e194-4279-ae1b-d7d281bb0002},1589617814635,1589710449871,1589617846802",
  ]);
  await LoginCSVImport.importFromCSV(initialDataFile);

  let csvFile = await LoginTestUtils.file.setupCsvFileWithLines([
    "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
    "https://nochange.example.com,jane@example.com,nochange_password,My realm,,{5ec0d12f-e194-4279-ae1b-d7d281bb0002},1589617814635,1589710449871,1589617846802",
  ]);

  let [noChange] = await LoginCSVImport.importFromCSV(csvFile.path);

  equal(noChange.result, "no_change", `Check that the login was not changed`);
});

/**
 * Imports login data summary contains logins with errors in case of missing fields.
 */
add_task(async function test_import_summary_contains_missing_fields_errors() {
  const missingFieldsToCheck = ["url", "password"];
  const sourceObject = {
    url: "https://invalid.password.example.com",
    username: "jane@example.com",
    password: "qwerty",
  };
  for (const missingField of missingFieldsToCheck) {
    const clonedUser = { ...sourceObject };
    clonedUser[missingField] = "";
    let csvFilePath = await setupCsv([
      "url,username,password",
      `${clonedUser.url},${clonedUser.username},${clonedUser.password}`,
    ]);

    let [importLogin] = await LoginCSVImport.importFromCSV(csvFilePath);

    equal(
      importLogin.result,
      "error_missing_field",
      `Check that the missing field error is reported for ${missingField}`
    );
    equal(
      importLogin.field_name,
      missingField,
      `Check that the invalid field name is correctly reported for the ${missingField}`
    );
  }
});

/**
 * Imports login with wrong file format will have correct errorType.
 */
add_task(async function test_import_summary_with_bad_format() {
  let csvFilePath = await setupCsv(["password", "123qwe!@#QWE"]);

  await Assert.rejects(
    LoginCSVImport.importFromCSV(csvFilePath),
    /FILE_FORMAT_ERROR/,
    "Check that the errorType is file format error"
  );

  LoginTestUtils.checkLogins(
    [],
    "Check that no login was added with bad format"
  );
});

/**
 * Imports login with wrong file type will have correct errorType.
 */
add_task(async function test_import_summary_with_non_csv_file() {
  let csvFilePath = await setupCsv([
    "<body>this is totally not a csv file</body>",
  ]);

  await Assert.rejects(
    LoginCSVImport.importFromCSV(csvFilePath),
    /FILE_FORMAT_ERROR/,
    "Check that the errorType is file format error"
  );

  LoginTestUtils.checkLogins(
    [],
    "Check that no login was added with file of different format"
  );
});

/**
 * Imports login multiple url and user will import the first and skip the second.
 */
add_task(async function test_import_summary_with_url_user_multiple_values() {
  let csvFilePath = await setupCsv([
    "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
    "https://example.com,jane@example.com,password1,My realm",
    "https://example.com,jane@example.com,password2,My realm",
  ]);

  let initialLoginCount = Services.logins.getAllLogins().length;

  let results = await LoginCSVImport.importFromCSV(csvFilePath);
  let afterImportLoginCount = Services.logins.getAllLogins().length;

  equal(results.length, 2, `Check that we got a result for each imported row`);
  equal(results[0].result, "added", `Check that the first login was added`);
  equal(
    results[1].result,
    "no_change",
    `Check that the second login was skipped`
  );
  equal(initialLoginCount, 0, `Check that initially we had no logins`);
  equal(afterImportLoginCount, 1, `Check that we imported only one login`);
});

/**
 * Imports login with duplicated guid values throws error.
 */
add_task(async function test_import_summary_with_duplicated_guid_values() {
  let csvFilePath = await setupCsv([
    "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
    "https://example1.com,jane1@example.com,password1,My realm,,{5ec0d12f-e194-4279-ae1b-d7d281bb0004},1589617814635,1589710449871,1589617846802",
    "https://example2.com,jane2@example.com,password2,My realm,,{5ec0d12f-e194-4279-ae1b-d7d281bb0004},1589617814635,1589710449871,1589617846802",
  ]);
  let initialLoginCount = Services.logins.getAllLogins().length;

  let results = await LoginCSVImport.importFromCSV(csvFilePath);
  let afterImportLoginCount = Services.logins.getAllLogins().length;

  equal(results.length, 2, `Check that we got a result for each imported row`);
  equal(results[0].result, "added", `Check that the first login was added`);
  equal(results[1].result, "error", `Check that the second login was an error`);
  equal(initialLoginCount, 0, `Check that initially we had no logins`);
  equal(afterImportLoginCount, 1, `Check that we imported only one login`);
});

/**
 * Imports login with different passwords will pick up the newest one and ignore the oldest one.
 */
add_task(async function test_import_summary_with_different_time_changed() {
  let csvFilePath = await setupCsv([
    "url,username,password,timeCreated,timeLastUsed,timePasswordChanged",
    "https://example.com,eve@example.com,old password,1589617814635,1589710449800,1589617846800",
    "https://example.com,eve@example.com,new password,1589617814635,1589710449801,1589617846801",
  ]);
  let initialLoginCount = Services.logins.getAllLogins().length;

  let results = await LoginCSVImport.importFromCSV(csvFilePath);
  let afterImportLoginCount = Services.logins.getAllLogins().length;

  equal(results.length, 2, `Check that we got a result for each imported row`);
  equal(
    results[0].result,
    "no_change",
    `Check that the oldest password is skipped`
  );
  equal(
    results[1].login.password,
    "new password",
    `Check that the newest password is imported`
  );
  equal(
    results[1].result,
    "added",
    `Check that the newest password result is correct`
  );
  equal(initialLoginCount, 0, `Check that initially we had no logins`);
  equal(afterImportLoginCount, 1, `Check that we imported only one login`);
});

/**
 * Imports duplicate logins as one without an error.
 */
add_task(async function test_import_duplicate_logins_as_one() {
  let csvFilePath = await setupCsv([
    "name,url,username,password",
    "somesite,https://example.com/,user@example.com,asdasd123123",
    "somesite,https://example.com/,user@example.com,asdasd123123",
  ]);
  let initialLoginCount = Services.logins.getAllLogins().length;

  let results = await LoginCSVImport.importFromCSV(csvFilePath);
  let afterImportLoginCount = Services.logins.getAllLogins().length;

  equal(results.length, 2, `Check that we got a result for each imported row`);
  equal(
    results[0].result,
    "added",
    `Check that the first login login was added`
  );
  equal(
    results[1].result,
    "no_change",
    `Check that the second login was not changed`
  );

  equal(initialLoginCount, 0, `Check that initially we had no logins`);
  equal(afterImportLoginCount, 1, `Check that we imported only one login`);
});
