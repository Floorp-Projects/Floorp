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
           testserver.identity.primaryPort + "/data/";

/**
 * Compares binary data of 2 arrays and returns true if they are the same
 *
 * @param arr1 The first array to compare
 * @param arr2 The second array to compare
*/
function compareBinaryData(arr1, arr2) {
  Assert.equal(arr1.length, arr2.length);
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

add_task(async function test_404() {
  let res = await ProductAddonChecker.getProductAddonList(root + "404.xml");
  Assert.ok(res.usedFallback);
});

add_task(async function test_not_xml() {
  let res = await ProductAddonChecker.getProductAddonList(root + "bad.txt");
  Assert.ok(res.usedFallback);
});

add_task(async function test_invalid_xml() {
  let res = await ProductAddonChecker.getProductAddonList(root + "bad.xml");
  Assert.ok(res.usedFallback);
});

add_task(async function test_wrong_xml() {
  let res = await ProductAddonChecker.getProductAddonList(root + "bad2.xml");
  Assert.ok(res.usedFallback);
});

add_task(async function test_missing() {
  let addons = await ProductAddonChecker.getProductAddonList(root + "missing.xml");
  Assert.equal(addons, null);
});

add_task(async function test_empty() {
  let res = await ProductAddonChecker.getProductAddonList(root + "empty.xml");
  Assert.ok(Array.isArray(res.gmpAddons));
  Assert.equal(res.gmpAddons.length, 0);
});

add_task(async function test_good_xml() {
  let res = await ProductAddonChecker.getProductAddonList(root + "good.xml");
  Assert.ok(Array.isArray(res.gmpAddons));

  // There are three valid entries in the XML
  Assert.equal(res.gmpAddons.length, 5);

  let addon = res.gmpAddons[0];
  Assert.equal(addon.id, "test1");
  Assert.equal(addon.URL, "http://example.com/test1.xpi");
  Assert.equal(addon.hashFunction, undefined);
  Assert.equal(addon.hashValue, undefined);
  Assert.equal(addon.version, undefined);
  Assert.equal(addon.size, undefined);

  addon = res.gmpAddons[1];
  Assert.equal(addon.id, "test2");
  Assert.equal(addon.URL, "http://example.com/test2.xpi");
  Assert.equal(addon.hashFunction, "md5");
  Assert.equal(addon.hashValue, "djhfgsjdhf");
  Assert.equal(addon.version, undefined);
  Assert.equal(addon.size, undefined);

  addon = res.gmpAddons[2];
  Assert.equal(addon.id, "test3");
  Assert.equal(addon.URL, "http://example.com/test3.xpi");
  Assert.equal(addon.hashFunction, undefined);
  Assert.equal(addon.hashValue, undefined);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.size, 45);

  addon = res.gmpAddons[3];
  Assert.equal(addon.id, "test4");
  Assert.equal(addon.URL, undefined);
  Assert.equal(addon.hashFunction, undefined);
  Assert.equal(addon.hashValue, undefined);
  Assert.equal(addon.version, undefined);
  Assert.equal(addon.size, undefined);

  addon = res.gmpAddons[4];
  Assert.equal(addon.id, undefined);
  Assert.equal(addon.URL, "http://example.com/test5.xpi");
  Assert.equal(addon.hashFunction, undefined);
  Assert.equal(addon.hashValue, undefined);
  Assert.equal(addon.version, undefined);
  Assert.equal(addon.size, undefined);
});

add_task(async function test_download_nourl() {
  try {
    let path = await ProductAddonChecker.downloadAddon({});

    await OS.File.remove(path);
    do_throw("Should not have downloaded a file with a missing url");
  } catch (e) {
    Assert.ok(true, "Should have thrown when downloading a file with a missing url.");
  }
});

add_task(async function test_download_missing() {
  try {
    let path = await ProductAddonChecker.downloadAddon({
      URL: root + "nofile.xpi",
    });

    await OS.File.remove(path);
    do_throw("Should not have downloaded a missing file");
  } catch (e) {
    Assert.ok(true, "Should have thrown when downloading a missing file.");
  }
});

add_task(async function test_download_noverify() {
  let path = await ProductAddonChecker.downloadAddon({
    URL: root + "unsigned.xpi",
  });

  let stat = await OS.File.stat(path);
  Assert.ok(!stat.isDir);
  Assert.equal(stat.size, 452);

  Assert.ok(compareFiles(do_get_file("data/productaddons/unsigned.xpi"), new LocalFile(path)));

  await OS.File.remove(path);
});

add_task(async function test_download_badsize() {
  try {
    let path = await ProductAddonChecker.downloadAddon({
      URL: root + "unsigned.xpi",
      size: 400,
    });

    await OS.File.remove(path);
    do_throw("Should not have downloaded a file with a bad size");
  } catch (e) {
    Assert.ok(true, "Should have thrown when downloading a file with a bad size.");
  }
});

add_task(async function test_download_badhashfn() {
  try {
    let path = await ProductAddonChecker.downloadAddon({
      URL: root + "unsigned.xpi",
      hashFunction: "sha2567",
      hashValue: "9b9abf7ddfc1a6d7ffc7e0247481dcc202363e4445ad3494fb22036f1698c7f3",
    });

    await OS.File.remove(path);
    do_throw("Should not have downloaded a file with a bad hash function");
  } catch (e) {
    Assert.ok(true, "Should have thrown when downloading a file with a bad hash function.");
  }
});

add_task(async function test_download_badhash() {
  try {
    let path = await ProductAddonChecker.downloadAddon({
      URL: root + "unsigned.xpi",
      hashFunction: "sha256",
      hashValue: "8b9abf7ddfc1a6d7ffc7e0247481dcc202363e4445ad3494fb22036f1698c7f3",
    });

    await OS.File.remove(path);
    do_throw("Should not have downloaded a file with a bad hash");
  } catch (e) {
    Assert.ok(true, "Should have thrown when downloading a file with a bad hash.");
  }
});

add_task(async function test_download_works() {
  let path = await ProductAddonChecker.downloadAddon({
    URL: root + "unsigned.xpi",
    size: 452,
    hashFunction: "sha256",
    hashValue: "9b9abf7ddfc1a6d7ffc7e0247481dcc202363e4445ad3494fb22036f1698c7f3",
  });

  let stat = await OS.File.stat(path);
  Assert.ok(!stat.isDir);

  Assert.ok(compareFiles(do_get_file("data/productaddons/unsigned.xpi"), new LocalFile(path)));

  await OS.File.remove(path);
});
