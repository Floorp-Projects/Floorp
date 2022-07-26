const ALLOWED = 0;
const BLOCKED = 1;

async function cleanupData() {
  await new Promise(resolve => {
    const chromeScript = SpecialPowers.loadChromeScript(_ => {
      /* eslint-env mozilla/chrome-script */
      addMessageListener("go", __ => {
        Services.clearData.deleteData(
          Services.clearData.CLEAR_COOKIES |
            Services.clearData.CLEAR_ALL_CACHES |
            Services.clearData.CLEAR_DOM_STORAGES,
          ___ => {
            sendAsyncMessage("done");
          }
        );
      });
    });

    chromeScript.addMessageListener("done", _ => {
      chromeScript.destroy();
      resolve();
    });

    chromeScript.sendAsyncMessage("go");
  });
}

async function checkLastRequest(type, state) {
  let json = await fetch("cookie.sjs?last&" + Math.random()).then(r =>
    r.json()
  );
  is(json.type, type, "Type: " + type);
  is(json.hasCookie, state == ALLOWED, "Fetch has cookies");
}

async function runTests(currentTest) {
  await cleanupData();
  await SpecialPowers.pushPrefEnv({
    set: [["network.cookie.cookieBehavior", 2]],
  });
  let windowBlocked = window.open("cookie.sjs?window&" + Math.random());
  await new Promise(resolve => {
    windowBlocked.onload = resolve;
  });
  await currentTest(windowBlocked, BLOCKED);
  windowBlocked.close();

  await cleanupData();
  await SpecialPowers.pushPrefEnv({
    set: [["network.cookie.cookieBehavior", 1]],
  });
  let windowAllowed = window.open("cookie.sjs?window&" + Math.random());
  await new Promise(resolve => {
    windowAllowed.onload = resolve;
  });
  await currentTest(windowAllowed, ALLOWED);
  windowAllowed.close();

  SimpleTest.finish();
}

SimpleTest.waitForExplicitFinish();
