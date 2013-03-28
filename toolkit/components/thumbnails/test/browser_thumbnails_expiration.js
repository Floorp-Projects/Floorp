/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://mochi.test:8888/?t=" + Date.now();
const URL1 = URL + "#1";
const URL2 = URL + "#2";
const URL3 = URL + "#3";

let tmp = {};
Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("resource://gre/modules/PageThumbs.jsm", tmp);

const {EXPIRATION_MIN_CHUNK_SIZE, PageThumbsExpiration} = tmp;

function runTests() {
  // Create three thumbnails.
  yield createDummyThumbnail(URL1);
  ok(thumbnailExists(URL1), "first thumbnail created");

  yield createDummyThumbnail(URL2);
  ok(thumbnailExists(URL2), "second thumbnail created");

  yield createDummyThumbnail(URL3);
  ok(thumbnailExists(URL3), "third thumbnail created");

  // Remove the third thumbnail.
  yield expireThumbnails([URL1, URL2]);
  ok(thumbnailExists(URL1), "first thumbnail still exists");
  ok(thumbnailExists(URL2), "second thumbnail still exists");
  ok(!thumbnailExists(URL3), "third thumbnail has been removed");

  // Remove the second thumbnail.
  yield expireThumbnails([URL1]);
  ok(thumbnailExists(URL1), "first thumbnail still exists");
  ok(!thumbnailExists(URL2), "second thumbnail has been removed");

  // Remove all thumbnails.
  yield expireThumbnails([]);
  ok(!thumbnailExists(URL1), "all thumbnails have been removed");

  // Create some more files than the min chunk size.
  let urls = [];
  for (let i = 0; i < EXPIRATION_MIN_CHUNK_SIZE + 10; i++) {
    let url = URL + "#dummy" + i;
    urls.push(url);
    yield createDummyThumbnail(url);
  }

  ok(urls.every(thumbnailExists), "all dummy thumbnails created");

  // Make sure our dummy thumbnails aren't expired too early.
  let dontExpireDummyURLs = function (cb) cb(urls);
  PageThumbs.addExpirationFilter(dontExpireDummyURLs);

  registerCleanupFunction(function () {
    PageThumbs.removeExpirationFilter(dontExpireDummyURLs);
  });

  // Expire thumbnails and expect 10 remaining.
  yield expireThumbnails([]);
  let remainingURLs = [u for (u of urls) if (thumbnailExists(u))];
  is(remainingURLs.length, 10, "10 dummy thumbnails remaining");

  // Expire thumbnails again. All should be gone by now.
  yield expireThumbnails([]);
  remainingURLs = [u for (u of remainingURLs) if (thumbnailExists(u))];
  is(remainingURLs.length, 0, "no dummy thumbnails remaining");
}

function createDummyThumbnail(aURL) {
  info("Creating dummy thumbnail for " + aURL);
  let dummy = new Uint8Array(10);
  for (let i = 0; i < 10; ++i) {
    dummy[i] = i;
  }
  PageThumbsStorage.writeData(aURL, dummy).then(
    function onSuccess() {
      info("createDummyThumbnail succeeded");
      executeSoon(next);
    },
    function onFailure(error) {
      ok(false, "createDummyThumbnail failed " + error);
    }
  );
}

function expireThumbnails(aKeep) {
  PageThumbsExpiration.expireThumbnails(aKeep).then(
    function onSuccess() {
      info("expireThumbnails succeeded");
      executeSoon(next);
    },
    function onFailure(error) {
      ok(false, "expireThumbnails failed " + error);
    }
  );
}
