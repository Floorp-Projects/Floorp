add_task(async function() {
  // create mocked objects
  let launcher = createMockedObjects(true);

  // open helper app dialog with mocked launcher
  let dlg = await openHelperAppDialog(launcher);

  let doc = dlg.document;

  // Set remember choice
  ok(!doc.getElementById("rememberChoice").checked,
     "Remember choice checkbox should be not checked.");
  doc.getElementById("rememberChoice").checked = true;

  // Make sure the mock handler information is not in nsIHandlerService
  ok(!gHandlerSvc.exists(launcher.MIMEInfo), "Should not be in nsIHandlerService.");

  // close the dialog by pushing the ok button.
  let dialogClosedPromise = BrowserTestUtils.windowClosed(dlg);
  // Make sure the ok button is enabled, since the ok button might be disabled by
  // EnableDelayHelper mechanism. Please refer the detailed
  // https://dxr.mozilla.org/mozilla-central/source/toolkit/components/prompts/src/SharedPromptUtils.jsm#53
  doc.documentElement.getButton("accept").disabled = false;
  doc.documentElement.acceptDialog();
  await dialogClosedPromise;

  // check the mocked handler information is saved in nsIHandlerService
  ok(gHandlerSvc.exists(launcher.MIMEInfo), "Should be in nsIHandlerService.");
  // check the extension.
  var mimeType = gHandlerSvc.getTypeFromExtension("abc");
  is(mimeType, launcher.MIMEInfo.type, "Got correct mime type.");
  var handlerInfos = gHandlerSvc.enumerate();
  while (handlerInfos.hasMoreElements()) {
    let handlerInfo = handlerInfos.getNext().QueryInterface(Ci.nsIHandlerInfo);
    if (handlerInfo.type == launcher.MIMEInfo.type) {
      // check the alwaysAskBeforeHandling
      ok(!handlerInfo.alwaysAskBeforeHandling,
         "Should turn off the always ask.");
      // check the preferredApplicationHandler
      ok(handlerInfo.preferredApplicationHandler.equals(
         launcher.MIMEInfo.preferredApplicationHandler),
         "Should be equal to the mockedHandlerApp.");
      // check the perferredAction
      is(handlerInfo.preferredAction, launcher.MIMEInfo.preferredAction,
         "Should be equal to Ci.nsIHandlerInfo.useHelperApp.");
      break;
    }
  }
});
