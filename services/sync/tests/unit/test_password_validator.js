/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { PasswordValidator } = ChromeUtils.import(
  "resource://services-sync/engines/passwords.js"
);

function getDummyServerAndClient() {
  return {
    server: [
      {
        id: "11111",
        guid: "11111",
        hostname: "https://www.11111.com",
        formSubmitURL: "https://www.11111.com",
        password: "qwerty123",
        passwordField: "pass",
        username: "foobar",
        usernameField: "user",
        httpRealm: null,
      },
      {
        id: "22222",
        guid: "22222",
        hostname: "https://www.22222.org",
        formSubmitURL: "https://www.22222.org",
        password: "hunter2",
        passwordField: "passwd",
        username: "baz12345",
        usernameField: "user",
        httpRealm: null,
      },
      {
        id: "33333",
        guid: "33333",
        hostname: "https://www.33333.com",
        formSubmitURL: "https://www.33333.com",
        password: "p4ssw0rd",
        passwordField: "passwad",
        username: "quux",
        usernameField: "user",
        httpRealm: null,
      },
    ],
    client: [
      {
        id: "11111",
        guid: "11111",
        hostname: "https://www.11111.com",
        formSubmitURL: "https://www.11111.com",
        password: "qwerty123",
        passwordField: "pass",
        username: "foobar",
        usernameField: "user",
        httpRealm: null,
      },
      {
        id: "22222",
        guid: "22222",
        hostname: "https://www.22222.org",
        formSubmitURL: "https://www.22222.org",
        password: "hunter2",
        passwordField: "passwd",
        username: "baz12345",
        usernameField: "user",
        httpRealm: null,
      },
      {
        id: "33333",
        guid: "33333",
        hostname: "https://www.33333.com",
        formSubmitURL: "https://www.33333.com",
        password: "p4ssw0rd",
        passwordField: "passwad",
        username: "quux",
        usernameField: "user",
        httpRealm: null,
      },
    ],
  };
}

add_task(async function test_valid() {
  let { server, client } = getDummyServerAndClient();
  let validator = new PasswordValidator();
  let {
    problemData,
    clientRecords,
    records,
    deletedRecords,
  } = await validator.compareClientWithServer(client, server);
  equal(clientRecords.length, 3);
  equal(records.length, 3);
  equal(deletedRecords.length, 0);
  deepEqual(problemData, validator.emptyProblemData());
});

add_task(async function test_missing() {
  let validator = new PasswordValidator();
  {
    let { server, client } = getDummyServerAndClient();

    client.pop();

    let {
      problemData,
      clientRecords,
      records,
      deletedRecords,
    } = await validator.compareClientWithServer(client, server);

    equal(clientRecords.length, 2);
    equal(records.length, 3);
    equal(deletedRecords.length, 0);

    let expected = validator.emptyProblemData();
    expected.clientMissing.push("33333");
    deepEqual(problemData, expected);
  }
  {
    let { server, client } = getDummyServerAndClient();

    server.pop();

    let {
      problemData,
      clientRecords,
      records,
      deletedRecords,
    } = await validator.compareClientWithServer(client, server);

    equal(clientRecords.length, 3);
    equal(records.length, 2);
    equal(deletedRecords.length, 0);

    let expected = validator.emptyProblemData();
    expected.serverMissing.push("33333");
    deepEqual(problemData, expected);
  }
});

add_task(async function test_deleted() {
  let { server, client } = getDummyServerAndClient();
  let deletionRecord = { id: "444444", guid: "444444", deleted: true };

  server.push(deletionRecord);
  let validator = new PasswordValidator();

  let {
    problemData,
    clientRecords,
    records,
    deletedRecords,
  } = await validator.compareClientWithServer(client, server);

  equal(clientRecords.length, 3);
  equal(records.length, 4);
  deepEqual(deletedRecords, [deletionRecord]);

  let expected = validator.emptyProblemData();
  deepEqual(problemData, expected);
});

add_task(async function test_duplicates() {
  let validator = new PasswordValidator();
  {
    let { server, client } = getDummyServerAndClient();
    client.push(Cu.cloneInto(client[0], {}));

    let { problemData } = await validator.compareClientWithServer(
      client,
      server
    );

    let expected = validator.emptyProblemData();
    expected.clientDuplicates.push("11111");
    deepEqual(problemData, expected);
  }
  {
    let { server, client } = getDummyServerAndClient();
    server.push(Cu.cloneInto(server[server.length - 1], {}));

    let { problemData } = await validator.compareClientWithServer(
      client,
      server
    );

    let expected = validator.emptyProblemData();
    expected.duplicates.push("33333");
    deepEqual(problemData, expected);
  }
});
