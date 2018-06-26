/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/Log.jsm");
ChromeUtils.import("resource://services-sync/record.js");
ChromeUtils.import("resource://services-sync/engines.js");
ChromeUtils.import("resource://services-sync/engines/bookmarks.js");
ChromeUtils.import("resource://services-sync/util.js");
ChromeUtils.import("resource://services-sync/service.js");

// Record borrowed from Bug 631361.
const record631361 = {
  id: "M5bwUKK8hPyF",
  index: 150,
  modified: 1296768176.49,
  payload:
  {"id": "M5bwUKK8hPyF",
   "type": "livemark",
   "siteUri": "http://www.bbc.co.uk/go/rss/int/news/-/news/",
   "feedUri": "http://fxfeeds.mozilla.com/en-US/firefox/headlines.xml",
   "parentName": "Bookmarks Toolbar",
   "parentid": "toolbar",
   "title": "Latest Headlines",
   "description": "",
   "children":
     ["7oBdEZB-8BMO", "SUd1wktMNCTB", "eZe4QWzo1BcY", "YNBhGwhVnQsN",
      "92Aw2SMEkFg0", "uw0uKqrVFwd-", "x7mx2P3--8FJ", "d-jVF8UuC9Ye",
      "DV1XVtKLEiZ5", "g4mTaTjr837Z", "1Zi5W3lwBw8T", "FEYqlUHtbBWS",
      "qQd2u7LjosCB", "VUs2djqYfbvn", "KuhYnHocu7eg", "u2gcg9ILRg-3",
      "hfK_RP-EC7Ol", "Aq5qsa4E5msH", "6pZIbxuJTn-K", "k_fp0iN3yYMR",
      "59YD3iNOYO8O", "01afpSdAk2iz", "Cq-kjXDEPIoP", "HtNTjt9UwWWg",
      "IOU8QRSrTR--", "HJ5lSlBx6d1D", "j2dz5R5U6Khc", "5GvEjrNR0yJl",
      "67ozIBF5pNVP", "r5YB0cUx6C_w", "FtmFDBNxDQ6J", "BTACeZq9eEtw",
      "ll4ozQ-_VNJe", "HpImsA4_XuW7", "nJvCUQPLSXwA", "94LG-lh6TUYe",
      "WHn_QoOL94Os", "l-RvjgsZYlej", "LipQ8abcRstN", "74TiLvarE3n_",
      "8fCiLQpQGK1P", "Z6h4WkbwfQFa", "GgAzhqakoS6g", "qyt92T8vpMsK",
      "RyOgVCe2EAOE", "bgSEhW3w6kk5", "hWODjHKGD7Ph", "Cky673aqOHbT",
      "gZCYT7nx3Nwu", "iJzaJxxrM58L", "rUHCRv68aY5L", "6Jc1hNJiVrV9",
      "lmNgoayZ-ym8", "R1lyXsDzlfOd", "pinrXwDnRk6g", "Sn7TmZV01vMM",
      "qoXyU6tcS1dd", "TRLanED-QfBK", "xHbhMeX_FYEA", "aPqacdRlAtaW",
      "E3H04Wn2RfSi", "eaSIMI6kSrcz", "rtkRxFoG5Vqi", "dectkUglV0Dz",
      "B4vUE0BE15No", "qgQFW5AQrgB0", "SxAXvwOhu8Zi", "0S6cRPOg-5Z2",
      "zcZZBGeLnaWW", "B0at8hkQqVZQ", "sgPtgGulbP66", "lwtwGHSCPYaQ",
      "mNTdpgoRZMbW", "-L8Vci6CbkJY", "bVzudKSQERc1", "Gxl9lb4DXsmL",
      "3Qr13GucOtEh"]},
  collection: "bookmarks"
};

function makeLivemark(p, mintGUID) {
  let b = new Livemark("bookmarks", p.id);
  // Copy here, because tests mutate the contents.
  b.cleartext = Cu.cloneInto(p, {});

  if (mintGUID)
    b.id = Utils.makeGUID();

  return b;
}

add_task(async function test_livemark_invalid() {
  let engine = new BookmarksEngine(Service);
  await engine.initialize();
  let store = engine._store;

  _("Livemarks considered invalid by nsLivemarkService are skipped.");

  _("Parent is unknown. Will be set to unfiled.");
  let lateParentRec = makeLivemark(record631361.payload, true);
  lateParentRec.parentid = Utils.makeGUID();

  await store.create(lateParentRec);
  let recInfo = await PlacesUtils.bookmarks.fetch(lateParentRec.id);
  Assert.equal(recInfo.parentGuid, PlacesUtils.bookmarks.unfiledGuid);

  _("No feed URI, which is invalid. Will be skipped.");
  let noFeedURIRec = makeLivemark(record631361.payload, true);
  delete noFeedURIRec.cleartext.feedUri;
  await store.create(noFeedURIRec);
  // No exception, but no creation occurs.
  let noFeedURIItem = await PlacesUtils.bookmarks.fetch(noFeedURIRec.id);
  Assert.equal(null, noFeedURIItem);

  _("Parent is a Livemark. Will be skipped.");
  let lmParentRec = makeLivemark(record631361.payload, true);
  lmParentRec.parentid = recInfo.guid;
  await store.create(lmParentRec);
  // No exception, but no creation occurs.
  let lmParentItem = await PlacesUtils.bookmarks.fetch(lmParentRec.id);
  Assert.equal(null, lmParentItem);

  await engine.finalize();
});
