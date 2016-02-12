"use strict";

const IMAGE_PAGE = "https://example.com/browser/toolkit/content/tests/browser/image_page.html";
const PREF_UNSAFE_FORBIDDEN = "dom.ipc.cpows.forbid-unsafe-from-browser";

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
    }
  })
}

/**
 * Test that saveImageURL works when we pass in the aIsContentWindowPrivate
 * argument instead of a document. This is the preferred API.
 */
add_task(function* preferred_API() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: IMAGE_PAGE,
  }, function*(browser) {
    let url = yield ContentTask.spawn(browser, null, function*() {
      let image = content.document.getElementById("image");
      return image.href;
    });

    saveImageURL(url, "image.jpg", null, true, false, null, null, null, null, false);
    yield waitForFilePicker();
  });
});

/**
 * Test that saveImageURL will still work when passed a document instead
 * of the aIsContentWindowPrivate argument. This is the deprecated API, and
 * will not work in apps using remote browsers having PREF_UNSAFE_FORBIDDEN
 * set to true.
 */
add_task(function* deprecated_API() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: IMAGE_PAGE,
  }, function*(browser) {
    yield pushPrefs([PREF_UNSAFE_FORBIDDEN, false]);

    let url = yield ContentTask.spawn(browser, null, function*() {
      let image = content.document.getElementById("image");
      return image.href;
    });

    // Now get the document directly from content. If we run this test with
    // e10s-enabled, this will be a CPOW, which is forbidden. We'll just
    // pass the XUL document instead to test this interface.
    let doc = document;

    saveImageURL(url, "image.jpg", null, true, false, null, doc, null, null);
    yield waitForFilePicker();
  });
});
