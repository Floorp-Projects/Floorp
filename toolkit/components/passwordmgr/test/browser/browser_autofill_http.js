const TEST_URL_PATH =
  "://example.org/browser/toolkit/components/passwordmgr/test/browser/";

add_task(async function setup() {
  let login = LoginTestUtils.testData.formLogin({
    origin: "http://example.org",
    formActionOrigin: "http://example.org",
    username: "username",
    password: "password",
  });
  Services.logins.addLogin(login);
  login = LoginTestUtils.testData.formLogin({
    origin: "http://example.org",
    formActionOrigin: "http://example.com",
    username: "username",
    password: "password",
  });
  Services.logins.addLogin(login);
  await SpecialPowers.pushPrefEnv({
    set: [["signon.autofillForms.http", false]],
  });
});

add_task(async function test_http_autofill() {
  for (let scheme of ["http", "https"]) {
    let formFilled = listenForTestNotification("FormProcessed");

    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      `${scheme}${TEST_URL_PATH}form_basic.html`
    );

    await formFilled;

    let [username, password] = await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [],
      async function() {
        let doc = content.document;
        let contentUsername = doc.getElementById("form-basic-username").value;
        let contentPassword = doc.getElementById("form-basic-password").value;
        return [contentUsername, contentPassword];
      }
    );

    is(
      username,
      scheme == "http" ? "" : "username",
      "Username filled correctly"
    );
    is(
      password,
      scheme == "http" ? "" : "password",
      "Password filled correctly"
    );

    gBrowser.removeTab(tab);
  }
});

add_task(async function test_iframe_in_http_autofill() {
  for (let scheme of ["http", "https"]) {
    // Wait for parent and child iframe to be processed.
    let formFilled = listenForTestNotification("FormProcessed", 2);

    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      `${scheme}${TEST_URL_PATH}form_basic_iframe.html`
    );

    await formFilled;

    let [username, password] = await SpecialPowers.spawn(
      gBrowser.selectedBrowser.browsingContext.children[0],
      [],
      async function() {
        let doc = this.content.document;
        return [
          doc.getElementById("form-basic-username").value,
          doc.getElementById("form-basic-password").value,
        ];
      }
    );

    is(
      username,
      scheme == "http" ? "" : "username",
      "Username filled correctly"
    );
    is(
      password,
      scheme == "http" ? "" : "password",
      "Password filled correctly"
    );

    gBrowser.removeTab(tab);
  }
});

add_task(async function test_http_action_autofill() {
  for (let type of ["insecure", "secure"]) {
    let formFilled = listenForTestNotification("FormProcessed");

    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      `https${TEST_URL_PATH}form_cross_origin_${type}_action.html`
    );

    await formFilled;

    let [username, password] = await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [],
      async function() {
        let doc = this.content.document;
        return [
          doc.getElementById("form-basic-username").value,
          doc.getElementById("form-basic-password").value,
        ];
      }
    );

    is(
      username,
      type == "insecure" ? "" : "username",
      "Username filled correctly"
    );
    is(
      password,
      type == "insecure" ? "" : "password",
      "Password filled correctly"
    );

    gBrowser.removeTab(tab);
  }
});
