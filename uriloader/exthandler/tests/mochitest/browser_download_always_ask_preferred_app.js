add_task(function*() {
  // Create mocked objects for test
  let launcher = createMockedObjects(false);
  // Open helper app dialog with mocked launcher
  let dlg = yield* openHelperAppDialog(launcher);
  let doc = dlg.document;
  let location = doc.getElementById("source");
  let expectedValue = launcher.source.prePath;
  if (location.value != expectedValue) {
    info("Waiting for dialog to be populated.");
    yield BrowserTestUtils.waitForAttribute("value", location, expectedValue);
  }
  is(doc.getElementById("mode").selectedItem.id, "open", "Should be opening the file.");
  ok(!dlg.document.getElementById("openHandler").selectedItem.hidden,
     "Should not have selected a hidden item.");
  let helperAppDialogHiddenPromise = BrowserTestUtils.windowClosed(dlg);
  doc.documentElement.cancelDialog();
  yield helperAppDialogHiddenPromise;
});
