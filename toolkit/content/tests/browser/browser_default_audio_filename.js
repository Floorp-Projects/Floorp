/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);
registerCleanupFunction(function() {
  MockFilePicker.cleanup();
});

/**
 * TestCase for bug 789550
 * <https://bugzilla.mozilla.org/show_bug.cgi?id=789550>
 */
add_task(async function() {
  const DATA_AUDIO_URL = await fetch(
    getRootDirectory(gTestPath) + "audio_file.txt"
  ).then(async response => response.text());
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: DATA_AUDIO_URL,
    },
    async function(browser) {
      let popupShownPromise = BrowserTestUtils.waitForEvent(
        document,
        "popupshown"
      );

      await BrowserTestUtils.synthesizeMouseAtCenter(
        "video",
        {
          type: "contextmenu",
          button: 2,
        },
        browser
      );

      await popupShownPromise;

      let showFilePickerPromise = new Promise(resolve => {
        MockFilePicker.showCallback = function(fp) {
          is(
            fp.defaultString.startsWith("index"),
            true,
            "File name should be index"
          );
          resolve();
        };
      });

      // Select "Save Audio As" option from context menu
      var saveImageAsCommand = document.getElementById("context-saveaudio");
      saveImageAsCommand.doCommand();

      await showFilePickerPromise;

      let contextMenu = document.getElementById("contentAreaContextMenu");
      let popupHiddenPromise = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popuphidden"
      );
      contextMenu.hidePopup();
      await popupHiddenPromise;
    }
  );
});

/**
 * TestCase for bug 789550
 * <https://bugzilla.mozilla.org/show_bug.cgi?id=789550>
 */
add_task(async function() {
  const DATA_AUDIO_URL = await fetch(
    getRootDirectory(gTestPath) + "audio_file.txt"
  ).then(async response => response.text());
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: DATA_AUDIO_URL,
    },
    async function(browser) {
      let showFilePickerPromise = new Promise(resolve => {
        MockFilePicker.showCallback = function(fp) {
          is(
            fp.defaultString.startsWith("index"),
            true,
            "File name should be index"
          );
          resolve();
        };
      });

      saveBrowser(browser);

      await showFilePickerPromise;
    }
  );
});
