/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Provides infrastructure for automated download components tests.
 */

"use strict";

// Globals

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "DownloadPaths",
  "resource://gre/modules/DownloadPaths.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Downloads",
  "resource://gre/modules/Downloads.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "HttpServer",
  "resource://testing-common/httpd.js"
);
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "FileTestUtils",
  "resource://testing-common/FileTestUtils.jsm"
);

const TEST_TARGET_FILE_NAME_PDF = "test-download.pdf";

// Support functions

/**
 * Returns a reference to a temporary file that is guaranteed not to exist and
 * is cleaned up later. See FileTestUtils.getTempFile for details.
 */
function getTempFile(leafName) {
  return FileTestUtils.getTempFile(leafName);
}

function promiseBrowserLoaded(browser) {
  return new Promise(resolve => {
    browser.addEventListener(
      "load",
      function onLoad(event) {
        if (event.target == browser.contentDocument) {
          browser.removeEventListener("load", onLoad, true);
          resolve();
        }
      },
      true
    );
  });
}
