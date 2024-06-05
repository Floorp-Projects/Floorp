/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { PlacesUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesUtils.sys.mjs"
);
const { PlacesTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PlacesTestUtils.sys.mjs"
);

Assert.equal(AppConstants.platform, "win", "Is running under Windows");

const TASKBAR_SERVICE = Cc["@mozilla.org/windows-taskbar;1"].getService(
  Ci.nsIWinTaskbar
);
Assert.ok(TASKBAR_SERVICE, "Got nsIWinTaskBar");

const JUMP_LIST_BUILDER = TASKBAR_SERVICE.createJumpListBuilder(
  false /* aPrivateBrowsing */
);
Assert.ok(JUMP_LIST_BUILDER, "Got an nsIJumpListBuilder");

// The Windows Jump List favicon cache is in the profile directory, so let's
// make sure that a profile exists.
do_get_profile();

// See FaviconHelper::kJumpListCacheDir
const JUMP_LIST_CACHE_FOLDER = "jumpListCache";
const JUMP_LIST_CACHE_PATH = PathUtils.join(
  PathUtils.profileDir,
  JUMP_LIST_CACHE_FOLDER
);
const TEST_PAGE_URI = Services.io.newURI("https://widget.test");
const TEST_FAVICON_URI = Services.io.newURI(
  `${TEST_PAGE_URI.spec}/favicon.ico`
);
const FAVICON_MIME_TYPE = "image/x-icon";

/**
 * Clears the contents of the Windows Jump List favicon cache.
 *
 * @returns {Promise<undefined>}
 */
async function emptyFaviconCache() {
  await IOUtils.remove(JUMP_LIST_CACHE_PATH, {
    ignoreAbsent: true,
    recursive: true,
  });
  await IOUtils.makeDirectory(JUMP_LIST_CACHE_PATH);
}

add_setup(async () => {
  const FAVICON_BYTES = await IOUtils.read(do_get_file("favicon.ico").path);
  Assert.ok(FAVICON_BYTES.length, "Was able to load the testing favicon.");

  await PlacesTestUtils.addVisits(TEST_PAGE_URI);
  PlacesUtils.favicons.replaceFaviconData(
    TEST_FAVICON_URI,
    FAVICON_BYTES,
    FAVICON_MIME_TYPE
  );

  return new Promise(resolve => {
    PlacesUtils.favicons.setAndFetchFaviconForPage(
      TEST_PAGE_URI,
      TEST_FAVICON_URI,
      true /* forceReload */,
      PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
      resolve,
      Services.scriptSecurityManager.getSystemPrincipal()
    );
  });
});

/**
 * Test that we can populate the Windows Jump List favicon cache with a known
 * favicon from the favicon database.
 */
add_task(async function test_write_favicon_cache() {
  // Let's make sure that the Windows Jump List favicon cache folder is empty
  // to start.
  await emptyFaviconCache();

  let firstCallCachePath = await JUMP_LIST_BUILDER.obtainAndCacheFaviconAsync(
    TEST_PAGE_URI
  );
  Assert.ok(firstCallCachePath, "Got a non-empty cache path");

  Assert.ok(
    await IOUtils.exists(firstCallCachePath),
    "A file exists at the cache path"
  );
  let cacheBytes = await IOUtils.read(firstCallCachePath);
  let sniffer = Cc["@mozilla.org/image/loader;1"].createInstance(
    Ci.nsIContentSniffer
  );

  // Now test that if we re-request the same favicon, that we don't write the
  // same favicon to disk - but continue to use the existing cached file.
  Assert.equal(
    sniffer.getMIMETypeFromContent(null, cacheBytes, cacheBytes.length),
    FAVICON_MIME_TYPE,
    "An image was written to the path."
  );
  let { lastModified } = await IOUtils.stat(firstCallCachePath);

  let secondCallCachePath = await JUMP_LIST_BUILDER.obtainAndCacheFaviconAsync(
    TEST_PAGE_URI
  );
  Assert.equal(
    secondCallCachePath,
    firstCallCachePath,
    "Got back the same path the second time."
  );
  Assert.equal(
    (await IOUtils.stat(secondCallCachePath)).lastModified,
    lastModified,
    "The file did not get written to again."
  );
});

/**
 * Test that if we request to cache a favicon that doesn't exist within the
 * favicon database that we reject the Promise with a result code.
 */
add_task(async function test_invalid_favicon() {
  // Let's make sure that the Windows Jump List favicon cache folder is empty
  // to start.
  await emptyFaviconCache();
  await Assert.rejects(
    JUMP_LIST_BUILDER.obtainAndCacheFaviconAsync(
      Services.io.newURI("https://page.that.does.not.exist.com")
    ),
    /NS_ERROR_FAILURE/
  );

  // And ensure that no favicons got written despite the error.
  let { length: numberOfFavicons } = await IOUtils.getChildren(
    JUMP_LIST_CACHE_PATH
  );
  Assert.equal(numberOfFavicons, 0, "No favicons were written");
});

/**
 * Test that we will overwrite the favicon cache with a fresh copy if the cache
 * has expired.
 */
add_task(async function test_expired_favicon_cache() {
  // Let's make sure that the Windows Jump List favicon cache folder is empty
  // to start.
  await emptyFaviconCache();

  let firstCallCachePath = await JUMP_LIST_BUILDER.obtainAndCacheFaviconAsync(
    TEST_PAGE_URI
  );
  Assert.ok(firstCallCachePath, "Got a non-empty cache path");
  Assert.ok(
    await IOUtils.exists(firstCallCachePath),
    "A file exists at the cache path"
  );

  // Now make it seem as if the favicon was last modified last week. That's
  // definitely expired, since the actual expiry is when the icon is a day old.
  let aWeekAgo = Date.now() - 86400 * 7; // 86400 seconds in a day, times 7 days.
  await IOUtils.setModificationTime(firstCallCachePath, aWeekAgo);
  let { lastModified } = await IOUtils.stat(firstCallCachePath);

  let secondCallCachePath = await JUMP_LIST_BUILDER.obtainAndCacheFaviconAsync(
    TEST_PAGE_URI
  );
  Assert.equal(
    secondCallCachePath,
    firstCallCachePath,
    "Got back the same path the second time."
  );
  Assert.equal(
    (await IOUtils.stat(secondCallCachePath)).lastModified,
    lastModified,
    "The file got overwritten."
  );
});
