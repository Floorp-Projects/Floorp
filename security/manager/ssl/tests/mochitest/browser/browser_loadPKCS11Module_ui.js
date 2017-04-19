// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests the dialog used for loading PKCS #11 modules.

const { MockRegistrar } =
  Cu.import("resource://testing-common/MockRegistrar.jsm", {});

const gMockPKCS11 = {
  addModuleCallCount: 0,
  expectedLibPath: "",
  expectedModuleName: "",
  throwOnAddModule: false,

  addModule(moduleName, libraryFullPath, cryptoMechanismFlags, cipherFlags) {
    this.addModuleCallCount++;
    Assert.equal(moduleName, this.expectedModuleName,
                 "addModule: Name given should be what's in the name textbox");
    Assert.equal(libraryFullPath, this.expectedLibPath,
                 "addModule: Path given should be what's in the path textbox");
    Assert.equal(cryptoMechanismFlags, 0,
                 "addModule: No crypto mechanism flags should be passed");
    Assert.equal(cipherFlags, 0,
                 "addModule: No cipher flags should be passed");

    if (this.throwOnAddModule) {
      throw new Error(`addModule: Throwing exception`);
    }
  },

  deleteModule(moduleName) {
    Assert.ok(false, `deleteModule: should not be called`);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPKCS11])
};

const gMockPromptService = {
  alertCallCount: 0,
  expectedText: "",
  expectedWindow: null,

  alert(parent, dialogTitle, text) {
    this.alertCallCount++;
    Assert.equal(parent, this.expectedWindow,
                 "alert: Parent should be expected window");
    Assert.equal(dialogTitle, null, "alert: Title should be null");
    Assert.equal(text, this.expectedText,
                 "alert: Actual and expected text should match");
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPromptService])
};

var gMockPKCS11CID =
  MockRegistrar.register("@mozilla.org/security/pkcs11;1",
                         gMockPKCS11);
var gMockPromptServiceCID =
  MockRegistrar.register("@mozilla.org/embedcomp/prompt-service;1",
                         gMockPromptService);

var gMockFilePicker = SpecialPowers.MockFilePicker;
gMockFilePicker.init(window);

var gTempFile = Cc["@mozilla.org/file/directory_service;1"]
                  .getService(Ci.nsIProperties)
                  .get("TmpD", Ci.nsIFile);
gTempFile.append("browser_loadPKCS11Module_ui-fakeModule");

registerCleanupFunction(() => {
  gMockFilePicker.cleanup();
  MockRegistrar.unregister(gMockPKCS11CID);
  MockRegistrar.unregister(gMockPromptServiceCID);
});

function resetCallCounts() {
  gMockPKCS11.addModuleCallCount = 0;
  gMockPromptService.alertCallCount = 0;
}

/**
 * Opens the dialog shown to load a PKCS #11 module.
 *
 * @returns {Promise}
 *          A promise that resolves when the dialog has finished loading, with
 *          the window of the opened dialog.
 */
function openLoadModuleDialog() {
  let win = window.openDialog("chrome://pippki/content/load_device.xul", "", "");
  return new Promise(resolve => {
    win.addEventListener("load", function() {
      resolve(win);
    }, {once: true});
  });
}

/**
 * Presses the browse button and simulates interacting with the file picker that
 * should be triggered.
 *
 * @param {window} win
 *        The dialog window.
 * @param {Boolean} cancel
 *        If true, the file picker is canceled. If false, gTempFile is chosen in
 *        the file picker and the file picker is accepted.
 */
async function browseToTempFile(win, cancel) {
  gMockFilePicker.showCallback = () => {
    gMockFilePicker.setFiles([gTempFile]);

    if (cancel) {
      info("MockFilePicker returning cancel");
      return Ci.nsIFilePicker.returnCancel;
    }

    info("MockFilePicker returning OK");
    return Ci.nsIFilePicker.returnOK;
  };

  info("Pressing browse button");
  win.document.getElementById("browse").doCommand();
  await TestUtils.topicObserved("LoadPKCS11Module:FilePickHandled");
}

add_task(async function testBrowseButton() {
  let win = await openLoadModuleDialog();
  let pathBox = win.document.getElementById("device_path");
  let originalPathBoxValue = "expected path if picker is canceled";
  pathBox.value = originalPathBoxValue;

  // Test what happens if the file picker is canceled.
  await browseToTempFile(win, true);
  Assert.equal(pathBox.value, originalPathBoxValue,
               "Path shown should be unchanged due to canceled picker");

  // Test what happens if the file picker is not canceled.
  await browseToTempFile(win, false);
  Assert.equal(pathBox.value, gTempFile.path,
               "Path shown should be same as the one chosen in the file picker");

  await BrowserTestUtils.closeWindow(win);
});

function testAddModuleHelper(win, throwOnAddModule) {
  resetCallCounts();
  gMockPKCS11.expectedLibPath = gTempFile.path;
  gMockPKCS11.expectedModuleName = "test module";
  gMockPKCS11.throwOnAddModule = throwOnAddModule;

  win.document.getElementById("device_name").value =
    gMockPKCS11.expectedModuleName;
  win.document.getElementById("device_path").value =
    gMockPKCS11.expectedLibPath;

  info("Accepting dialog");
  win.document.getElementById("loaddevice").acceptDialog();
}

add_task(async function testAddModuleSuccess() {
  let win = await openLoadModuleDialog();

  testAddModuleHelper(win, false);
  await BrowserTestUtils.windowClosed(win);

  Assert.equal(gMockPKCS11.addModuleCallCount, 1,
               "addModule() should have been called once");
  Assert.equal(gMockPromptService.alertCallCount, 0,
               "alert() should never have been called");
});

add_task(async function testAddModuleFailure() {
  let win = await openLoadModuleDialog();
  gMockPromptService.expectedText = "Unable to add module";
  gMockPromptService.expectedWindow = win;

  testAddModuleHelper(win, true);
  // If adding a module fails, the dialog will not close. As such, we have to
  // close the window ourselves.
  await BrowserTestUtils.closeWindow(win);

  Assert.equal(gMockPKCS11.addModuleCallCount, 1,
               "addModule() should have been called once");
  Assert.equal(gMockPromptService.alertCallCount, 1,
               "alert() should have been called once");
});

add_task(async function testCancel() {
  let win = await openLoadModuleDialog();
  resetCallCounts();

  info("Canceling dialog");
  win.document.getElementById("loaddevice").cancelDialog();

  Assert.equal(gMockPKCS11.addModuleCallCount, 0,
               "addModule() should never have been called");
  Assert.equal(gMockPromptService.alertCallCount, 0,
               "alert() should never have been called");

  await BrowserTestUtils.windowClosed(win);
});
