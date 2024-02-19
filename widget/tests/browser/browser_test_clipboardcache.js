/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Note: widget/tests/test_bug1123480.xhtml checks whether nsTransferable behaves
// as expected with regards to private browsing mode and the clipboard cache,
// i.e. that the clipboard is not cached to the disk when private browsing mode
// is enabled.
//
// This test tests that the clipboard is not cached to the disk by IPC,
// as a regression test for bug 1396224.
// It indirectly uses nsTransferable, via the async navigator.clipboard API.

// Create over 1 MB of sample garbage text. JavaScript strings are represented
// by UTF16 strings, so the size is twice as much as the actual string length.
// This value is chosen such that the size of the memory for the string exceeds
// the kLargeDatasetSize threshold in nsTransferable.h.
// It is also not a round number to reduce the odds of having an accidental
// collisions with another file (since the test below looks at the file size
// to identify the file).
var Ipsum = "0123456789".repeat(1234321);
var IpsumByteLength = Ipsum.length * 2;
var SHORT_STRING_NO_CACHE = "short string that will not be cached to the disk";

// Get a list of open file descriptors that refer to a file with the same size
// as the expected data (and assume that any mutations in file descriptor counts
// are caused by our test).
// TODO: This logic only counts file descriptors that are still open (e.g. when
// data persists after a copy). It does not detect cache files that exist only
// temporarily (e.g. after a paste).
function getClipboardCacheFDCount() {
  let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  if (AppConstants.platform === "win") {
    // On Windows, nsAnonymousTemporaryFile does not immediately delete a file.
    // Instead, the Windows-specific FILE_FLAG_DELETE_ON_CLOSE flag is used,
    // which means that the file is deleted when the last handle is closed.
    // Apparently, this flag is unreliable (e.g. when the application crashes),
    // so nsAnonymousTemporaryFile stores the temporary files in a subdirectory,
    // which is cleaned up some time after start-up.

    // This is just a test, and during the test we deterministically close the
    // handles, so if FILE_FLAG_DELETE_ON_CLOSE does the thing it promises, the
    // file is actually removed when the handle is closed.

    // Path from nsAnonymousTemporaryFile.cpp, GetTempDir.
    dir.initWithPath(PathUtils.join(PathUtils.tempDir, "mozilla-temp-files"));
  } else {
    dir.initWithPath("/dev/fd");
  }
  let count = 0;
  for (let fdFile of dir.directoryEntries) {
    let fileSize;
    try {
      fileSize = fdFile.fileSize;
    } catch (e) {
      // This can happen on macOS.
      continue;
    }
    if (fileSize === IpsumByteLength) {
      // Assume that the file was created by us if the size matches.
      ++count;
    }
  }
  return count;
}

async function testCopyPaste(isPrivate) {
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: isPrivate });
  let tab = await BrowserTestUtils.openNewForegroundTab(win);
  let browser = tab.linkedBrowser;

  // Sanitize environment
  await ContentTask.spawn(browser, SHORT_STRING_NO_CACHE, async shortStr => {
    await content.navigator.clipboard.writeText(shortStr);
  });

  let initialFdCount = getClipboardCacheFDCount();

  await SpecialPowers.spawn(browser, [Ipsum], async largeString => {
    await content.navigator.clipboard.writeText(largeString);
  });

  let fdCountAfterCopy = getClipboardCacheFDCount();
  if (isPrivate) {
    is(fdCountAfterCopy, initialFdCount, "Private write");
  } else {
    is(fdCountAfterCopy, initialFdCount + 1, "Cached write");
  }

  let readStr = await SpecialPowers.spawn(browser, [], async () => {
    let { document } = content;
    document.body.contentEditable = true;
    document.body.focus();
    let pastePromise = new Promise(resolve => {
      document.addEventListener(
        "paste",
        e => {
          resolve(e.clipboardData.getData("text/plain"));
        },
        { once: true }
      );
    });
    document.execCommand("paste");
    return pastePromise;
  });

  // Don't use Assert.strictEqual here because the test starts timing out,
  // because the logging creates lots of copies of this very huge string.
  // eslint-disable-next-line mozilla/no-comparison-or-assignment-inside-ok
  ok(readStr === Ipsum, "Read what we pasted");

  if (isPrivate) {
    is(getClipboardCacheFDCount(), fdCountAfterCopy, "Private read");
  } else {
    // Upon reading from the clipboard, a temporary nsTransferable is used, for
    // which the cache is disabled. The content process does not cache clipboard
    // data either. So the file descriptor count should be identical.
    is(getClipboardCacheFDCount(), fdCountAfterCopy, "Read not cached");
  }

  // Cleanup.
  await SpecialPowers.spawn(
    browser,
    [SHORT_STRING_NO_CACHE],
    async shortStr => {
      await content.navigator.clipboard.writeText(shortStr);
    }
  );
  is(getClipboardCacheFDCount(), initialFdCount, "Drop clipboard cache if any");

  BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.closeWindow(win);
}

add_task(async function test_private() {
  await testCopyPaste(true);
});

add_task(async function test_non_private() {
  await testCopyPaste(false);
});
