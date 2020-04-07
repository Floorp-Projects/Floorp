/**
 * Test for handling too big alternative data
 *
 *  - first we try to open an output stream for too big alt-data which must fail
 *    and leave original data intact
 *
 *  - then we open the output stream without passing predicted data size which
 *    succeeds but writing must fail later at the size limit and the original
 *    data must be kept
 */

"use strict";

var data = "data    ";
var altData = "alt-data";

function run_test() {
  do_get_profile();

  // Expand both data to 1MB
  for (let i = 0; i < 17; i++) {
    data += data;
    altData += altData;
  }

  // Set the limit so that the data fits but alt-data doesn't.
  Services.prefs.setIntPref("browser.cache.disk.max_entry_size", 1800);

  write_data();

  do_test_pending();
}

function write_data() {
  asyncOpenCacheEntry(
    "http://data/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    function(status, entry) {
      Assert.equal(status, Cr.NS_OK);

      var os = entry.openOutputStream(0, -1);
      var written = os.write(data, data.length);
      Assert.equal(written, data.length);
      os.close();

      open_big_altdata_output(entry);
    }
  );
}

function open_big_altdata_output(entry) {
  try {
    var os = entry.openAlternativeOutputStream("text/binary", altData.length);
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_FILE_TOO_BIG);
  }
  entry.close();

  check_entry(write_big_altdata);
}

function write_big_altdata() {
  asyncOpenCacheEntry(
    "http://data/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    function(status, entry) {
      Assert.equal(status, Cr.NS_OK);

      var os = entry.openAlternativeOutputStream("text/binary", -1);
      try {
        os.write(altData, altData.length);
      } catch (e) {
        Assert.equal(e.result, Cr.NS_ERROR_FILE_TOO_BIG);
      }
      os.close();
      entry.close();

      check_entry(do_test_finished);
    }
  );
}

function check_entry(cb) {
  asyncOpenCacheEntry(
    "http://data/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    function(status, entry) {
      Assert.equal(status, Cr.NS_OK);

      var is = null;
      try {
        is = entry.openAlternativeInputStream("text/binary");
      } catch (e) {
        Assert.equal(e.result, Cr.NS_ERROR_NOT_AVAILABLE);
      }

      is = entry.openInputStream(0);
      pumpReadStream(is, function(read) {
        Assert.equal(read.length, data.length);
        is.close();
        entry.close();

        executeSoon(cb);
      });
    }
  );
}
