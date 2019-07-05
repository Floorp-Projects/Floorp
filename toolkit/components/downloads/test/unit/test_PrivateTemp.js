/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * The temporary directory downloads saves to, should be only readable
 * for the current user.
 */
add_task(async function test_private_temp() {
  let download = await promiseStartExternalHelperAppServiceDownload(
    httpUrl("empty.txt")
  );

  await promiseDownloadStopped(download);

  var targetFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  targetFile.initWithPath(download.target.path);

  // 488 is the decimal value of 0o700.
  equal(targetFile.parent.permissions, 448);
});
