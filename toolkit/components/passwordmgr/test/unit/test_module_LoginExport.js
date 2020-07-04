/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests the LoginExport module.
 */

"use strict";

let { LoginExport } = ChromeUtils.import(
  "resource://gre/modules/LoginExport.jsm"
);
let { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

/**
 * Saves the logins to a temporary CSV file, reads the lines and returns the CSV lines.
 * After extracting the CSV lines, it deletes the tmp file.
 */
async function exportAsCSVInTmpFile() {
  let tmpFilePath = FileTestUtils.getTempFile("logins.csv").path;
  await LoginExport.exportAsCSV(tmpFilePath);
  let csvContent = await OS.File.read(tmpFilePath);
  let csvString = new TextDecoder().decode(csvContent);
  await OS.File.remove(tmpFilePath);
  // CSV uses CRLF
  return csvString.split(/\r\n/);
}

const COMMON_LOGIN_MODS = {
  guid: "{5ec0d12f-e194-4279-ae1b-d7d281bb46f0}",
  timeCreated: 1589617814635,
  timeLastUsed: 1589710449871,
  timePasswordChanged: 1589617846802,
  timesUsed: 1,
  username: "joe@example.com",
  password: "qwerty",
  origin: "https://example.com",
};

/**
 * Generates a new login object with all the form login fields populated.
 */
function exportFormLogin(modifications) {
  return LoginTestUtils.testData.formLogin({
    ...COMMON_LOGIN_MODS,
    formActionOrigin: "https://action.example.com",
    ...modifications,
  });
}

function exportAuthLogin(modifications) {
  return LoginTestUtils.testData.authLogin({
    ...COMMON_LOGIN_MODS,
    httpRealm: "My realm",
    ...modifications,
  });
}

add_task(async function setup() {
  let oldLogins = Services.logins;
  Services.logins = { getAllLoginsAsync: sinon.stub() };
  registerCleanupFunction(() => {
    Services.logins = oldLogins;
  });
});

add_task(async function test_buildCSVRow() {
  let testObject = {
    null: null,
    emptyString: "",
    number: 99,
    string: "Foo",
  };
  Assert.deepEqual(
    LoginExport._buildCSVRow(testObject, [
      "null",
      "emptyString",
      "number",
      "string",
    ]),
    ["", `""`, `"99"`, `"Foo"`],
    "Check _buildCSVRow with different types"
  );
});

add_task(async function test_no_new_properties_to_export() {
  let login = exportFormLogin();
  Assert.deepEqual(
    Object.keys(login),
    [
      "QueryInterface",
      "displayOrigin",
      "origin",
      "hostname",
      "formActionOrigin",
      "formSubmitURL",
      "httpRealm",
      "username",
      "usernameField",
      "password",
      "passwordField",
      "init",
      "equals",
      "matches",
      "clone",
      "guid",
      "timeCreated",
      "timeLastUsed",
      "timePasswordChanged",
      "timesUsed",
    ],
    "Check that no new properties were added to a login that should maybe be exported"
  );
});

add_task(async function test_export_one_form_login() {
  let login = exportFormLogin();
  Services.logins.getAllLoginsAsync.returns([login]);

  let rows = await exportAsCSVInTmpFile();

  Assert.equal(
    rows[0],
    '"url","username","password","httpRealm","formActionOrigin","guid","timeCreated","timeLastUsed","timePasswordChanged"',
    "checking csv headers"
  );
  Assert.equal(
    rows[1],
    '"https://example.com","joe@example.com","qwerty",,"https://action.example.com","{5ec0d12f-e194-4279-ae1b-d7d281bb46f0}","1589617814635","1589710449871","1589617846802"',
    `checking login is saved as CSV row\n${JSON.stringify(login)}\n`
  );
});

add_task(async function test_export_one_auth_login() {
  let login = exportAuthLogin();
  Services.logins.getAllLoginsAsync.returns([login]);

  let rows = await exportAsCSVInTmpFile();

  Assert.equal(
    rows[0],
    '"url","username","password","httpRealm","formActionOrigin","guid","timeCreated","timeLastUsed","timePasswordChanged"',
    "checking csv headers"
  );
  Assert.equal(
    rows[1],
    '"https://example.com","joe@example.com","qwerty","My realm",,"{5ec0d12f-e194-4279-ae1b-d7d281bb46f0}","1589617814635","1589710449871","1589617846802"',
    `checking login is saved as CSV row\n${JSON.stringify(login)}\n`
  );
});

add_task(async function test_export_escapes_values() {
  let login = exportFormLogin({
    password: "!@#$%^&*()_+,'",
  });
  Services.logins.getAllLoginsAsync.returns([login]);

  let rows = await exportAsCSVInTmpFile();

  Assert.equal(
    rows[1],
    '"https://example.com","joe@example.com","!@#$%^&*()_+,\'",,"https://action.example.com","{5ec0d12f-e194-4279-ae1b-d7d281bb46f0}","1589617814635","1589710449871","1589617846802"',
    `checking login correctly escapes CSV characters \n${JSON.stringify(login)}`
  );
});

add_task(async function test_export_multiple_rows() {
  let logins = await LoginTestUtils.testData.loginList();
  Services.logins.getAllLoginsAsync.returns(logins);

  let actualRows = await exportAsCSVInTmpFile();
  let expectedRows = [
    '"url","username","password","httpRealm","formActionOrigin","guid","timeCreated","timeLastUsed","timePasswordChanged"',
    '"http://www.example.com","the username","the password for www.example.com",,"http://www.example.com",,,,',
    '"https://www.example.com","the username","the password for https",,"https://www.example.com",,,,',
    '"https://example.com","the username","the password for example.com",,"https://example.com",,,,',
    '"http://www3.example.com","the username","the password",,"http://www.example.com",,,,',
    '"http://www3.example.com","the username","the password",,"https://www.example.com",,,,',
    '"http://www3.example.com","the username","the password",,"http://example.com",,,,',
    '"http://www4.example.com","username one","password one",,"http://www4.example.com",,,,',
    '"http://www4.example.com","username two","password two",,"http://www4.example.com",,,,',
    '"http://www4.example.com","","password three",,"http://www4.example.com",,,,',
    '"http://www5.example.com","multi username","multi password",,"http://www5.example.com",,,,',
    '"http://www6.example.com","","12345",,"http://www6.example.com",,,,',
    '"https://www7.example.com:8080","8080_username","8080_pass",,"https://www7.example.com:8080",,,,',
    '"https://www7.example.com:8080","8080_username2","8080_pass2","My dev server",,,,,',
    '"http://www.example.org","the username","the password","The HTTP Realm",,,,,',
    '"ftp://ftp.example.org","the username","the password","ftp://ftp.example.org",,,,,',
    '"http://www2.example.org","the username","the password","The HTTP Realm",,,,,',
    '"http://www2.example.org","the username other","the password other","The HTTP Realm Other",,,,,',
    '"http://example.net","the username","the password",,"http://example.net",,,,',
    '"http://example.net","the username","the password",,"http://www.example.net",,,,',
    '"http://example.net","username two","the password",,"http://www.example.net",,,,',
    '"http://example.net","the username","the password","The HTTP Realm",,,,,',
    '"http://example.net","username two","the password","The HTTP Realm Other",,,,,',
    '"ftp://example.net","the username","the password","ftp://example.net",,,,,',
    '"chrome://example_extension","the username","the password one","Example Login One",,,,,',
    '"chrome://example_extension","the username","the password two","Example Login Two",,,,,',
    '"file://","file: username","file: password",,"file://",,,,',
    '"https://js.example.com","javascript: username","javascript: password",,"javascript:",,,,',
  ];

  Assert.equal(actualRows.length, expectedRows.length, "Check number of lines");
  for (let i = 0; i < logins.length; i++) {
    let login = logins[i];
    Assert.equal(
      actualRows[i],
      expectedRows[i],
      `checking CSV correctly writes row at index=${i} \n${JSON.stringify(
        login
      )}\n`
    );
  }
});
