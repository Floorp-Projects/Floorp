/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

const LINUX = navigator.platform.startsWith("Linux");
const MAC = navigator.platform.startsWith("Mac");
const WIN = navigator.platform.startsWith("Win");
const MAC_106 = navigator.userAgent.contains("Mac OS X 10.6");

function checkFiles(files) {
  return Task.spawn(function*() {
    for (let file of files) {
      if (!(yield OS.File.exists(file))) {
        info("File doesn't exist: " + file);
        return false;
      }
    }

    return true;
  });
}

function checkDateHigherThan(files, date) {
  return Task.spawn(function*() {
    for (let file of files) {
      if (!(yield OS.File.exists(file))) {
        info("File doesn't exist: " + file);
        return false;
      }

      let stat = yield OS.File.stat(file);
      if (!(stat.lastModificationDate > date)) {
        info("File not newer: " + file);
        return false;
      }
    }

    return true;
  });
}

function wait(time) {
  let deferred = Promise.defer();

  setTimeout(function() {
    deferred.resolve();
  }, time);

  return deferred.promise;
}

// Helper to create a nsIFile from a set of path components
function getFile() {
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(OS.Path.join.apply(OS.Path, arguments));
  return file;
}

function setDryRunPref() {
  let old_dry_run;
  try {
    old_dry_run = Services.prefs.getBoolPref("browser.mozApps.installer.dry_run");
  } catch (ex) {}

  Services.prefs.setBoolPref("browser.mozApps.installer.dry_run", false);

  SimpleTest.registerCleanupFunction(function() {
    if (old_dry_run === undefined) {
      Services.prefs.clearUserPref("browser.mozApps.installer.dry_run");
    } else {
      Services.prefs.setBoolPref("browser.mozApps.installer.dry_run", old_dry_run);
    }
  });
}
