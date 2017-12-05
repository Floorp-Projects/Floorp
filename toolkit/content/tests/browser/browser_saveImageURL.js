"use strict";

const IMAGE_PAGE = "https://example.com/browser/toolkit/content/tests/browser/image_page.html";
const PREF_UNSAFE_FORBIDDEN = "dom.ipc.cpows.forbid-unsafe-from-browser";

var MockFilePicker = SpecialPowers.MockFilePicker;

MockFilePicker.init(window);
MockFilePicker.returnValue = MockFilePicker.returnCancel;

registerCleanupFunction(function() {
  MockFilePicker.cleanup();
});

function waitForFilePicker() {
  return new Promise((resolve) => {
    MockFilePicker.showCallback = () => {
      MockFilePicker.showCallback = null;
      ok(true, "Saw the file picker");
      resolve();
    };
  });
}

/**
 * Test that saveImageURL works when we pass in the aIsContentWindowPrivate
 * argument instead of a document. This is the preferred API.
 */
add_task(async function preferred_API() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: IMAGE_PAGE,
  }, async function(browser) {
    let url = await ContentTask.spawn(browser, null, async function() {
      let image = content.document.getElementById("image");
      return image.href;
    });

    saveImageURL(url, "image.jpg", null, true, false, null, null, null, null, false);
    // eslint-disable-next-line mozilla/no-cpows-in-tests
    let channel = gBrowser.contentDocumentAsCPOW.docShell.currentDocumentChannel;
    if (channel) {
      ok(true, channel.QueryInterface(Ci.nsIHttpChannelInternal)
                      .channelIsForDownload);

      // Throttleable is the only class flag assigned to downloads.
      ok(channel.QueryInterface(Ci.nsIClassOfService).classFlags,
         Ci.nsIClassOfService.Throttleable);
    }
    await waitForFilePicker();
  });
});

/**
 * Test that saveImageURL will still work when passed a document instead
 * of the aIsContentWindowPrivate argument. This is the deprecated API, and
 * will not work in apps using remote browsers having PREF_UNSAFE_FORBIDDEN
 * set to true.
 */
add_task(async function deprecated_API() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: IMAGE_PAGE,
  }, async function(browser) {
    await pushPrefs([PREF_UNSAFE_FORBIDDEN, false]);

    let url = await ContentTask.spawn(browser, null, async function() {
      let image = content.document.getElementById("image");
      return image.href;
    });

    // Now get the document directly from content. If we run this test with
    // e10s-enabled, this will be a CPOW, which is forbidden. We'll just
    // pass the XUL document instead to test this interface.
    let doc = document;

    // eslint-disable-next-line mozilla/no-cpows-in-tests
    let channel = gBrowser.contentDocumentAsCPOW.docShell.currentDocumentChannel;
    if (channel) {
      ok(true, channel.QueryInterface(Ci.nsIHttpChannelInternal)
                      .channelIsForDownload);

      // Throttleable is the only class flag assigned to downloads.
      ok(channel.QueryInterface(Ci.nsIClassOfService).classFlags,
         Ci.nsIClassOfService.Throttleable);
    }
    saveImageURL(url, "image.jpg", null, true, false, null, doc, null, null);
    await waitForFilePicker();
  });
});
