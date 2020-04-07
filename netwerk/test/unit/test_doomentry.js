/**
 * Test for nsICacheStorage.asyncDoomURI().
 * It tests dooming
 *   - an existent inactive entry
 *   - a non-existent inactive entry
 *   - an existent active entry
 */

"use strict";

function doom(url, callback) {
  get_cache_service()
    .diskCacheStorage(Services.loadContextInfo.default, false)
    .asyncDoomURI(createURI(url), "", {
      onCacheEntryDoomed(result) {
        callback(result);
      },
    });
}

function write_and_check(str, data, len) {
  var written = str.write(data, len);
  if (written != len) {
    do_throw(
      "str.write has not written all data!\n" +
        "  Expected: " +
        len +
        "\n" +
        "  Actual: " +
        written +
        "\n"
    );
  }
}

function write_entry() {
  asyncOpenCacheEntry(
    "http://testentry/",
    "disk",
    Ci.nsICacheStorage.OPEN_TRUNCATE,
    null,
    function(status, entry) {
      write_entry_cont(entry, entry.openOutputStream(0, -1));
    }
  );
}

function write_entry_cont(entry, ostream) {
  var data = "testdata";
  write_and_check(ostream, data, data.length);
  ostream.close();
  entry.close();
  doom("http://testentry/", check_doom1);
}

function check_doom1(status) {
  Assert.equal(status, Cr.NS_OK);
  doom("http://nonexistententry/", check_doom2);
}

function check_doom2(status) {
  Assert.equal(status, Cr.NS_ERROR_NOT_AVAILABLE);
  asyncOpenCacheEntry(
    "http://testentry/",
    "disk",
    Ci.nsICacheStorage.OPEN_TRUNCATE,
    null,
    function(status, entry) {
      write_entry2(entry, entry.openOutputStream(0, -1));
    }
  );
}

var gEntry;
var gOstream;
function write_entry2(entry, ostream) {
  // write some data and doom the entry while it is active
  var data = "testdata";
  write_and_check(ostream, data, data.length);
  gEntry = entry;
  gOstream = ostream;
  doom("http://testentry/", check_doom3);
}

function check_doom3(status) {
  Assert.equal(status, Cr.NS_OK);
  // entry was doomed but writing should still succeed
  var data = "testdata";
  write_and_check(gOstream, data, data.length);
  gOstream.close();
  gEntry.close();
  // dooming the same entry again should fail
  doom("http://testentry/", check_doom4);
}

function check_doom4(status) {
  Assert.equal(status, Cr.NS_ERROR_NOT_AVAILABLE);
  do_test_finished();
}

function run_test() {
  do_get_profile();

  // clear the cache
  evict_cache_entries();
  write_entry();
  do_test_pending();
}
