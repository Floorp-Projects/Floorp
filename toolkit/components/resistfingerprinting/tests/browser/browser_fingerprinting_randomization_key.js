let { ForgetAboutSite } = ChromeUtils.importESModule(
  "resource://gre/modules/ForgetAboutSite.sys.mjs"
);

requestLongerTimeout(2);

const TEST_DOMAIN = "https://example.com";
const TEST_DOMAIN_ANOTHER = "https://example.org";
const TEST_DOMAIN_THIRD = "https://example.net";

const TEST_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    TEST_DOMAIN
  ) + "testPage.html";
const TEST_DOMAIN_ANOTHER_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    TEST_DOMAIN_ANOTHER
  ) + "testPage.html";
const TEST_DOMAIN_THIRD_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    TEST_DOMAIN_THIRD
  ) + "testPage.html";

/**
 * A helper function to get the random key in a hex string format and test if
 * the random key works properly.
 *
 * @param {Browser} browser The browser element of the testing tab.
 * @param {string} firstPartyDomain The first-party domain loaded on the tab
 * @param {string} thirdPartyDomain The third-party domain to test
 * @returns {string} The random key hex string
 */
async function getRandomKeyHexFromBrowser(
  browser,
  firstPartyDomain,
  thirdPartyDomain
) {
  // Get the key from the cookieJarSettings of the browser element.
  let key = browser.cookieJarSettings.fingerprintingRandomizationKey;
  let keyHex = key.map(bytes => bytes.toString(16).padStart(2, "0")).join("");

  // Get the key from the cookieJarSettings of the top-level document.
  let keyTop = await SpecialPowers.spawn(browser, [], _ => {
    return content.document.cookieJarSettings.fingerprintingRandomizationKey;
  });
  let keyTopHex = keyTop
    .map(bytes => bytes.toString(16).padStart(2, "0"))
    .join("");

  is(
    keyTopHex,
    keyHex,
    "The fingerprinting random key should match between the browser element and the top-level document."
  );

  // Get the key from the cookieJarSettings of an about:blank iframe.
  let keyAboutBlank = await SpecialPowers.spawn(browser, [], async _ => {
    let ifr = content.document.createElement("iframe");

    let loaded = new content.Promise(resolve => {
      ifr.onload = resolve;
    });
    content.document.body.appendChild(ifr);
    ifr.src = "about:blank";

    await loaded;

    return SpecialPowers.spawn(ifr, [], _ => {
      return content.document.cookieJarSettings.fingerprintingRandomizationKey;
    });
  });

  let keyAboutBlankHex = keyAboutBlank
    .map(bytes => bytes.toString(16).padStart(2, "0"))
    .join("");
  is(
    keyAboutBlankHex,
    keyHex,
    "The fingerprinting random key should match between the browser element and the about:blank iframe document."
  );

  // Get the key from the cookieJarSettings of the javascript URL iframe
  // document.
  let keyJavascriptURL = await SpecialPowers.spawn(browser, [], async _ => {
    let ifr = content.document.getElementById("testFrame");

    return ifr.contentDocument.cookieJarSettings.fingerprintingRandomizationKey;
  });

  let keyJavascriptURLHex = keyJavascriptURL
    .map(bytes => bytes.toString(16).padStart(2, "0"))
    .join("");
  is(
    keyJavascriptURLHex,
    keyHex,
    "The fingerprinting random key should match between the browser element and the javascript URL iframe document."
  );

  // Get the key from the cookieJarSettings of an first-party iframe.
  let keyFirstPartyFrame = await SpecialPowers.spawn(
    browser,
    [firstPartyDomain],
    async domain => {
      let ifr = content.document.createElement("iframe");

      let loaded = new content.Promise(resolve => {
        ifr.onload = resolve;
      });
      content.document.body.appendChild(ifr);
      ifr.src = domain;

      await loaded;

      return SpecialPowers.spawn(ifr, [], _ => {
        return content.document.cookieJarSettings
          .fingerprintingRandomizationKey;
      });
    }
  );

  let keyFirstPartyFrameHex = keyFirstPartyFrame
    .map(bytes => bytes.toString(16).padStart(2, "0"))
    .join("");
  is(
    keyFirstPartyFrameHex,
    keyHex,
    "The fingerprinting random key should match between the browser element and the first-party iframe document."
  );

  // Get the key from the cookieJarSettings of an third-party iframe
  let keyThirdPartyFrame = await SpecialPowers.spawn(
    browser,
    [thirdPartyDomain],
    async domain => {
      let ifr = content.document.createElement("iframe");

      let loaded = new content.Promise(resolve => {
        ifr.onload = resolve;
      });
      content.document.body.appendChild(ifr);
      ifr.src = domain;

      await loaded;

      return SpecialPowers.spawn(ifr, [], _ => {
        return content.document.cookieJarSettings
          .fingerprintingRandomizationKey;
      });
    }
  );

  let keyThirdPartyFrameHex = keyThirdPartyFrame
    .map(bytes => bytes.toString(16).padStart(2, "0"))
    .join("");
  is(
    keyThirdPartyFrameHex,
    keyHex,
    "The fingerprinting random key should match between the browser element and the third-party iframe document."
  );

  return keyHex;
}

// Test accessing the fingerprinting randomization key will throw if
// fingerprinting resistance is disabled.
add_task(async function test_randomization_disabled_with_rfp_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting", false],
      ["privacy.resistFingerprinting.pbmode", false],
      ["privacy.fingerprintingProtection", false],
      ["privacy.fingerprintingProtection.pbmode", false],
    ],
  });

  // Ensure accessing the fingerprinting randomization key of the browser
  // element will throw if fingerprinting randomization is disabled.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);

  try {
    let key =
      tab.linkedBrowser.cookieJarSettings.fingerprintingRandomizationKey;
    ok(
      false,
      `Accessing the fingerprinting randomization key should throw when fingerprinting resistance is disabled. ${key}`
    );
  } catch (e) {
    ok(
      true,
      "It should throw when getting the key when fingerprinting resistance is disabled."
    );
  }

  // Ensure accessing the fingerprinting randomization key of the top-level
  // document will throw if fingerprinting randomization is disabled.
  try {
    await SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
      return content.document.cookieJarSettings.fingerprintingRandomizationKey;
    });
  } catch (e) {
    ok(
      true,
      "It should throw when getting the key when fingerprinting resistance is disabled."
    );
  }

  BrowserTestUtils.removeTab(tab);
});

// Test the fingerprinting randomization key generation.
add_task(async function test_generate_randomization_key() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.resistFingerprinting", true]],
  });

  for (let testPrivateWin of [true, false]) {
    let win = window;

    if (testPrivateWin) {
      win = await BrowserTestUtils.openNewBrowserWindow({
        private: true,
      });
    }

    let tabOne = await BrowserTestUtils.openNewForegroundTab(
      win.gBrowser,
      TEST_PAGE
    );
    let keyHexOne;

    try {
      keyHexOne = await getRandomKeyHexFromBrowser(
        tabOne.linkedBrowser,
        TEST_PAGE,
        TEST_DOMAIN_THIRD_PAGE
      );
      ok(true, `The fingerprinting random key: ${keyHexOne}`);
    } catch (e) {
      ok(
        false,
        "Shouldn't fail when getting the random key from the cookieJarSettings"
      );
    }

    // Open the test domain again and check if the key remains the same.
    let tabTwo = await BrowserTestUtils.openNewForegroundTab(
      win.gBrowser,
      TEST_PAGE
    );
    try {
      let keyHexTwo = await getRandomKeyHexFromBrowser(
        tabTwo.linkedBrowser,
        TEST_PAGE,
        TEST_DOMAIN_THIRD_PAGE
      );
      is(
        keyHexTwo,
        keyHexOne,
        `The key should remain the same after reopening the tab.`
      );
    } catch (e) {
      ok(
        false,
        "Shouldn't fail when getting the random key from the cookieJarSettings"
      );
    }

    // Open a tab with a different domain to see if the key changes.
    let tabAnother = await BrowserTestUtils.openNewForegroundTab(
      win.gBrowser,
      TEST_DOMAIN_ANOTHER_PAGE
    );
    try {
      let keyHexAnother = await getRandomKeyHexFromBrowser(
        tabAnother.linkedBrowser,
        TEST_DOMAIN_ANOTHER_PAGE,
        TEST_DOMAIN_THIRD_PAGE
      );
      isnot(
        keyHexAnother,
        keyHexOne,
        `The key should be different when loading a different domain`
      );
    } catch (e) {
      ok(
        false,
        "Shouldn't fail when getting the random key from the cookieJarSettings"
      );
    }

    BrowserTestUtils.removeTab(tabOne);
    BrowserTestUtils.removeTab(tabTwo);
    BrowserTestUtils.removeTab(tabAnother);
    if (testPrivateWin) {
      await BrowserTestUtils.closeWindow(win);
    }
  }
});

// Test the fingerprinting randomization key will change after private session
// ends.
add_task(async function test_reset_key_after_pbm_session_ends() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.resistFingerprinting", true]],
  });

  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // Open a tab in the private window.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    privateWin.gBrowser,
    TEST_PAGE
  );

  let keyHex = await getRandomKeyHexFromBrowser(
    tab.linkedBrowser,
    TEST_PAGE,
    TEST_DOMAIN_THIRD_PAGE
  );

  // Close the window and open another private window.
  BrowserTestUtils.removeTab(tab);

  let promisePBExit = TestUtils.topicObserved("last-pb-context-exited");
  await BrowserTestUtils.closeWindow(privateWin);
  await promisePBExit;

  privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // Open a tab again in the new private window.
  tab = await BrowserTestUtils.openNewForegroundTab(
    privateWin.gBrowser,
    TEST_PAGE
  );

  let keyHexNew = await getRandomKeyHexFromBrowser(
    tab.linkedBrowser,
    TEST_PAGE,
    TEST_DOMAIN_THIRD_PAGE
  );

  // Ensure the keys are different.
  isnot(keyHexNew, keyHex, "Ensure the new key is different from the old one.");

  BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.closeWindow(privateWin);
});

// Test accessing the fingerprinting randomization key will throw in normal
// windows if we exempt fingerprinting protection in normal windows.
add_task(async function test_randomization_with_exempted_normal_window() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting", false],
      ["privacy.resistFingerprinting.pbmode", true],
      ["privacy.fingerprintingProtection", false],
      ["privacy.fingerprintingProtection.pbmode", false],
    ],
  });

  // Ensure accessing the fingerprinting randomization key of the browser
  // element will throw if fingerprinting randomization is exempted from normal
  // windows.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);

  try {
    let key =
      tab.linkedBrowser.cookieJarSettings.fingerprintingRandomizationKey;
    ok(
      false,
      `Accessing the fingerprinting randomization key should throw when fingerprinting resistance is exempted in normal windows. ${key}`
    );
  } catch (e) {
    ok(
      true,
      "It should throw when getting the key when fingerprinting resistance is exempted in normal windows."
    );
  }

  // Ensure accessing the fingerprinting randomization key of the top-level
  // document will throw if fingerprinting randomization is exempted from normal
  // windows.
  try {
    await SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
      return content.document.cookieJarSettings.fingerprintingRandomizationKey;
    });
  } catch (e) {
    ok(
      true,
      "It should throw when getting the key when fingerprinting resistance is exempted in normal windows."
    );
  }

  BrowserTestUtils.removeTab(tab);

  // Open a private window and check the key can be accessed there.
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // Open a tab in the private window.
  tab = await BrowserTestUtils.openNewForegroundTab(
    privateWin.gBrowser,
    TEST_PAGE
  );

  // Access the key, this shouldn't throw an error.
  await getRandomKeyHexFromBrowser(
    tab.linkedBrowser,
    TEST_PAGE,
    TEST_DOMAIN_THIRD_PAGE
  );

  BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.closeWindow(privateWin);
});

// Test that the random key gets reset when the site data gets cleared.
add_task(async function test_reset_random_key_when_clear_site_data() {
  // Enable fingerprinting randomization key generation.
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.resistFingerprinting", true]],
  });

  // Open a tab and get randomization key from the test domain.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);

  let keyHex = await getRandomKeyHexFromBrowser(
    tab.linkedBrowser,
    TEST_PAGE,
    TEST_DOMAIN_THIRD_PAGE
  );

  // Open another tab and get randomization key from another domain.
  let anotherTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_DOMAIN_ANOTHER_PAGE
  );

  let keyHexAnother = await getRandomKeyHexFromBrowser(
    anotherTab.linkedBrowser,
    TEST_PAGE,
    TEST_DOMAIN_THIRD_PAGE
  );

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(anotherTab);

  // Call ForgetAboutSite for the test domain.
  await ForgetAboutSite.removeDataFromDomain("example.com");

  // Open the tab for the test domain again and verify the key is reset.
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
  let keyHexNew = await getRandomKeyHexFromBrowser(
    tab.linkedBrowser,
    TEST_PAGE,
    TEST_DOMAIN_THIRD_PAGE
  );

  // Open the tab for another domain again and verify the key is intact.
  anotherTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_DOMAIN_ANOTHER_PAGE
  );
  let keyHexAnotherNew = await getRandomKeyHexFromBrowser(
    anotherTab.linkedBrowser,
    TEST_PAGE,
    TEST_DOMAIN_THIRD_PAGE
  );

  // Ensure the keys are different for the test domain.
  isnot(keyHexNew, keyHex, "Ensure the new key is different from the old one.");

  // Ensure the key for another domain isn't changed.
  is(
    keyHexAnother,
    keyHexAnotherNew,
    "Ensure the key of another domain isn't reset."
  );

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(anotherTab);
});
