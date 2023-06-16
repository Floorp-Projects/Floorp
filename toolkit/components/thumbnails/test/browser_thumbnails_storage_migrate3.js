/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://mochi.test:8888/migration3";
const URL2 = URL + "#2";
const URL3 = URL + "#3";
const THUMBNAIL_DIRECTORY = "thumbnails";
const PREF_STORAGE_VERSION = "browser.pagethumbnails.storage_version";

var tmp = Cu.Sandbox(window, { wantGlobalProperties: ["ChromeUtils"] });
Services.scriptloader.loadSubScript(
  "resource://gre/modules/PageThumbs.jsm",
  tmp
);
var { PageThumbsStorageMigrator } = tmp;

/**
 * This test makes sure we correctly migrate to thumbnail storage version 3.
 * This means copying existing thumbnails from the roaming to the local profile
 * directory and should just apply to Linux.
 */
async function runTests() {
  // Prepare a local profile directory.
  let localProfile = await IOUtils.getDirectory(
    PathUtils.join(PathUtils.profileDir, "local-test")
  );
  changeLocation("ProfLD", localProfile);

  let roaming = await IOUtils.getDirectory(
    PathUtils.join(PathUtils.profileDir, THUMBNAIL_DIRECTORY)
  );

  // Set up some data in the roaming profile.
  let name = PageThumbsStorageService.getLeafNameForURL(URL);
  let file = await IOUtils.getFile(
    PathUtils.profileDir,
    THUMBNAIL_DIRECTORY,
    name
  );
  writeDummyFile(file);

  name = PageThumbsStorageService.getLeafNameForURL(URL2);
  file = await IOUtils.getFile(PathUtils.profileDir, THUMBNAIL_DIRECTORY, name);
  writeDummyFile(file);

  name = PageThumbsStorageService.getLeafNameForURL(URL3);
  file = await IOUtils.getFile(PathUtils.profileDir, THUMBNAIL_DIRECTORY, name);
  writeDummyFile(file);

  // Pretend to have one of the thumbnails
  // already in place at the new storage site.
  name = PageThumbsStorageService.getLeafNameForURL(URL3);
  file = await IOUtils.getFile(PathUtils.profileDir, THUMBNAIL_DIRECTORY, name);
  writeDummyFile(file, "no-overwrite-plz");

  // Kick off thumbnail storage migration.
  PageThumbsStorageMigrator.migrateToVersion3(localProfile.path);
  ok(true, "migration finished");

  // Wait until the first thumbnail was moved to its new location.
  await whenFileExists(URL);
  ok(true, "first thumbnail moved");

  // Wait for the second thumbnail to be moved as well.
  await whenFileExists(URL2);
  ok(true, "second thumbnail moved");

  await whenFileRemoved(roaming);
  ok(true, "roaming thumbnail directory removed");

  // Check that our existing thumbnail wasn't overwritten.
  is(
    getFileContents(file),
    "no-overwrite-plz",
    "existing thumbnail was not overwritten"
  );
}

function changeLocation(aLocation, aNewDir) {
  let oldDir = Services.dirsvc.get(aLocation, Ci.nsIFile);
  Services.dirsvc.undefine(aLocation);
  Services.dirsvc.set(aLocation, aNewDir);

  registerCleanupFunction(function () {
    Services.dirsvc.undefine(aLocation);
    Services.dirsvc.set(aLocation, oldDir);
  });
}

function writeDummyFile(aFile, aContents) {
  let fos = FileUtils.openSafeFileOutputStream(aFile);
  let data = aContents || "dummy";
  fos.write(data, data.length);
  FileUtils.closeSafeFileOutputStream(fos);
}

function getFileContents(aFile) {
  let istream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  istream.init(aFile, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);
  return NetUtil.readInputStreamToString(istream, istream.available());
}
