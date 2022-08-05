// This functionality covered in this test is also covered in other tests.
// The purpose of this test is to catch window leaks.  It should fail in
// debug builds if a window reference is held onto after an install finishes.
// See bug 1541577 for further details.

let win;
let popupPromise;
let newtabPromise;
const exampleURI = Services.io.newURI("http://example.com");
async function test() {
  waitForExplicitFinish(); // have to call this ourselves because we're async.

  // This test currently depends on InstallTrigger.install availability.
  setInstallTriggerPrefs();

  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", false]],
  });
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install => {
    return install.addon.uninstall();
  };
  Harness.installsCompletedCallback = finish_test;
  Harness.finalContentEvent = "InstallComplete";
  win = await BrowserTestUtils.openNewBrowserWindow();
  Harness.setup(win);

  PermissionTestUtils.add(exampleURI, "install", Services.perms.ALLOW_ACTION);

  const triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI": {
        URL: TESTROOT + "amosigned.xpi",
        IconURL: TESTROOT + "icon.png",
      },
    })
  );

  const url = `${TESTROOT}installtrigger.html?${triggers}`;
  newtabPromise = BrowserTestUtils.openNewForegroundTab(win.gBrowser, url);
  popupPromise = BrowserTestUtils.waitForEvent(
    win.PanelUI.notificationPanel,
    "popupshown"
  );
}

function confirm_install(panel) {
  is(panel.getAttribute("name"), "XPI Test", "Should have seen the name");
  return true;
}

async function finish_test(count) {
  is(count, 1, "1 Add-on should have been successfully installed");

  PermissionTestUtils.remove(exampleURI, "install");

  const results = await SpecialPowers.spawn(
    win.gBrowser.selectedBrowser,
    [],
    () => {
      return {
        return: content.document.getElementById("return").textContent,
        status: content.document.getElementById("status").textContent,
      };
    }
  );

  is(results.return, "true", "installTrigger should have claimed success");
  is(results.status, "0", "Callback should have seen a success");

  // Explicitly click the "OK" button to avoid the panel reopening in the other window once this
  // window closes (see also bug 1535069):
  await popupPromise;
  win.PanelUI.notificationPanel
    .querySelector("popupnotification[popupid=addon-installed]")
    .button.click();

  // Wait for the promise returned by BrowserTestUtils.openNewForegroundTab
  // to be resolved before removing the window to prevent an uncaught exception
  // triggered from inside openNewForegroundTab to trigger a test failure due
  // to a race between openNewForegroundTab and closeWindow calls, e.g. as for
  // Bug 1728482).
  await newtabPromise;

  // Now finish the test:
  await BrowserTestUtils.closeWindow(win);
  Harness.finish(win);
  win = null;
}
