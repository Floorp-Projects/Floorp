"use strict";

const IMAGE_PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/image_page.html";

var MockFilePicker = SpecialPowers.MockFilePicker;

MockFilePicker.init(window);
MockFilePicker.returnValue = MockFilePicker.returnCancel;

registerCleanupFunction(function() {
  MockFilePicker.cleanup();
});

function waitForFilePicker() {
  return new Promise(resolve => {
    MockFilePicker.showCallback = () => {
      MockFilePicker.showCallback = null;
      ok(true, "Saw the file picker");
      resolve();
    };
  });
}

/**
 * Test that internalSave works when saving an image like the context menu does.
 */
add_task(async function preferred_API() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: IMAGE_PAGE,
    },
    async function(browser) {
      let url = await SpecialPowers.spawn(browser, [], async function() {
        let image = content.document.getElementById("image");
        return image.href;
      });

      let filePickerPromise = waitForFilePicker();
      internalSave(
        url,
        null, // document
        "image.jpg",
        null, // content disposition
        "image/jpeg",
        true, // bypass cache
        null, // dialog title key
        null, // chosen data
        null, // no referrer info
        null, // no document
        false, // don't skip the filename prompt
        null, // cache key
        false, // not private.
        gBrowser.contentPrincipal
      );
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
        let channel = docShell.currentDocumentChannel;
        if (channel) {
          todo(
            channel.QueryInterface(Ci.nsIHttpChannelInternal)
              .channelIsForDownload
          );

          // Throttleable is the only class flag assigned to downloads.
          todo(
            channel.QueryInterface(Ci.nsIClassOfService).classFlags ==
              Ci.nsIClassOfService.Throttleable
          );
        }
      });
      await filePickerPromise;
    }
  );
});
