/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);
const DATA_IMAGE_GIF_URL =
  "data:image/gif;base64,R0lGODlhEAAOALMAAOazToeHh0tLS/7LZv/0jvb29t/f3//Ub//ge8WSLf/rhf/3kdbW1mxsbP//mf///yH5BAAAAAAALAAAAAAQAA4AAARe8L1Ekyky67QZ1hLnjM5UUde0ECwLJoExKcppV0aCcGCmTIHEIUEqjgaORCMxIC6e0CcguWw6aFjsVMkkIr7g77ZKPJjPZqIyd7sJAgVGoEGv2xsBxqNgYPj/gAwXEQA7";
registerCleanupFunction(function() {
  MockFilePicker.cleanup();
});
/**
 * TestCase for bug 564387
 * <https://bugzilla.mozilla.org/show_bug.cgi?id=564387>
 */
add_task(async function() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: DATA_IMAGE_GIF_URL,
    },
    async function(browser) {
      let popupShownPromise = BrowserTestUtils.waitForEvent(
        document,
        "popupshown"
      );

      await BrowserTestUtils.synthesizeMouseAtCenter(
        "img",
        {
          type: "contextmenu",
          button: 2,
        },
        browser
      );

      await popupShownPromise;

      let showFilePickerPromise = new Promise(resolve => {
        MockFilePicker.showCallback = function(fp) {
          is(fp.defaultString, "index.gif");
          resolve();
        };
      });

      // Select "Save Image As" option from context menu
      var saveImageAsCommand = document.getElementById("context-saveimage");
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
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: DATA_IMAGE_GIF_URL,
    },
    async function(browser) {
      let showFilePickerPromise = new Promise(resolve => {
        MockFilePicker.showCallback = function(fp) {
          is(fp.defaultString, "index.gif");
          resolve();
        };
      });

      saveBrowser(browser);

      await showFilePickerPromise;
    }
  );
});
