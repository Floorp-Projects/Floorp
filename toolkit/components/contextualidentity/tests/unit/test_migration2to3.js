"use strict";

const profileDir = do_get_profile();

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/ContextualIdentityService.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/osfile.jsm");

const TEST_STORE_FILE_PATH = OS.Path.join(profileDir.path, "test-containers.json");

const BASE_URL = "http://example.org/";

const COOKIE = {
  host: BASE_URL,
  path: "/",
  name: "test",
  value: "yes",
  isSecure: false,
  isHttpOnly: false,
  isSession: true,
  expiry: 2145934800,
  originAttributes: { userContextId: 1 },
};

function writeFile(data) {
  let bytes = (new TextEncoder()).encode(JSON.stringify(data));
  return OS.File.writeAtomic(TEST_STORE_FILE_PATH, bytes,
                             { tmpPath: TEST_STORE_FILE_PATH + ".tmp" });
}

function readFile() {
  return OS.File.read(TEST_STORE_FILE_PATH).then(bytes => {
    return JSON.parse((new TextDecoder()).decode(bytes));
  });
}

function createCookie() {
  Services.cookies.add(COOKIE.host,
                       COOKIE.path,
                       COOKIE.name,
                       COOKIE.value,
                       COOKIE.isSecure,
                       COOKIE.isHttpOnly,
                       COOKIE.isSession,
                       COOKIE.expiry,
                       COOKIE.originAttributes);
}

function hasCookie() {
  let found = false;
  let enumerator = Services.cookies.getCookiesFromHost(BASE_URL, COOKIE.originAttributes);
  while (enumerator.hasMoreElements()) {
    let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie);
    if (cookie.originAttributes.userContextId == COOKIE.originAttributes.userContextId) {
      found = true;
      break;
    }
  }
  return found;
}

// From version 2 to version 3.
add_task(async function simpleMigration() {
  // Let's create a version 2.
  await writeFile({ version: 2, lastUserContextId: 100,
                    identities: [{ userContextId: 1, public: true,
                                  icon: "fingerprint", color: "orange",
                                  name: "TEST" }]});

  let cis = ContextualIdentityService.createNewInstanceForTesting(TEST_STORE_FILE_PATH);
  ok(!!cis, "We have our instance of ContextualIdentityService");

  equal(cis.getPublicIdentities().length, 1, "The test file containes 1 identity");
  ok(!!cis.getPublicIdentityFromId(1), "Identity 1 exists");

  // Let's force the saving.
  await cis.save();

  // Let's check the current version
  let data = await readFile();
  equal(data.version, 3, "We want to be in version 3.");
});

// Here we test that, migrating from 2 to 3, cookies are deleted.
add_task(async function cookieDeleted() {
  createCookie();
  ok(hasCookie(), "We have the new cookie!");

  // Let's create a version 2.
  await writeFile({ version: 2, lastUserContextId: 100,
                    identities: [{ userContextId: 1, public: true,
                                  icon: "fingerprint", color: "orange",
                                  name: "TEST" }]});

  let cis = ContextualIdentityService.createNewInstanceForTesting(TEST_STORE_FILE_PATH);
  ok(!!cis, "We have our instance of ContextualIdentityService");

  equal(cis.getPublicIdentities().length, 1, "The test file containes 1 identity");
  ok(!!cis.getPublicIdentityFromId(1), "Identity 1 exists");

  // Cookie is gone!
  ok(!hasCookie(), "We should not have the new cookie!");

  // Let's force the saving.
  await cis.save();

  // Let's be sure that the cookie is not deleted again.
  createCookie();
  ok(hasCookie(), "We have the new cookie!");

  cis = ContextualIdentityService.createNewInstanceForTesting(TEST_STORE_FILE_PATH);
  ok(!!cis, "We have our instance of ContextualIdentityService");

  equal(cis.getPublicIdentities().length, 1, "The test file containes 1 identity");

  ok(hasCookie(), "Cookie is not deleted when the file is reopened");
});
