//
// Test that data can be appended to a cache entry even when the data is 
// compressed by the cache compression feature - bug 648429.
//

function write_and_check(str, data, len)
{
  var written = str.write(data, len);
  if (written != len) {
    do_throw("str.write has not written all data!\n" +
             "  Expected: " + len  + "\n" +
             "  Actual: " + written + "\n");
  }
}

function TestAppend(compress, callback)
{
  this._compress = compress;
  this._callback = callback;
  this.run();
}

TestAppend.prototype = {
  _compress: false,
  _callback: null,

  run: function() {
    evict_cache_entries();
    asyncOpenCacheEntry("http://data/",
                        "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                        this.writeData.bind(this));
  },

  writeData: function(status, entry) {
    Assert.equal(status, Cr.NS_OK);
    if (this._compress)
      entry.setMetaDataElement("uncompressed-len", "0");
    var os = entry.openOutputStream(0, 5);
    write_and_check(os, "12345", 5);
    os.close();
    entry.close();
    asyncOpenCacheEntry("http://data/",
                        "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                        this.appendData.bind(this));
  },

  appendData: function(status, entry) {
    Assert.equal(status, Cr.NS_OK);
    var os = entry.openOutputStream(entry.storageDataSize, 5);
    write_and_check(os, "abcde", 5);
    os.close();
    entry.close();

    asyncOpenCacheEntry("http://data/",
                        "disk", Ci.nsICacheStorage.OPEN_READONLY, null,
                        this.checkData.bind(this));
  },

  checkData: function(status, entry) {
    Assert.equal(status, Cr.NS_OK);
    var self = this;
    pumpReadStream(entry.openInputStream(0), function(str) {
      Assert.equal(str.length, 10);
      Assert.equal(str, "12345abcde");
      entry.close();

      executeSoon(self._callback);
    });
  }
};

function run_test() {
  do_get_profile();
  new TestAppend(false, run_test2);
  do_test_pending();
}

function run_test2() {
  new TestAppend(true, do_test_finished);
}
