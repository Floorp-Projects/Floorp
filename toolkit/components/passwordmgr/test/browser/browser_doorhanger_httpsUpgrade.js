/*
 * Test capture popup notifications with HTTPS upgrades
 */

let nsLoginInfo = new Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  Ci.nsILoginInfo,
  "init"
);
let login1 = new nsLoginInfo(
  "http://example.com",
  "http://example.com",
  null,
  "notifyu1",
  "notifyp1",
  "user",
  "pass"
);
let login1HTTPS = new nsLoginInfo(
  "https://example.com",
  "https://example.com",
  null,
  "notifyu1",
  "notifyp1",
  "user",
  "pass"
);

add_task(async function test_httpsUpgradeCaptureFields_noChange() {
  info(
    "Check that we don't prompt to remember when capturing an upgraded login with no change"
  );
  await Services.logins.addLoginAsync(login1);
  // Sanity check the HTTP login exists.
  let logins = await Services.logins.getAllLogins();
  Assert.equal(logins.length, 1, "Should have the HTTP login");

  await testSubmittingLoginForm(
    "subtst_notifications_1.html",
    function (fieldValues) {
      Assert.equal(
        fieldValues.username,
        "notifyu1",
        "Checking submitted username"
      );
      Assert.equal(
        fieldValues.password,
        "notifyp1",
        "Checking submitted password"
      );
      let notif = getCaptureDoorhanger("password-save");
      Assert.ok(!notif, "checking for no notification popup");
    },
    "https://example.com"
  ); // This is HTTPS whereas the saved login is HTTP

  logins = await Services.logins.getAllLogins();
  Assert.equal(logins.length, 1, "Should only have 1 login still");
  let login = logins[0].QueryInterface(Ci.nsILoginMetaInfo);
  Assert.equal(
    login.origin,
    "http://example.com",
    "Check the origin is unchanged"
  );
  Assert.equal(login.username, "notifyu1", "Check the username is unchanged");
  Assert.equal(login.password, "notifyp1", "Check the password is unchanged");
  Assert.equal(login.timesUsed, 2, "Check times used increased");

  Services.logins.removeLogin(login1);
});

add_task(async function test_httpsUpgradeCaptureFields_changePW() {
  info(
    "Check that we prompt to change when capturing an upgraded login with a new PW"
  );
  await Services.logins.addLoginAsync(login1);
  // Sanity check the HTTP login exists.
  let logins = await Services.logins.getAllLogins();
  Assert.equal(logins.length, 1, "Should have the HTTP login");

  await testSubmittingLoginForm(
    "subtst_notifications_8.html",
    async function (fieldValues) {
      Assert.equal(
        fieldValues.username,
        "notifyu1",
        "Checking submitted username"
      );
      Assert.equal(
        fieldValues.password,
        "pass2",
        "Checking submitted password"
      );
      let notif = await getCaptureDoorhangerThatMayOpen("password-change");
      Assert.ok(notif, "checking for a change popup");

      await checkDoorhangerUsernamePassword("notifyu1", "pass2");
      clickDoorhangerButton(notif, CHANGE_BUTTON);

      Assert.ok(
        !getCaptureDoorhanger("password-change"),
        "popup should be gone"
      );
    },
    "https://example.com"
  ); // This is HTTPS whereas the saved login is HTTP

  await checkOnlyLoginWasUsedTwice({ justChanged: true });
  logins = await Services.logins.getAllLogins();
  Assert.equal(logins.length, 1, "Should only have 1 login still");
  let login = logins[0].QueryInterface(Ci.nsILoginMetaInfo);
  Assert.equal(
    login.origin,
    "https://example.com",
    "Check the origin is upgraded"
  );
  Assert.equal(
    login.formActionOrigin,
    "https://example.com",
    "Check the formActionOrigin is upgraded"
  );
  Assert.equal(login.username, "notifyu1", "Check the username is unchanged");
  Assert.equal(login.password, "pass2", "Check the password changed");
  Assert.equal(login.timesUsed, 2, "Check times used increased");

  Services.logins.removeAllUserFacingLogins();
});

add_task(
  async function test_httpsUpgradeCaptureFields_changePWWithBothSchemesSaved() {
    info(
      "Check that we prompt to change and properly save when capturing an upgraded login with a new PW when an http login also exists for that username"
    );
    await Services.logins.addLogins([login1, login1HTTPS]);

    let logins = await Services.logins.getAllLogins();
    Assert.equal(logins.length, 2, "Should have both HTTP and HTTPS logins");

    await testSubmittingLoginForm(
      "subtst_notifications_8.html",
      async function (fieldValues) {
        Assert.equal(
          fieldValues.username,
          "notifyu1",
          "Checking submitted username"
        );
        Assert.equal(
          fieldValues.password,
          "pass2",
          "Checking submitted password"
        );
        let notif = await getCaptureDoorhangerThatMayOpen("password-change");
        Assert.ok(notif, "checking for a change popup");

        await checkDoorhangerUsernamePassword("notifyu1", "pass2");
        clickDoorhangerButton(notif, CHANGE_BUTTON);

        Assert.ok(
          !getCaptureDoorhanger("password-change"),
          "popup should be gone"
        );
      },
      "https://example.com"
    );

    logins = await Services.logins.getAllLogins();
    Assert.equal(logins.length, 2, "Should have 2 logins still");
    let loginHTTP = logins[0].QueryInterface(Ci.nsILoginMetaInfo);
    let loginHTTPS = logins[1].QueryInterface(Ci.nsILoginMetaInfo);
    Assert.ok(
      LoginHelper.doLoginsMatch(login1, loginHTTP, { ignorePassword: true }),
      "Check HTTP login is equal"
    );
    Assert.equal(loginHTTP.timesUsed, 1, "Check times used stayed the same");
    Assert.equal(
      loginHTTP.timeCreated,
      loginHTTP.timePasswordChanged,
      "login.timeCreated == login.timePasswordChanged"
    );
    Assert.equal(
      loginHTTP.timeLastUsed,
      loginHTTP.timePasswordChanged,
      "timeLastUsed == timePasswordChanged"
    );

    Assert.ok(
      LoginHelper.doLoginsMatch(login1HTTPS, loginHTTPS, {
        ignorePassword: true,
      }),
      "Check HTTPS login is equal"
    );
    Assert.equal(
      loginHTTPS.username,
      "notifyu1",
      "Check the username is unchanged"
    );
    Assert.equal(loginHTTPS.password, "pass2", "Check the password changed");
    Assert.equal(loginHTTPS.timesUsed, 2, "Check times used increased");
    Assert.ok(
      loginHTTPS.timeCreated < loginHTTPS.timePasswordChanged,
      "login.timeCreated < login.timePasswordChanged"
    );
    Assert.equal(
      loginHTTPS.timeLastUsed,
      loginHTTPS.timePasswordChanged,
      "timeLastUsed == timePasswordChanged"
    );

    Services.logins.removeAllUserFacingLogins();
  }
);

add_task(async function test_httpsUpgradeCaptureFields_captureMatchingHTTP() {
  info("Capture a new HTTP login which matches a stored HTTPS one.");
  await Services.logins.addLoginAsync(login1HTTPS);

  const storageChangedPromise = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "addLogin"
  );

  await testSubmittingLoginFormHTTP(
    "subtst_notifications_1.html",
    async function (fieldValues) {
      Assert.equal(
        fieldValues.username,
        "notifyu1",
        "Checking submitted username"
      );
      Assert.equal(
        fieldValues.password,
        "notifyp1",
        "Checking submitted password"
      );
      let notif = await getCaptureDoorhangerThatMayOpen("password-save");
      Assert.ok(notif, "got notification popup");

      Assert.equal(
        (await Services.logins.getAllLogins()).length,
        1,
        "Should only have the HTTPS login"
      );

      await checkDoorhangerUsernamePassword("notifyu1", "notifyp1");
      clickDoorhangerButton(notif, REMEMBER_BUTTON);
    }
  );

  await storageChangedPromise;

  let logins = await Services.logins.getAllLogins();
  Assert.equal(logins.length, 2, "Should have both HTTP and HTTPS logins");
  for (let login of logins) {
    login = login.QueryInterface(Ci.nsILoginMetaInfo);
    Assert.equal(
      login.username,
      "notifyu1",
      "Check the username used on the new entry"
    );
    Assert.equal(
      login.password,
      "notifyp1",
      "Check the password used on the new entry"
    );
    Assert.equal(login.timesUsed, 1, "Check times used on entry");
  }

  info(
    "Make sure Remember took effect and we don't prompt for an existing HTTP login"
  );
  await testSubmittingLoginFormHTTP(
    "subtst_notifications_1.html",
    function (fieldValues) {
      Assert.equal(
        fieldValues.username,
        "notifyu1",
        "Checking submitted username"
      );
      Assert.equal(
        fieldValues.password,
        "notifyp1",
        "Checking submitted password"
      );
      let notif = getCaptureDoorhanger("password-save");
      Assert.ok(!notif, "checking for no notification popup");
    }
  );

  logins = await Services.logins.getAllLogins();
  Assert.equal(logins.length, 2, "Should have both HTTP and HTTPS still");

  let httpsLogins = LoginHelper.searchLoginsWithObject({
    origin: "https://example.com",
  });
  Assert.equal(httpsLogins.length, 1, "Check https logins count");
  let httpsLogin = httpsLogins[0].QueryInterface(Ci.nsILoginMetaInfo);
  Assert.ok(httpsLogin.equals(login1HTTPS), "Check HTTPS login didn't change");
  Assert.equal(httpsLogin.timesUsed, 1, "Check times used");

  let httpLogins = LoginHelper.searchLoginsWithObject({
    origin: "http://example.com",
  });
  Assert.equal(httpLogins.length, 1, "Check http logins count");
  let httpLogin = httpLogins[0].QueryInterface(Ci.nsILoginMetaInfo);
  Assert.ok(httpLogin.equals(login1), "Check HTTP login is as expected");
  Assert.equal(httpLogin.timesUsed, 2, "Check times used increased");

  Services.logins.removeLogin(login1);
  Services.logins.removeLogin(login1HTTPS);
});
