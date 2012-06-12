const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

function gen_1MiB()
{
  var i;
  var data="x";
  for (i=0 ; i < 20 ; i++)
    data+=data;
  return data;
}

function write_and_check(str, data, len)
{
  var written = str.write(data, len);
  if (written != len) {
    do_throw("str.write has not written all data!\n" +
             "  Expected: " + len  + "\n" +
             "  Actual: " + written + "\n");
  }
}

function write_big_datafile(status, entry)
{
  do_check_eq(status, Cr.NS_OK);
  var os = entry.openOutputStream(0);
  var data = gen_1MiB();

  // >64MiB
  var i;
  for (i=0 ; i<65 ; i++)
    write_and_check(os, data, data.length);

  os.close();
  entry.close();
  do_execute_soon(run_test_2);
}

function write_and_doom_big_metafile(status, entry)
{
  do_check_eq(status, Cr.NS_OK);
  var os = entry.openOutputStream(0);
  var data = gen_1MiB();

  // >64MiB
  var i;
  for (i=0 ; i<65 ; i++)
    entry.setMetaDataElement("metadata_"+i, data);

  write_and_check(os, data, data.length);

  os.close();
  entry.doom();
  entry.close();
  do_execute_soon(run_test_3);
}

function check_cache_size() {
  var diskDeviceVisited;
  var visitor = {
    visitDevice: function (deviceID, deviceInfo) {
      if (deviceID == "disk") {
        diskDeviceVisited = true;
        do_check_eq(deviceInfo.totalSize, 0)
      }
      return false;
    },
    visitEntry: function (deviceID, entryInfo) {
      do_throw("unexpected call to visitEntry");
    }
  };

  var cs = get_cache_service();
  diskDeviceVisited = false;
  cs.visitEntries(visitor);
  do_check_true(diskDeviceVisited);
}

function run_test() {
  var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                     getService(Ci.nsIPrefBranch);
  prefBranch.setIntPref("browser.cache.disk.capacity", 50000);

  do_get_profile();

  // clear the cache
  evict_cache_entries();

  // write an entry with data > 64MiB
  asyncOpenCacheEntry("bigdata",
                      "HTTP",
                      Ci.nsICache.STORE_ON_DISK_AS_FILE,
                      Ci.nsICache.ACCESS_WRITE,
                      write_big_datafile);

  do_test_pending();
}

function run_test_2()
{
  check_cache_size();

  // write an entry with metadata > 64MiB
  asyncOpenCacheEntry("bigmetadata",
                      "HTTP",
                      Ci.nsICache.STORE_ON_DISK_AS_FILE,
                      Ci.nsICache.ACCESS_WRITE,
                      write_and_doom_big_metafile);
}

function run_test_3()
{
  check_cache_size();

  do_test_finished();
}
