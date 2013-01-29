/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the objects defined in the "DownloadCore" module.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Tests

/**
 * Executes a download, started by constructing the simplest Download object.
 */
add_task(function test_download_construction()
{
  let targetFile = getTempFile(TEST_TARGET_FILE_NAME);

  let download = yield Downloads.createDownload({
    source: { uri: TEST_SOURCE_URI },
    target: { file: targetFile },
    saver: { type: "copy" },
  });

  // Starts the download and waits for completion.
  yield download.start();

  yield promiseVerifyContents(targetFile, TEST_DATA_SHORT);
});
