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

  // Create a cookie in the userContextId 1.
  createCookie(1);

  // Create a cookie in the thumbnail private userContextId.
  createCookie(thumbnailPrivateId);

  ok(hasCookie(1), "We have the new cookie in a public identity!");
  ok(hasCookie(thumbnailPrivateId), "We have the new cookie in the thumbnail private identity!");

  // Let's create a corrupted file.
  await OS.File.writeAtomic(TEST_STORE_FILE_PATH, "{ vers",
                            { tmpPath: TEST_STORE_FILE_PATH + ".tmp" });

  let cis = ContextualIdentityService.createNewInstanceForTesting(TEST_STORE_FILE_PATH);
  ok(!!cis, "We have our instance of ContextualIdentityService");

  equal(cis.getPublicIdentities().length, 4, "We should have the default public identities");

  const privThumbnailIdentity = cis.getPrivateIdentity("userContextIdInternal.thumbnail");
  equal(privThumbnailIdentity && privThumbnailIdentity.userContextId, thumbnailPrivateId,
        "We should have the default thumbnail private identity");

  // Cookie is gone!
  ok(!hasCookie(1), "We should not have the new cookie in the userContextId 1!");

  // The data stored in the non-public userContextId (e.g. thumbnails private identity)
  // should have not be cleared.
  ok(hasCookie(thumbnailPrivateId),
     "We should have the new cookie in the thumbnail private userContextId!");
});
