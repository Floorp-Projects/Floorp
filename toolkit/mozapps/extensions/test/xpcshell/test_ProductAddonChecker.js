"use strict";

Components.utils.import("resource://gre/modules/addons/ProductAddonChecker.jsm");
Components.utils.import("resource://testing-common/httpd.js");
Components.utils.import("resource://gre/modules/osfile.jsm");

const LocalFile = new Components.Constructor("@mozilla.org/file/local;1", AM_Ci.nsIFile, "initWithPath");

var testserver = new HttpServer();
testserver.registerDirectory("/data/", do_get_file("data/productaddons"));
testserver.start();
var root = testserver.identity.primaryScheme + "://" +
           testserver.identity.primaryHost + ":" +
           testserver.identity.primaryPort + "/data/"

/**
 * Compares binary data of 2 arrays and returns true if they are the same
 *
 * @param arr1 The first array to compare
 * @param arr2 The second array to compare
*/
function compareBinaryData(arr1, arr2) {
  do_check_eq(arr1.length, arr2.length);
  for (let i = 0; i < arr1.length; i++) {
    if (arr1[i] != arr2[i]) {
      do_print("Data differs at index " + i +
               ", arr1: " + arr1[i] + ", arr2: " + arr2[i]);
      return false;
    }
  }
  return true;
}

/**
 * Reads a file's data and returns it
 *
 * @param file The file to read the data from
 * @return array of bytes for the data in the file.
*/
function getBinaryFileData(file) {
  let fileStream = AM_Cc["@mozilla.org/network/file-input-stream;1"].
                   createInstance(AM_Ci.nsIFileInputStream);
  // Open as RD_ONLY with default permissions.
  fileStream.init(file, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);

  let stream = AM_Cc["@mozilla.org/binaryinputstream;1"].
               createInstance(AM_Ci.nsIBinaryInputStream);
  stream.setInputStream(fileStream);
  let bytes = stream.readByteArray(stream.available());
  fileStream.close();
  return bytes;
}

/**
 * Compares binary data of 2 files and returns true if they are the same
 *
 * @param file1 The first file to compare
 * @param file2 The second file to compare
*/
function compareFiles(file1, file2) {
  return compareBinaryData(getBinaryFileData(file1), getBinaryFileData(file2));
}

add_task(function* test_404() {
  try {
    let addons = yield ProductAddonChecker.getProductAddonList(root + "404.xml");
    do_throw("Should not have returned anything");
  }
  catch (e) {
    do_check_true(true, "Expected to throw for a missing update file");
  }
});

add_task(function* test_not_xml() {
  try {
    let addons = yield ProductAddonChecker.getProductAddonList(root + "bad.txt");
    do_throw("Should not have returned anything");
  }
  catch (e) {
    do_check_true(true, "Expected to throw for a non XML result");
  }
});

add_task(function* test_invalid_xml() {
  try {
    let addons = yield ProductAddonChecker.getProductAddonList(root + "bad.xml");
    do_throw("Should not have returned anything");
  }
  catch (e) {
    do_check_true(true, "Expected to throw for invalid XML");
  }
});

add_task(function* test_wrong_xml() {
  try {
    let addons = yield ProductAddonChecker.getProductAddonList(root + "bad2.xml");
    do_throw("Should not have returned anything");
  }
  catch (e) {
    do_check_true(true, "Expected to throw for a missing <updates> tag");
  }
});

add_task(function* test_missing() {
  let addons = yield ProductAddonChecker.getProductAddonList(root + "missing.xml");
  do_check_eq(addons, null);
});

add_task(function* test_empty() {
  let addons = yield ProductAddonChecker.getProductAddonList(root + "empty.xml");
  do_check_true(Array.isArray(addons));
  do_check_eq(addons.length, 0);
});

add_task(function* test_good_xml() {
  let addons = yield ProductAddonChecker.getProductAddonList(root + "good.xml");
  do_check_true(Array.isArray(addons));

  // There are three valid entries in the XML
  do_check_eq(addons.length, 5);

  let addon = addons[0];
  do_check_eq(addon.id, "test1");
  do_check_eq(addon.URL, "http://example.com/test1.xpi");
  do_check_eq(addon.hashFunction, undefined);
  do_check_eq(addon.hashValue, undefined);
  do_check_eq(addon.version, undefined);
  do_check_eq(addon.size, undefined);

  addon = addons[1];
  do_check_eq(addon.id, "test2");
  do_check_eq(addon.URL, "http://example.com/test2.xpi");
  do_check_eq(addon.hashFunction, "md5");
  do_check_eq(addon.hashValue, "djhfgsjdhf");
  do_check_eq(addon.version, undefined);
  do_check_eq(addon.size, undefined);

  addon = addons[2];
  do_check_eq(addon.id, "test3");
  do_check_eq(addon.URL, "http://example.com/test3.xpi");
  do_check_eq(addon.hashFunction, undefined);
  do_check_eq(addon.hashValue, undefined);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.size, 45);

  addon = addons[3];
  do_check_eq(addon.id, "test4");
  do_check_eq(addon.URL, undefined);
  do_check_eq(addon.hashFunction, undefined);
  do_check_eq(addon.hashValue, undefined);
  do_check_eq(addon.version, undefined);
  do_check_eq(addon.size, undefined);

  addon = addons[4];
  do_check_eq(addon.id, undefined);
  do_check_eq(addon.URL, "http://example.com/test5.xpi");
  do_check_eq(addon.hashFunction, undefined);
  do_check_eq(addon.hashValue, undefined);
  do_check_eq(addon.version, undefined);
  do_check_eq(addon.size, undefined);
});

add_task(function* test_download_nourl() {
  try {
    let path = yield ProductAddonChecker.downloadAddon({});

    yield OS.File.remove(path);
    do_throw("Should not have downloaded a file with a missing url");
  }
  catch (e) {
    do_check_true(true, "Should have thrown when downloading a file with a missing url.");
  }
});

add_task(function* test_download_missing() {
  try {
    let path = yield ProductAddonChecker.downloadAddon({
      URL: root + "nofile.xpi",
    });

    yield OS.File.remove(path);
    do_throw("Should not have downloaded a missing file");
  }
  catch (e) {
    do_check_true(true, "Should have thrown when downloading a missing file.");
  }
});

add_task(function* test_download_noverify() {
  let path = yield ProductAddonChecker.downloadAddon({
    URL: root + "unsigned.xpi",
  });

  let stat = yield OS.File.stat(path);
  do_check_false(stat.isDir);
  do_check_eq(stat.size, 452)

  do_check_true(compareFiles(do_get_file("data/productaddons/unsigned.xpi"), new LocalFile(path)));

  yield OS.File.remove(path);
});

add_task(function* test_download_badsize() {
  try {
    let path = yield ProductAddonChecker.downloadAddon({
      URL: root + "unsigned.xpi",
      size: 400,
    });

    yield OS.File.remove(path);
    do_throw("Should not have downloaded a file with a bad size");
  }
  catch (e) {
    do_check_true(true, "Should have thrown when downloading a file with a bad size.");
  }
});

add_task(function* test_download_badhashfn() {
  try {
    let path = yield ProductAddonChecker.downloadAddon({
      URL: root + "unsigned.xpi",
      hashFunction: "sha2567",
      hashValue: "9b9abf7ddfc1a6d7ffc7e0247481dcc202363e4445ad3494fb22036f1698c7f3",
    });

    yield OS.File.remove(path);
    do_throw("Should not have downloaded a file with a bad hash function");
  }
  catch (e) {
    do_check_true(true, "Should have thrown when downloading a file with a bad hash function.");
  }
});

add_task(function* test_download_badhash() {
  try {
    let path = yield ProductAddonChecker.downloadAddon({
      URL: root + "unsigned.xpi",
      hashFunction: "sha256",
      hashValue: "8b9abf7ddfc1a6d7ffc7e0247481dcc202363e4445ad3494fb22036f1698c7f3",
    });

    yield OS.File.remove(path);
    do_throw("Should not have downloaded a file with a bad hash");
  }
  catch (e) {
    do_check_true(true, "Should have thrown when downloading a file with a bad hash.");
  }
});

add_task(function* test_download_works() {
  let path = yield ProductAddonChecker.downloadAddon({
    URL: root + "unsigned.xpi",
    size: 452,
    hashFunction: "sha256",
    hashValue: "9b9abf7ddfc1a6d7ffc7e0247481dcc202363e4445ad3494fb22036f1698c7f3",
  });

  let stat = yield OS.File.stat(path);
  do_check_false(stat.isDir);

  do_check_true(compareFiles(do_get_file("data/productaddons/unsigned.xpi"), new LocalFile(path)));

  yield OS.File.remove(path);
});
