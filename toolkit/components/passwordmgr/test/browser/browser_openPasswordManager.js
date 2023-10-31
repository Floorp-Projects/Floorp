const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

add_task(async function test_noFilter() {
  let openingFunc = () =>
    LoginHelper.openPasswordManager(window, { entryPoint: "mainmenu" });
  let passwordManager = await openPasswordManager(openingFunc);

  Assert.ok(passwordManager, "Login dialog was opened");
  await passwordManager.close();
  await TestUtils.waitForCondition(() => {
    return Services.wm.getMostRecentWindow("Toolkit:PasswordManager") === null;
  }, "Waiting for the password manager dialog to close");
});

add_task(async function test_filter() {
  // Greek IDN for example.test
  let domain = "παράδειγμα.δοκιμή";
  let openingFunc = () =>
    LoginHelper.openPasswordManager(window, {
      filterString: domain,
      entryPoint: "mainmenu",
    });
  let passwordManager = await openPasswordManager(openingFunc, true);
  Assert.equal(
    passwordManager.filterValue,
    domain,
    "search string to filter logins should match expectation"
  );
  await passwordManager.close();
  await TestUtils.waitForCondition(() => {
    return Services.wm.getMostRecentWindow("Toolkit:PasswordManager") === null;
  }, "Waiting for the password manager dialog to close");
});

add_task(async function test_management_noFilter() {
  let tabOpenPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:logins");
  LoginHelper.openPasswordManager(window, { entryPoint: "mainmenu" });
  let tab = await tabOpenPromise;
  Assert.ok(tab, "Got the new tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_management_filter() {
  let tabOpenPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:logins?filter=%CF%80%CE%B1%CF%81%CE%AC%CE%B4%CE%B5%CE%B9%CE%B3%CE%BC%CE%B1.%CE%B4%CE%BF%CE%BA%CE%B9%CE%BC%CE%AE"
  );
  // Greek IDN for example.test
  LoginHelper.openPasswordManager(window, {
    filterString: "παράδειγμα.δοκιμή",
    entryPoint: "mainmenu",
  });
  let tab = await tabOpenPromise;
  Assert.ok(tab, "Got the new tab with a domain filter");
  BrowserTestUtils.removeTab(tab);
});

add_task(
  async function test_url_when_opening_password_manager_without_a_filterString() {
    sinon.spy(window.gBrowser, "addTrustedTab");
    const openingFunc = () =>
      LoginHelper.openPasswordManager(window, {
        filterString: "",
        entryPoint: "mainmenu",
      });
    const passwordManager = await openPasswordManager(openingFunc);

    const url = window.gBrowser.addTrustedTab.lastCall.args[0];

    Assert.ok(
      !url.includes("filter"),
      "LoginHelper.openPasswordManager call without a filterString navigated to a URL with a filter query param"
    );
    Assert.equal(
      0,
      url.split("").filter(char => char === "&").length,
      "LoginHelper.openPasswordManager call without a filterString navigated to a URL with an &"
    );
    Assert.equal(
      url,
      "about:logins?entryPoint=mainmenu",
      "LoginHelper.openPasswordManager call without a filterString navigated to an unexpected URL"
    );

    Assert.ok(passwordManager, "Login dialog was opened");
    await passwordManager.close();
    window.gBrowser.addTrustedTab.restore();
  }
);

add_task(
  async function test_url_when_opening_password_manager_with_a_filterString() {
    sinon.spy(window.gBrowser, "addTrustedTab");
    const openingFunc = () =>
      LoginHelper.openPasswordManager(window, {
        filterString: "testFilter",
        entryPoint: "mainmenu",
      });
    const passwordManager = await openPasswordManager(openingFunc);

    const url = window.gBrowser.addTrustedTab.lastCall.args[0];

    Assert.ok(
      url.includes("filter"),
      "LoginHelper.openPasswordManager call with a filterString navigated to a URL without a filter query param"
    );
    Assert.equal(
      1,
      url.split("").filter(char => char === "&").length,
      "LoginHelper.openPasswordManager call with a filterString navigated to a URL without the correct number of '&'s"
    );
    Assert.equal(
      url,
      "about:logins?filter=testFilter&entryPoint=mainmenu",
      "LoginHelper.openPasswordManager call with a filterString navigated to an unexpected URL"
    );

    Assert.ok(passwordManager, "Login dialog was opened");
    await passwordManager.close();
    window.gBrowser.addTrustedTab.restore();
  }
);

add_task(
  async function test_url_when_opening_password_manager_without_filterString_or_entryPoint() {
    sinon.spy(window.gBrowser, "addTrustedTab");
    const openingFunc = () =>
      LoginHelper.openPasswordManager(window, {
        filterString: "",
        entryPoint: "",
      });
    const passwordManager = await openPasswordManager(openingFunc);

    const url = window.gBrowser.addTrustedTab.lastCall.args[0];

    Assert.ok(
      !url.includes("filter"),
      "LoginHelper.openPasswordManager call without a filterString navigated to a URL with a filter query param"
    );
    Assert.ok(
      !url.includes("entryPoint"),
      "LoginHelper.openPasswordManager call without an entryPoint navigated to a URL with an entryPoint query param"
    );
    Assert.equal(
      0,
      url.split("").filter(char => char === "&").length,
      "LoginHelper.openPasswordManager call without query params navigated to a URL that included at least one '&'"
    );
    Assert.equal(
      url,
      "about:logins",
      "LoginHelper.openPasswordManager call without a filterString or entryPoint navigated to an unexpected URL"
    );

    Assert.ok(passwordManager, "Login dialog was opened");
    await passwordManager.close();
    window.gBrowser.addTrustedTab.restore();
  }
);
