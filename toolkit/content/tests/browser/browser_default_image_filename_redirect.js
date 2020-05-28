/**
 * TestCase for bug 1406253
 * <https://bugzilla.mozilla.org/show_bug.cgi?id=1406253>
 *
 * Load firebird.png, redirect it to doggy.png, and verify the filename is
 * doggy.png in file picker dialog.
 */

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);
add_task(async function() {
  // This URL will redirect to doggy.png.
  const URL_FIREBIRD =
    "http://mochi.test:8888/browser/toolkit/content/tests/browser/firebird.png";

  await BrowserTestUtils.withNewTab(URL_FIREBIRD, async function(browser) {
    // Click image to show context menu.
    let popupShownPromise = BrowserTestUtils.waitForEvent(
      document,
      "popupshown"
    );
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "img",
      { type: "contextmenu", button: 2 },
      browser
    );
    await popupShownPromise;

    // Prepare mock file picker.
    let showFilePickerPromise = new Promise(resolve => {
      MockFilePicker.showCallback = fp => resolve(fp.defaultString);
    });
    registerCleanupFunction(function() {
      MockFilePicker.cleanup();
    });

    // Select "Save Image As" option from context menu
    var saveImageAsCommand = document.getElementById("context-saveimage");
    saveImageAsCommand.doCommand();

    let filename = await showFilePickerPromise;
    is(filename, "doggy.png", "Verify image filename.");

    // Close context menu.
    let contextMenu = document.getElementById("contentAreaContextMenu");
    let popupHiddenPromise = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popuphidden"
    );
    contextMenu.hidePopup();
    await popupHiddenPromise;
  });
});
