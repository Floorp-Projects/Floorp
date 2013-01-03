/**
 * Test for nsICacheSession.doomEntry().
 * It tests dooming
 *   - an existent inactive entry
 *   - a non-existent inactive entry
 *   - an existent active entry
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

function GetOutputStreamForEntry(key, append, callback)
{
  this._key = key;
  this._append = append;
  this._callback = callback;
  this.run();
}

GetOutputStreamForEntry.prototype = {
  _key: "",
  _append: false,
  _callback: null,

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsICacheListener) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  onCacheEntryAvailable: function (entry, access, status) {
    if (!entry)
      do_throw("entry not available");

    var ostream = entry.openOutputStream(this._append ? entry.dataSize : 0);
    this._callback(entry, ostream);
  },

  run: function() {
    var cache = get_cache_service();
    var session = cache.createSession(
                    "HTTP",
                    Ci.nsICache.STORE_ON_DISK,
                    Ci.nsICache.STREAM_BASED);
    session.asyncOpenCacheEntry(this._key,
                                this._append ? Ci.nsICache.ACCESS_READ_WRITE
                                             : Ci.nsICache.ACCESS_WRITE,
                                this);
  }
};

function DoomEntry(key, callback) {
  this._key = key;
  this._callback = callback;
  this.run();
}

DoomEntry.prototype = {
  _key: "",
  _callback: null,

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsICacheListener) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  onCacheEntryDoomed: function (status) {
    this._callback(status);
  },

  run: function() {
    get_cache_service()
      .createSession("HTTP",
                     Ci.nsICache.STORE_ANYWHERE,
                     Ci.nsICache.STREAM_BASED)
      .doomEntry(this._key, this);
  }
};

function write_and_check(str, data, len)
{
  var written = str.write(data, len);
  if (written != len) {
    do_throw("str.write has not written all data!\n" +
             "  Expected: " + len  + "\n" +
             "  Actual: " + written + "\n");
  }
}

function write_entry()
{
  new GetOutputStreamForEntry("testentry", false, write_entry_cont);
}

function write_entry_cont(entry, ostream)
{
  var data = "testdata";
  write_and_check(ostream, data, data.length);
  ostream.close();
  entry.close();
  new DoomEntry("testentry", check_doom1);
}

function check_doom1(status)
{
  do_check_eq(status, Cr.NS_OK);
  new DoomEntry("nonexistententry", check_doom2);
}

function check_doom2(status)
{
  do_check_eq(status, Cr.NS_ERROR_NOT_AVAILABLE);
  new GetOutputStreamForEntry("testentry", false, write_entry2);
}

var gEntry;
var gOstream;
function write_entry2(entry, ostream)
{
  // write some data and doom the entry while it is active
  var data = "testdata";
  write_and_check(ostream, data, data.length);
  gEntry = entry;
  gOstream = ostream;
  new DoomEntry("testentry", check_doom3);
}

function check_doom3(status)
{
  do_check_eq(status, Cr.NS_OK);
  // entry was doomed but writing should still succeed
  var data = "testdata";
  write_and_check(gOstream, data, data.length);
  gOstream.close();
  gEntry.close();
  // dooming the same entry again should fail
  new DoomEntry("testentry", check_doom4);
}

function check_doom4(status)
{
  do_check_eq(status, Cr.NS_ERROR_NOT_AVAILABLE);
  do_test_finished();
}

function run_test() {
  do_get_profile();

  // clear the cache
  evict_cache_entries();
  write_entry();
  do_test_pending();
}
