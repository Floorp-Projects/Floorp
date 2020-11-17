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
  Services.logins.addLogin(login1);
  // Sanity check the HTTP login exists.
  let logins = Services.logins.getAllLogins();
  is(logins.length, 1, "Should have the HTTP login");

  await testSubmittingLoginForm(
    "subtst_notifications_1.html",
    function(fieldValues) {
      is(fieldValues.username, "notifyu1", "Checking submitted username");
      is(fieldValues.password, "notifyp1", "Checking submitted password");
      let notif = getCaptureDoorhanger("password-save");
      ok(!notif, "checking for no notification popup");
    },
    "https://example.com"
  ); // This is HTTPS whereas the saved login is HTTP

  logins = Services.logins.getAllLogins();
  is(logins.length, 1, "Should only have 1 login still");
  let login = logins[0].QueryInterface(Ci.nsILoginMetaInfo);
  is(login.origin, "http://example.com", "Check the origin is unchanged");
  is(login.username, "notifyu1", "Check the username is unchanged");
  is(login.password, "notifyp1", "Check the password is unchanged");
  is(login.timesUsed, 2, "Check times used increased");

  Services.logins.removeLogin(login1);
});

add_task(async function test_httpsUpgradeCaptureFields_changePW() {
  info(
    "Check that we prompt to change when capturing an upgraded login with a new PW"
  );
  Services.logins.addLogin(login1);
  // Sanity check the HTTP login exists.
  let logins = Services.logins.getAllLogins();
  is(logins.length, 1, "Should have the HTTP login");

  await testSubmittingLoginForm(
    "subtst_notifications_8.html",
    async function(fieldValues) {
      is(fieldValues.username, "notifyu1", "Checking submitted username");
      is(fieldValues.password, "pass2", "Checking submitted password");
      let notif = await getCaptureDoorhangerThatMayOpen("password-change");
      ok(notif, "checking for a change popup");

      await checkDoorhangerUsernamePassword("notifyu1", "pass2");
      clickDoorhangerButton(notif, CHANGE_BUTTON);

      ok(!getCaptureDoorhanger("password-change"), "popup should be gone");
    },
    "https://example.com"
  ); // This is HTTPS whereas the saved login is HTTP

  checkOnlyLoginWasUsedTwice({ justChanged: true });
  logins = Services.logins.getAllLogins();
  is(logins.length, 1, "Should only have 1 login still");
  let login = logins[0].QueryInterface(Ci.nsILoginMetaInfo);
  is(login.origin, "https://example.com", "Check the origin is upgraded");
  is(
    login.formActionOrigin,
    "https://example.com",
    "Check the formActionOrigin is upgraded"
  );
  is(login.username, "notifyu1", "Check the username is unchanged");
  is(login.password, "pass2", "Check the password changed");
  is(login.timesUsed, 2, "Check times used increased");

  Services.logins.removeAllLogins();
});

add_task(
  async function test_httpsUpgradeCaptureFields_changePWWithBothSchemesSaved() {
    info(
      "Check that we prompt to change and properly save when capturing an upgraded login with a new PW when an http login also exists for that username"
    );
    Services.logins.addLogin(login1);
    Services.logins.addLogin(login1HTTPS);

    let logins = Services.logins.getAllLogins();
    is(logins.length, 2, "Should have both HTTP and HTTPS logins");

    await testSubmittingLoginForm(
      "subtst_notifications_8.html",
      async function(fieldValues) {
        is(fieldValues.username, "notifyu1", "Checking submitted username");
        is(fieldValues.password, "pass2", "Checking submitted password");
        let notif = await getCaptureDoorhangerThatMayOpen("password-change");
        ok(notif, "checking for a change popup");

        await checkDoorhangerUsernamePassword("notifyu1", "pass2");
        clickDoorhangerButton(notif, CHANGE_BUTTON);

        ok(!getCaptureDoorhanger("password-change"), "popup should be gone");
      },
      "https://example.com"
    );

    logins = Services.logins.getAllLogins();
    is(logins.length, 2, "Should have 2 logins still");
    let loginHTTP = logins[0].QueryInterface(Ci.nsILoginMetaInfo);
    let loginHTTPS = logins[1].QueryInterface(Ci.nsILoginMetaInfo);
    ok(
      LoginHelper.doLoginsMatch(login1, loginHTTP, { ignorePassword: true }),
      "Check HTTP login is equal"
    );
    is(loginHTTP.timesUsed, 1, "Check times used stayed the same");
    is(
      loginHTTP.timeCreated,
      loginHTTP.timePasswordChanged,
      "login.timeCreated == login.timePasswordChanged"
    );
    is(
      loginHTTP.timeLastUsed,
      loginHTTP.timePasswordChanged,
      "timeLastUsed == timePasswordChanged"
    );

    ok(
      LoginHelper.doLoginsMatch(login1HTTPS, loginHTTPS, {
        ignorePassword: true,
      }),
      "Check HTTPS login is equal"
    );
    is(loginHTTPS.username, "notifyu1", "Check the username is unchanged");
    is(loginHTTPS.password, "pass2", "Check the password changed");
    is(loginHTTPS.timesUsed, 2, "Check times used increased");
    ok(
      loginHTTPS.timeCreated < loginHTTPS.timePasswordChanged,
      "login.timeCreated < login.timePasswordChanged"
    );
    is(
      loginHTTPS.timeLastUsed,
      loginHTTPS.timePasswordChanged,
      "timeLastUsed == timePasswordChanged"
    );

    Services.logins.removeAllLogins();
  }
);

add_task(async function test_httpsUpgradeCaptureFields_captureMatchingHTTP() {
  info("Capture a new HTTP login which matches a stored HTTPS one.");
  Services.logins.addLogin(login1HTTPS);

  await testSubmittingLoginForm("subtst_notifications_1.html", async function(
    fieldValues
  ) {
    is(fieldValues.username, "notifyu1", "Checking submitted username");
    is(fieldValues.password, "notifyp1", "Checking submitted password");
    let notif = await getCaptureDoorhangerThatMayOpen("password-save");
    ok(notif, "got notification popup");

    is(
      Services.logins.getAllLogins().length,
      1,
      "Should only have the HTTPS login"
    );

    await checkDoorhangerUsernamePassword("notifyu1", "notifyp1");
    clickDoorhangerButton(notif, REMEMBER_BUTTON);
  });

  let logins = Services.logins.getAllLogins();
  is(logins.length, 2, "Should have both HTTP and HTTPS logins");
  for (let login of logins) {
    login = login.QueryInterface(Ci.nsILoginMetaInfo);
    is(login.username, "notifyu1", "Check the username used on the new entry");
    is(login.password, "notifyp1", "Check the password used on the new entry");
    is(login.timesUsed, 1, "Check times used on entry");
  }

  info(
    "Make sure Remember took effect and we don't prompt for an existing HTTP login"
  );
  await testSubmittingLoginForm("subtst_notifications_1.html", function(
    fieldValues
  ) {
    is(fieldValues.username, "notifyu1", "Checking submitted username");
    is(fieldValues.password, "notifyp1", "Checking submitted password");
    let notif = getCaptureDoorhanger("password-save");
    ok(!notif, "checking for no notification popup");
  });

  logins = Services.logins.getAllLogins();
  is(logins.length, 2, "Should have both HTTP and HTTPS still");

  let httpsLogins = LoginHelper.searchLoginsWithObject({
    origin: "https://example.com",
  });
  is(httpsLogins.length, 1, "Check https logins count");
  let httpsLogin = httpsLogins[0].QueryInterface(Ci.nsILoginMetaInfo);
  ok(httpsLogin.equals(login1HTTPS), "Check HTTPS login didn't change");
  is(httpsLogin.timesUsed, 1, "Check times used");

  let httpLogins = LoginHelper.searchLoginsWithObject({
    origin: "http://example.com",
  });
  is(httpLogins.length, 1, "Check http logins count");
  let httpLogin = httpLogins[0].QueryInterface(Ci.nsILoginMetaInfo);
  ok(httpLogin.equals(login1), "Check HTTP login is as expected");
  is(httpLogin.timesUsed, 2, "Check times used increased");

  Services.logins.removeLogin(login1);
  Services.logins.removeLogin(login1HTTPS);
});
