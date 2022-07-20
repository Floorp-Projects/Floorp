/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests PlacesPreviews.jsm
 */
const { PlacesPreviews } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesPreviews.sys.mjs"
);
const { PlacesTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PlacesTestUtils.sys.mjs"
);

const TEST_URL1 = "http://example.com/";
const TEST_URL2 = "http://example.org/";

/**
 * Counts tombstone entries.
 * @returns {integer} number of tombstone entries.
 */
async function countTombstones() {
  await PlacesTestUtils.promiseAsyncUpdates();
  let db = await PlacesUtils.promiseDBConnection();
  return (
    await db.execute("SELECT count(*) FROM moz_previews_tombstones")
  )[0].getResultByIndex(0);
}

add_task(async function test_thumbnail() {
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
    // Ensure tombstones table has been emptied.
    await TestUtils.waitForCondition(async () => {
      return (await countTombstones()) == 0;
    });
    PlacesPreviews.testSetDeletionTimeout(null);
  });
  // Sanity check initial state.
  Assert.equal(await countTombstones(), 0, "There's no tombstone entries");

  info("Test preview creation and storage.");
  await BrowserTestUtils.withNewTab(TEST_URL1, async browser => {
    await retryUpdatePreview(browser.currentURI.spec);
    let filePath = PlacesPreviews.getPathForUrl(TEST_URL1);
    Assert.ok(await IOUtils.exists(filePath), "The screenshot exists");
    Assert.equal(
      filePath.substring(filePath.lastIndexOf(".")),
      PlacesPreviews.fileExtension,
      "Check extension"
    );
    await testImageFile(filePath);
    await testMozPageThumb(TEST_URL1);
  });
});

add_task(async function test_page_removal() {
  info("Store another preview and test page removal.");
  await BrowserTestUtils.withNewTab(TEST_URL2, async browser => {
    await retryUpdatePreview(browser.currentURI.spec);
    let filePath = PlacesPreviews.getPathForUrl(TEST_URL2);
    Assert.ok(await IOUtils.exists(filePath), "The screenshot exists");
  });

  // Set deletion time to a small value so it runs immediately.
  PlacesPreviews.testSetDeletionTimeout(0);
  info("Wait for deletion, check one preview is removed, not the other one.");
  let promiseDeleted = new Promise(resolve => {
    PlacesPreviews.once("places-preview-deleted", (topic, filePath) => {
      resolve(filePath);
    });
  });
  await PlacesUtils.history.remove(TEST_URL1);

  let deletedFilePath = await promiseDeleted;
  Assert.ok(
    !(await IOUtils.exists(deletedFilePath)),
    "Check deleted file has been removed"
  );

  info("Check tombstones table has been emptied.");
  Assert.equal(await countTombstones(), 0, "There's no tombstone entries");

  info("Check the other thumbnail has not been removed.");
  let path = PlacesPreviews.getPathForUrl(TEST_URL2);
  Assert.ok(await IOUtils.exists(path), "Check non-deleted url is still there");
  await testImageFile(path);
  await testMozPageThumb(TEST_URL2);
});

add_task(async function async_test_deleteOrphans() {
  let path = PlacesPreviews.getPathForUrl(TEST_URL2);
  Assert.ok(await IOUtils.exists(path), "Sanity check one preview exists");
  // Create a file in the given path that doesn't have an entry in Places.
  let fakePath = PathUtils.join(
    PlacesPreviews.getPath(),
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa." + PlacesPreviews.fileExtension
  );
  // File contents don't matter.
  await IOUtils.writeJSON(fakePath, { test: true });
  let promiseDeleted = new Promise(resolve => {
    PlacesPreviews.once("places-preview-deleted", (topic, filePath) => {
      resolve(filePath);
    });
  });

  await PlacesPreviews.deleteOrphans();
  let deletedFilePath = await promiseDeleted;
  Assert.equal(deletedFilePath, fakePath, "Check orphan has been deleted");
  Assert.equal(await countTombstones(), 0, "There's no tombstone entries left");
  Assert.ok(
    !(await IOUtils.exists(fakePath)),
    "Ensure orphan has been deleted"
  );

  Assert.ok(await IOUtils.exists(path), "Ensure valid preview is still there");
});

async function testImageFile(path) {
  info("Load the file and check its content type.");
  const buffer = await IOUtils.read(path);
  const fourcc = new TextDecoder("utf-8").decode(buffer.slice(8, 12));
  Assert.equal(fourcc, "WEBP", "Check the stored preview is webp");
}

async function testMozPageThumb(url) {
  info("Check moz-page-thumb protocol: " + PlacesPreviews.getPageThumbURL(url));
  let { data, contentType } = await fetchImage(
    PlacesPreviews.getPageThumbURL(url)
  );
  Assert.equal(
    contentType,
    PlacesPreviews.fileContentType,
    "Check the content type"
  );
  const fourcc = data.slice(8, 12);
  Assert.equal(fourcc, "WEBP", "Check the returned preview is webp");
}

function fetchImage(url) {
  return new Promise((resolve, reject) => {
    NetUtil.asyncFetch(
      {
        uri: NetUtil.newURI(url),
        loadUsingSystemPrincipal: true,
        contentPolicyType: Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE,
      },
      (input, status, request) => {
        if (!Components.isSuccessCode(status)) {
          reject(new Error("unable to load image"));
          return;
        }

        try {
          let data = NetUtil.readInputStreamToString(input, input.available());
          let contentType = request.QueryInterface(Ci.nsIChannel).contentType;
          input.close();
          resolve({ data, contentType });
        } catch (ex) {
          reject(ex);
        }
      }
    );
  });
}

/**
 * Sometimes on macOS fetching the preview fails for timeout/network reasons,
 * this retries so the test doesn't intermittently fail over it.
 * @param {string} url The url to store a preview for.
 * @returns {Promise} resolved once a preview has been captured.
 */
function retryUpdatePreview(url) {
  return TestUtils.waitForCondition(() => PlacesPreviews.update(url));
}
