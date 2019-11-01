/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://mochi.test:8888/?t=" + Date.now();
const URL1 = URL + "#1";
const URL2 = URL + "#2";
const URL3 = URL + "#3";

var tmp = Cu.Sandbox(window, { wantGlobalProperties: ["ChromeUtils"] });
Services.scriptloader.loadSubScript(
  "resource://gre/modules/PageThumbs.jsm",
  tmp
);

const EXPIRATION_MIN_CHUNK_SIZE = 50;
const { PageThumbsExpiration } = tmp;

add_task(async function thumbnails_expiration() {
  // Create dummy URLs.
  let dummyURLs = [];
  for (let i = 0; i < EXPIRATION_MIN_CHUNK_SIZE + 10; i++) {
    dummyURLs.push(URL + "#dummy" + i);
  }

  // Make sure our thumbnails aren't expired too early.
  dontExpireThumbnailURLs([URL1, URL2, URL3].concat(dummyURLs));

  // Create three thumbnails.
  await createDummyThumbnail(URL1);
  ok(thumbnailExists(URL1), "first thumbnail created");

  await createDummyThumbnail(URL2);
  ok(thumbnailExists(URL2), "second thumbnail created");

  await createDummyThumbnail(URL3);
  ok(thumbnailExists(URL3), "third thumbnail created");

  // Remove the third thumbnail.
  await expireThumbnails([URL1, URL2]);
  ok(thumbnailExists(URL1), "first thumbnail still exists");
  ok(thumbnailExists(URL2), "second thumbnail still exists");
  ok(!thumbnailExists(URL3), "third thumbnail has been removed");

  // Remove the second thumbnail.
  await expireThumbnails([URL1]);
  ok(thumbnailExists(URL1), "first thumbnail still exists");
  ok(!thumbnailExists(URL2), "second thumbnail has been removed");

  // Remove all thumbnails.
  await expireThumbnails([]);
  ok(!thumbnailExists(URL1), "all thumbnails have been removed");

  // Create some more files than the min chunk size.
  for (let url of dummyURLs) {
    await createDummyThumbnail(url);
  }

  ok(dummyURLs.every(thumbnailExists), "all dummy thumbnails created");

  // Expire thumbnails and expect 10 remaining.
  await expireThumbnails([]);
  let remainingURLs = dummyURLs.filter(thumbnailExists);
  is(remainingURLs.length, 10, "10 dummy thumbnails remaining");

  // Expire thumbnails again. All should be gone by now.
  await expireThumbnails([]);
  remainingURLs = remainingURLs.filter(thumbnailExists);
  is(remainingURLs.length, 0, "no dummy thumbnails remaining");
});

function createDummyThumbnail(aURL) {
  info("Creating dummy thumbnail for " + aURL);
  let dummy = new Uint8Array(10);
  for (let i = 0; i < 10; ++i) {
    dummy[i] = i;
  }
  return PageThumbsStorage.writeData(aURL, dummy);
}

function expireThumbnails(aKeep) {
  return PageThumbsExpiration.expireThumbnails(aKeep);
}
