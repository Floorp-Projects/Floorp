"use strict";

const profileDir = do_get_profile();

ChromeUtils.import("resource://gre/modules/ContextualIdentityService.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/osfile.jsm");

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
};

function createCookie(userContextId) {
  Services.cookies.add(COOKIE.host,
                       COOKIE.path,
                       COOKIE.name,
                       COOKIE.value,
                       COOKIE.isSecure,
                       COOKIE.isHttpOnly,
                       COOKIE.isSession,
                       COOKIE.expiry,
                       {userContextId});
}

function hasCookie(userContextId) {
  let found = false;
  let enumerator = Services.cookies.getCookiesFromHost(BASE_URL, {userContextId});
  while (enumerator.hasMoreElements()) {
    let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie);
    if (cookie.originAttributes.userContextId == userContextId) {
      found = true;
      break;
    }
  }
  return found;
}

// Correpted file should delete all.
add_task(async function corruptedFile() {
  const thumbnailPrivateId = ContextualIdentityService._defaultIdentities.filter(
    identity => identity.name === "userContextIdInternal.thumbnail").pop().userContextId;

  const webextStoragePrivateId = ContextualIdentityService._defaultIdentities.filter(
    identity => identity.name === "userContextIdInternal.webextStorageLocal").pop().userContextId;

  // Create a cookie in the default Firefox identity (userContextId 0).
  createCookie(0);

  // Create a cookie in the userContextId 1.
  createCookie(1);

  // Create a cookie in the thumbnail private userContextId.
  createCookie(thumbnailPrivateId);

  // Create a cookie in the extension storage private userContextId.
  createCookie(webextStoragePrivateId);

  ok(hasCookie(0), "We have the new cookie the default firefox identity!");
  ok(hasCookie(1), "We have the new cookie in a public identity!");
  ok(hasCookie(thumbnailPrivateId), "We have the new cookie in the thumbnail private identity!");
  ok(hasCookie(webextStoragePrivateId), "We have the new cookie in the extension storage private identity!");

  // Let's create a corrupted file.
  await OS.File.writeAtomic(TEST_STORE_FILE_PATH, "{ vers",
                            { tmpPath: TEST_STORE_FILE_PATH + ".tmp" });

  let cis = ContextualIdentityService.createNewInstanceForTesting(TEST_STORE_FILE_PATH);
  ok(!!cis, "We have our instance of ContextualIdentityService");

  equal(cis.getPublicIdentities().length, 4, "We should have the default public identities");

  // Verify that when the containers.json file is being rebuilt, the computed lastUserContextId
  // is the expected one.
  equal(cis._lastUserContextId, thumbnailPrivateId,
         "Expect cis._lastUserContextId to be equal to the thumbnails userContextId");

  const privThumbnailIdentity = cis.getPrivateIdentity("userContextIdInternal.thumbnail");
  equal(privThumbnailIdentity && privThumbnailIdentity.userContextId, thumbnailPrivateId,
        "We should have the default thumbnail private identity");

  const privWebextStorageIdentity = cis.getPrivateIdentity("userContextIdInternal.webextStorageLocal");
  equal(privWebextStorageIdentity && privWebextStorageIdentity.userContextId,
        webextStoragePrivateId,
        "We should have the default extensions storage.local private identity");

  // Cookie is gone!
  ok(!hasCookie(1), "We should not have the new cookie in the userContextId 1!");

  // The data stored in the Firefox default userContextId (0), should have not be cleared.
  ok(hasCookie(0), "We should not have the new cookie in the default Firefox identity!");

  // The data stored in the non-public userContextId (e.g. thumbnails private identity)
  // should have not be cleared.
  ok(hasCookie(thumbnailPrivateId),
     "We should have the new cookie in the thumbnail private userContextId!");
  ok(hasCookie(webextStoragePrivateId),
     "We should have the new cookie in the extension storage private userContextId!");

  // Verify the version of the newly created containers.json file.
  cis.save();
  const stateFileText = await OS.File.read(TEST_STORE_FILE_PATH, {encoding: "utf-8"});
  equal(JSON.parse(stateFileText).version, cis.LAST_CONTAINERS_JSON_VERSION,
        "Expect the new containers.json file to have the expected version");
});
