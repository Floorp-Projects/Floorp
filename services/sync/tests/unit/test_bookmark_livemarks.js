/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://testing-common/services-common/utils.js");

const DESCRIPTION_ANNO = "bookmarkProperties/description";

let engine = Engines.get("bookmarks");
let store = engine._store;

// Record borrowed from Bug 631361.
let record631361 = {
  id: "M5bwUKK8hPyF",
  index: 150,
  modified: 1296768176.49,
  payload:
  {"id":"M5bwUKK8hPyF",
   "type":"livemark",
   "siteUri":"http://www.bbc.co.uk/go/rss/int/news/-/news/",
   "feedUri":"http://fxfeeds.mozilla.com/en-US/firefox/headlines.xml",
   "parentName":"Bookmarks Toolbar",
   "parentid":"toolbar",
   "title":"Latest Headlines",
   "description":"",
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

// Clean up after other tests. Only necessary in XULRunner.
store.wipe();

function makeLivemark(p, mintGUID) {
  let b = new Livemark("bookmarks", p.id);
  // Copy here, because tests mutate the contents.
  b.cleartext = TestingUtils.deepCopy(p);
  
  if (mintGUID)
    b.id = Utils.makeGUID();

  return b;
}


function run_test() {
  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Sync.Engine.Bookmarks").level = Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Sync.Store.Bookmarks").level  = Log4Moz.Level.Trace;

  run_next_test();
}

add_test(function test_livemark_descriptions() {
  let record = record631361.payload;

  function doRecord(r) {
    store._childrenToOrder = {};
    store.applyIncoming(r);
    store._orderChildren();
    delete store._childrenToOrder;
  }
  
  // Attempt to provoke an error by messing around with the description.
  record.description = null;
  doRecord(makeLivemark(record));
  record.description = "";
  doRecord(makeLivemark(record));
  
  // Attempt to provoke an error by adding a bad description anno.
  let id = store.idForGUID(record.id);
  PlacesUtils.annotations.setItemAnnotation(id, DESCRIPTION_ANNO, "", 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);

  run_next_test();
});

add_test(function test_livemark_invalid() {
  _("Livemarks considered invalid by nsLivemarkService are skipped.");
  
  _("Parent is 0, which is invalid. Will be set to unfiled.");
  let noParentRec = makeLivemark(record631361.payload, true);
  noParentRec._parent = 0;
  store.create(noParentRec);
  let recID = store.idForGUID(noParentRec.id, true);
  do_check_true(recID > 0);
  do_check_eq(PlacesUtils.bookmarks.getFolderIdForItem(recID), PlacesUtils.bookmarks.unfiledBookmarksFolder);
  
  _("Parent is unknown. Will be set to unfiled.");
  let lateParentRec = makeLivemark(record631361.payload, true);
  let parentGUID = Utils.makeGUID();
  lateParentRec.parentid = parentGUID;
  lateParentRec._parent = store.idForGUID(parentGUID);   // Usually done by applyIncoming.
  do_check_eq(-1, lateParentRec._parent);
  
  store.create(lateParentRec);
  recID = store.idForGUID(lateParentRec.id, true);
  do_check_true(recID > 0);
  do_check_eq(PlacesUtils.bookmarks.getFolderIdForItem(recID),
              PlacesUtils.bookmarks.unfiledBookmarksFolder);
  
  _("No feed URI, which is invalid. Will be skipped.");
  let noFeedURIRec = makeLivemark(record631361.payload, true);
  delete noFeedURIRec.cleartext.feedUri;
  store.create(noFeedURIRec);
  // No exception, but no creation occurs.
  do_check_eq(-1, store.idForGUID(noFeedURIRec.id, true));

  _("Parent is a Livemark. Will be skipped.");
  let lmParentRec = makeLivemark(record631361.payload, true);
  lmParentRec._parent = recID;
  store.create(lmParentRec);
  // No exception, but no creation occurs.
  do_check_eq(-1, store.idForGUID(lmParentRec.id, true));
  
  // Clear event loop.
  Utils.nextTick(run_next_test);
});
