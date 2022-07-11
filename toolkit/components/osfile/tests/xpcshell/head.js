/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

// Bug 1014484 can only be reproduced by loading OS.File first from the
// CommonJS loader, so we do not want OS.File to be loaded eagerly for
// all the tests in this directory.
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm"
);

Services.prefs.setBoolPref("toolkit.osfile.log", true);

/**
 * As add_task, but execute the test both with native operations and
 * without.
 */
function add_test_pair(generator) {
  add_task(async function() {
    info("Executing test " + generator.name + " with native operations");
    Services.prefs.setBoolPref("toolkit.osfile.native", true);
    return generator();
  });
  add_task(async function() {
    info("Executing test " + generator.name + " without native operations");
    Services.prefs.setBoolPref("toolkit.osfile.native", false);
    return generator();
  });
}

/**
 * Fetch asynchronously the contents of a file using xpcom.
 *
 * Used for comparing xpcom-based results to os.file-based results.
 *
 * @param {string} path The _absolute_ path to the file.
 * @return {promise}
 * @resolves {string} The contents of the file.
 */
function reference_fetch_file(path, test) {
  info("Fetching file " + path);
  return new Promise((resolve, reject) => {
    let file = new FileUtils.File(path);
    NetUtil.asyncFetch(
      {
        uri: NetUtil.newURI(file),
        loadUsingSystemPrincipal: true,
      },
      function(stream, status) {
        if (!Components.isSuccessCode(status)) {
          reject(status);
          return;
        }
        let result, reject;
        try {
          result = NetUtil.readInputStreamToString(stream, stream.available());
        } catch (x) {
          reject = x;
        }
        stream.close();
        if (reject) {
          reject(reject);
        } else {
          resolve(result);
        }
      }
    );
  });
}

/**
 * Compare asynchronously the contents two files using xpcom.
 *
 * Used for comparing xpcom-based results to os.file-based results.
 *
 * @param {string} a The _absolute_ path to the first file.
 * @param {string} b The _absolute_ path to the second file.
 *
 * @resolves {null}
 */
function reference_compare_files(a, b, test) {
  return (async function() {
    info("Comparing files " + a + " and " + b);
    let a_contents = await reference_fetch_file(a, test);
    let b_contents = await reference_fetch_file(b, test);
    Assert.equal(a_contents, b_contents);
  })();
}

async function removeTestFile(filePath, ignoreNoSuchFile = true) {
  try {
    await OS.File.remove(filePath);
  } catch (ex) {
    if (!ignoreNoSuchFile || !ex.becauseNoSuchFile) {
      do_throw(ex);
    }
  }
}
