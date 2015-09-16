/**
 * This file tests PlacesUtils.asyncGetBookmarkIds method.
 */

const TEST_URL = "http://www.example.com/";

var promiseAsyncGetBookmarkIds = Task.async(function* (url) {
  yield PlacesTestUtils.promiseAsyncUpdates();
  return new Promise(resolve => {
    PlacesUtils.asyncGetBookmarkIds(url, (itemIds, uri) => {
      Assert.equal(uri, url);
      resolve({ itemIds, url });
    });
  });
});

add_task(function* test_no_bookmark() {
  let { itemIds, url } = yield promiseAsyncGetBookmarkIds(TEST_URL);
  Assert.equal(itemIds.length, 0);
  Assert.equal(url, TEST_URL);
});

add_task(function* test_one_bookmark() {
  let bookmark = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URL,
    title: "test"
  });
  let itemId = yield PlacesUtils.promiseItemId(bookmark.guid);
  {
    let { itemIds, url } = yield promiseAsyncGetBookmarkIds(NetUtil.newURI(TEST_URL));
    Assert.equal(itemIds.length, 1);
    Assert.equal(itemIds[0], itemId);
    Assert.equal(url.spec, TEST_URL);
  }
  {
    let { itemIds, url } = yield promiseAsyncGetBookmarkIds(TEST_URL);
    Assert.equal(itemIds.length, 1);
    Assert.equal(itemIds[0], itemId);
    Assert.equal(url, TEST_URL);
  }
  yield PlacesUtils.bookmarks.remove(bookmark);
});

add_task(function* test_multiple_bookmarks() {
  let ids = [];
  let bookmark1 = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URL,
    title: "test"
  });
  ids.push((yield PlacesUtils.promiseItemId(bookmark1.guid)));
  let bookmark2 = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URL,
    title: "test"
  });
  ids.push((yield PlacesUtils.promiseItemId(bookmark2.guid)));

  let { itemIds, url } = yield promiseAsyncGetBookmarkIds(TEST_URL);
  Assert.deepEqual(ids, itemIds);
  Assert.equal(url, TEST_URL);

  yield PlacesUtils.bookmarks.remove(bookmark1);
  yield PlacesUtils.bookmarks.remove(bookmark2);
});

add_task(function* test_cancel() {
  let pending = PlacesUtils.asyncGetBookmarkIds(TEST_URL, () => {
    Assert.ok(false, "A canceled pending statement should not be invoked");
  });
  pending.cancel();

  let { itemIds, url } = yield promiseAsyncGetBookmarkIds(TEST_URL);
  Assert.equal(itemIds.length, 0);
  Assert.equal(url, TEST_URL);
});
