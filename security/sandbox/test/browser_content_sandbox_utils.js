/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
                      .getService(Ci.nsIUUIDGenerator);
const environment = Cc["@mozilla.org/process/environment;1"]
                    .getService(Ci.nsIEnvironment);

/*
 * Utility functions for the browser content sandbox tests.
 */

function isMac() { return Services.appinfo.OS == "Darwin"; }
function isWin() { return Services.appinfo.OS == "WINNT"; }
function isLinux() { return Services.appinfo.OS == "Linux"; }

function isNightly() {
  let version = SpecialPowers.Cc["@mozilla.org/xre/app-info;1"].
    getService(SpecialPowers.Ci.nsIXULAppInfo).version;
  return (version.endsWith("a1"));
}

function uuid() {
  return uuidGenerator.generateUUID().toString();
}

// Returns a file object for a new file in the home dir ($HOME/<UUID>).
function fileInHomeDir() {
  // get home directory, make sure it exists
  let homeDir = Services.dirsvc.get("Home", Ci.nsILocalFile);
  Assert.ok(homeDir.exists(), "Home dir exists");
  Assert.ok(homeDir.isDirectory(), "Home dir is a directory");

  // build a file object for a new file named $HOME/<UUID>
  let homeFile = homeDir.clone();
  homeFile.appendRelativePath(uuid());
  Assert.ok(!homeFile.exists(), homeFile.path + " does not exist");
  return (homeFile);
}

// Returns a file object for a new file in the content temp dir (.../<UUID>).
function fileInTempDir() {
  let contentTempKey = "ContentTmpD";
  if (Services.appinfo.OS == "Linux") {
    // Linux builds don't use the content-specific temp key
    contentTempKey = "TmpD";
  }

  // get the content temp dir, make sure it exists
  let ctmp = Services.dirsvc.get(contentTempKey, Ci.nsILocalFile);
  Assert.ok(ctmp.exists(), "Content temp dir exists");
  Assert.ok(ctmp.isDirectory(), "Content temp dir is a directory");

  // build a file object for a new file in content temp
  let tempFile = ctmp.clone();
  tempFile.appendRelativePath(uuid());
  Assert.ok(!tempFile.exists(), tempFile.path + " does not exist");
  return (tempFile);
}

function GetProfileDir() {
  // get profile directory
  let profileDir = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
  return (profileDir);
}

function GetHomeDir() {
  // get home directory
  let homeDir = Services.dirsvc.get("Home", Ci.nsILocalFile);
  return (homeDir);
}

// Returns a file object for the file or directory named |name| in the
// profile directory.
function GetProfileEntry(name) {
  let entry = GetProfileDir();
  entry.append(name);
  return (entry);
}

function GetDir(path) {
  let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
  dir.initWithPath(path);
  Assert.ok(dir.isDirectory(), `${path} is a directory`);
  return (dir);
}

function GetDirFromEnvVariable(varName) {
  return GetDir(environment.get(varName));
}
