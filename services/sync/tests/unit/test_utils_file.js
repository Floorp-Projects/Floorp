_("Test file-related utility functions");

Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/constants.js");

function run_test() {
  // Disabled due to Windows failures (bug 599193)
  //_test_getTmp();
  //_test_open();
}

function _test_getTmp() {
  // as the setup phase remove the tmp directory from the
  // filesystem
  let svc = Cc["@mozilla.org/file/directory_service;1"]
              .getService(Ci.nsIProperties);
  let tmp = svc.get("ProfD", Ci.nsIFile);
  tmp.QueryInterface(Ci.nsILocalFile);
  tmp.append("weave");
  tmp.append("tmp");
  if (tmp.exists())
    tmp.remove(true);

  // call getTmp with no argument. A ref to the tmp
  // dir is returned
  _("getTmp with no argument");
  let tmpDir = Utils.getTmp();
  do_check_true(tmpDir instanceof Ci.nsILocalFile);
  do_check_true(tmpDir.isDirectory());
  do_check_true(tmpDir.exists());
  do_check_eq(tmpDir.leafName, "tmp");

  // call getTmp with a string. A ref to the file
  // named with this string and included in the
  // tmp dir is returned
  _("getTmp with a string");
  let tmpFile = Utils.getTmp("name");
  do_check_true(tmpFile instanceof Ci.nsILocalFile);
  do_check_true(!tmpFile.exists()); // getTmp doesn't create the file!
  do_check_eq(tmpFile.leafName, "name");
  do_check_true(tmpDir.contains(tmpFile, false));
}

function _test_open() {

  // we rely on Utils.getTmp to get a temporary file and
  // test Utils.open on that file, that's ok Util.getTmp
  // is also tested (test_getTmp)
  function createFile() {
    let f = Utils.getTmp("_test_");
    f.create(f.NORMAL_FILE_TYPE, PERMS_FILE);
    return f;
  }

  // we should probably test more things here, for example
  // we should test that we cannot write to a stream that
  // is created as a result of opening a file for reading

  let s, f;

  _("Open for reading, providing a file");
  let f1 = createFile();
  [s, f] = Utils.open(f1, "<");
  do_check_eq(f.path, f1.path);
  do_check_true(s instanceof Ci.nsIConverterInputStream);
  f1.remove(false);

  _("Open for reading, providing a file name");
  let f2 = createFile();
  let path2 = f2.path;
  [s, f] = Utils.open(path2, "<");
  do_check_eq(f.path, path2);
  do_check_true(s instanceof Ci.nsIConverterInputStream);
  f2.remove(false);

  _("Open for writing with truncate mode, providing a file");
  let f3 = createFile();
  [s, f] = Utils.open(f3, ">");
  do_check_eq(f.path, f3.path);
  do_check_true(s instanceof Ci.nsIConverterOutputStream);
  f3.remove(false);

  _("Open for writing with truncate mode, providing a file name");
  let f4 = createFile();
  let path4 = f4.path;
  [s, f] = Utils.open(path4, ">");
  do_check_eq(f.path, path4);
  do_check_true(s instanceof Ci.nsIConverterOutputStream);
  f4.remove(false);

  _("Open for writing with append mode, providing a file");
  let f5 = createFile();
  [s, f] = Utils.open(f5, ">>");
  do_check_eq(f.path, f5.path);
  do_check_true(s instanceof Ci.nsIConverterOutputStream);
  f5.remove(false);

  _("Open for writing with append mode, providing a file name");
  let f6 = createFile();
  let path6 = f6.path;
  [s, f] = Utils.open(path6, ">>");
  do_check_eq(f.path, path6);
  do_check_true(s instanceof Ci.nsIConverterOutputStream);
  f6.remove(false);

  _("Open with illegal mode");
  let f7 = createFile();
  let except7;
  try {
    Utils.open(f7, "?!");
  } catch(e) {
    except7 = e;
  }
  do_check_true(!!except7);
  f7.remove(false);

  _("Open non-existing file for reading");
  let f8 = createFile();
  let path8 = f8.path;
  f8.remove(false);
  let except8;
  try {
    Utils.open(path8, "<");
  } catch(e) {
    except8 = e;
  }
  do_check_true(!!except8);

  _("Open for reading, provide permissions");
  let f9 = createFile();
  [s, f] = Utils.open(f9, "<", 0644);
  do_check_eq(f.path, f9.path);
  do_check_true(s instanceof Ci.nsIConverterInputStream);
  f9.remove(false);
}
