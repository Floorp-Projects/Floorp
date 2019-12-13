const TESTPAGE = `${SECURE_TESTROOT}webapi_checkavailable.html`;
const XPI_URL = `${SECURE_TESTROOT}../xpinstall/amosigned.xpi`;

function waitForClear() {
  const MSG = "WebAPICleanup";
  return new Promise(resolve => {
    let listener = {
      receiveMessage(msg) {
        if (msg.name == MSG) {
          Services.mm.removeMessageListener(MSG, listener);
          resolve();
        }
      },
    };

    Services.mm.addMessageListener(MSG, listener, true);
  });
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.webapi.testing", true],
      ["xpinstall.enabled", false],
      ["extensions.install.requireBuiltInCerts", false],
    ],
  });
  info("added preferences");
});

async function testInstall(browser, args) {
  let success = await SpecialPowers.spawn(browser, [{ args }], async function(
    opts
  ) {
    let { args } = opts;
    let install;
    try {
      install = await content.navigator.mozAddonManager.createInstall(args);
    } catch (e) {}
    return !!install;
  });
  is(success, false, "Install was blocked");
}

add_task(async function() {
  // withNewTab() will close the test tab before returning, at which point
  // the cleanup event will come from the content process.  We need to see
  // that event but don't want to race to install a listener for it after
  // the tab is closed.  So set up the listener now but don't yield the
  // listening promise until below.
  let clearPromise = waitForClear();

  await BrowserTestUtils.withNewTab(TESTPAGE, async function(browser) {
    await testInstall(browser, { url: XPI_URL });
  });

  await clearPromise;
});
