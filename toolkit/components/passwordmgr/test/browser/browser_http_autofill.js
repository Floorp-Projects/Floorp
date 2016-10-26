const TEST_URL_PATH = "://example.org/browser/toolkit/components/passwordmgr/test/browser/";

add_task(function* setup() {
  let login = LoginTestUtils.testData.formLogin({
    hostname: "http://example.org",
    formSubmitURL: "http://example.org",
    username: "username",
    password: "password",
  });
  Services.logins.addLogin(login);
  login = LoginTestUtils.testData.formLogin({
    hostname: "http://example.org",
    formSubmitURL: "http://another.domain",
    username: "username",
    password: "password",
  });
  Services.logins.addLogin(login);
  yield SpecialPowers.pushPrefEnv({ "set": [["signon.autofillForms.http", false]] });
});

add_task(function* test_http_autofill() {
  for (let scheme of ["http", "https"]) {
    let tab = yield BrowserTestUtils
      .openNewForegroundTab(gBrowser, `${scheme}${TEST_URL_PATH}form_basic.html`);

    let {username, password} = yield ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
      let doc = content.document;
      let username = doc.getElementById("form-basic-username").value;
      let password = doc.getElementById("form-basic-password").value;
      return { username, password };
    });

    is(username, scheme == "http" ? "" : "username", "Username filled correctly");
    is(password, scheme == "http" ? "" : "password", "Password filled correctly");

    gBrowser.removeTab(tab);
  }
});

add_task(function* test_iframe_in_http_autofill() {
  for (let scheme of ["http", "https"]) {
    let tab = yield BrowserTestUtils
      .openNewForegroundTab(gBrowser, `${scheme}${TEST_URL_PATH}form_basic_iframe.html`);

    let {username, password} = yield ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
      let doc = content.document;
      let iframe = doc.getElementById("test-iframe");
      let username = iframe.contentWindow.document.getElementById("form-basic-username").value;
      let password = iframe.contentWindow.document.getElementById("form-basic-password").value;
      return { username, password };
    });

    is(username, scheme == "http" ? "" : "username", "Username filled correctly");
    is(password, scheme == "http" ? "" : "password", "Password filled correctly");

    gBrowser.removeTab(tab);
  }
});

add_task(function* test_http_action_autofill() {
  for (let type of ["insecure", "secure"]) {
    let tab = yield BrowserTestUtils
      .openNewForegroundTab(gBrowser, `https${TEST_URL_PATH}form_cross_origin_${type}_action.html`);

    let {username, password} = yield ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
      let doc = content.document;
      let username = doc.getElementById("form-basic-username").value;
      let password = doc.getElementById("form-basic-password").value;
      return { username, password };
    });

    is(username, type == "insecure" ? "" : "username", "Username filled correctly");
    is(password, type == "insecure" ? "" : "password", "Password filled correctly");

    gBrowser.removeTab(tab);
  }
});

