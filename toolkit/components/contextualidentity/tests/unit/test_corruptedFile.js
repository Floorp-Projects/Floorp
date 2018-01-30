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

// Correpted file should delete all.
add_task(async function corruptedFile() {
  createCookie();
  ok(hasCookie(), "We have the new cookie!");

  // Let's create a corrupted file.
  await OS.File.writeAtomic(TEST_STORE_FILE_PATH, "{ vers",
                            { tmpPath: TEST_STORE_FILE_PATH + ".tmp" });

  let cis = ContextualIdentityService.createNewInstanceForTesting(TEST_STORE_FILE_PATH);
  ok(!!cis, "We have our instance of ContextualIdentityService");

  equal(cis.getPublicIdentities().length, 4, "We should have the default identities");

  // Cookie is gone!
  ok(!hasCookie(), "We should not have the new cookie!");
});
