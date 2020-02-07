const TEST_URL_PATH = "/browser/toolkit/components/passwordmgr/test/browser/";
const INITIAL_URL = `about:blank`;
const FORM_URL = `https://example.org${TEST_URL_PATH}form_basic.html`;
const FORMLESS_URL = `https://example.org${TEST_URL_PATH}formless_basic.html`;
const testUrls = [FORM_URL, FORMLESS_URL];
const BRAND_BUNDLE = Services.strings.createBundle(
  "chrome://branding/locale/brand.properties"
);
const BRAND_FULL_NAME = BRAND_BUNDLE.GetStringFromName("brandFullName");

async function getDocumentVisibilityState(browser) {
  let visibility = await SpecialPowers.spawn(browser, [], async function() {
    return content.document.visibilityState;
  });
  return visibility;
}

// Waits for the master password prompt and cancels it.
function observeMasterPasswordDialog(window, result) {
  let closedPromise;
  function topicObserver(subject) {
    let expected = "Password Required - " + BRAND_FULL_NAME;
    if (subject.Dialog.args.title == expected) {
      result.wasShown = true;
      subject.Dialog.ui.button1.click();
      closedPromise = BrowserTestUtils.waitForEvent(
        window,
        "DOMModalDialogClosed"
      );
    }
  }
  Services.obs.addObserver(topicObserver, "common-dialog-loaded");

  let waited = TestUtils.waitForCondition(() => {
    return result.wasShown;
  }, "Wait for master password dialog");

  return Promise.all([waited, closedPromise])
    .catch(ex => {
      info(
        `observeMasterPasswordDialog, caught exception from topicObserved: ${ex}`
      );
    })
    .finally(() => {
      Services.obs.removeObserver(topicObserver, "common-dialog-loaded");
    });
}

add_task(async function setup() {
  Services.logins.removeAllLogins();
  let login = LoginTestUtils.testData.formLogin({
    origin: "http://example.org",
    formActionOrigin: "http://example.org",
    username: "user1",
    password: "pass1",
  });
  Services.logins.addLogin(login);
});

add_task(async function test_processed_form_fired() {
  // Sanity check. If this doesnt work any results for the subsequent tasks are suspect
  const tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    INITIAL_URL
  );
  let tab1Visibility = await getDocumentVisibilityState(tab1.linkedBrowser);
  is(tab1Visibility, "visible", "The first tab should be foreground");

  let formProcessedPromise = listenForTestNotification("FormProcessed");
  await BrowserTestUtils.loadURI(tab1.linkedBrowser, FORM_URL);
  await formProcessedPromise;
  gBrowser.removeTab(tab1);
});

testUrls.forEach(testUrl => {
  add_task(async function test_defer_autofill_until_visible() {
    let result, tab1Visibility;
    // open 2 tabs
    const tab1 = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      INITIAL_URL
    );
    const tab2 = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      INITIAL_URL
    );

    // confirm document is hidden
    tab1Visibility = await getDocumentVisibilityState(tab1.linkedBrowser);
    is(tab1Visibility, "hidden", "The first tab should be backgrounded");

    // we shouldn't even try to autofill while hidden, so wait for the document to be in the
    // non-visible pending queue instead.
    let formFilled = false;
    listenForTestNotification("FormProcessed").then(() => {
      formFilled = true;
    });
    await BrowserTestUtils.loadURI(tab1.linkedBrowser, testUrl);

    await TestUtils.waitForCondition(() => {
      let windowGlobal = tab1.linkedBrowser.browsingContext.currentWindowGlobal;
      if (!windowGlobal || windowGlobal.documentURI.spec == "about:blank") {
        return false;
      }

      let actor = windowGlobal.getActor("LoginManager");
      return actor.sendQuery("PasswordManager:formIsPending");
    });

    ok(
      !formFilled,
      "Observer should not be notified when form is loaded into a hidden document"
    );

    // Add the observer before switching tab
    let formProcessedPromise = listenForTestNotification("FormProcessed");
    await BrowserTestUtils.switchTab(gBrowser, tab1);
    result = await formProcessedPromise;
    tab1Visibility = await getDocumentVisibilityState(tab1.linkedBrowser);
    is(tab1Visibility, "visible", "The first tab should be foreground");
    ok(
      result,
      "Observer should be notified when input's document becomes visible"
    );

    // the form should have been autofilled with the login
    let fieldValues = await SpecialPowers.spawn(
      tab1.linkedBrowser,
      [],
      function() {
        let doc = content.document;
        return {
          username: doc.getElementById("form-basic-username").value,
          password: doc.getElementById("form-basic-password").value,
        };
      }
    );
    is(fieldValues.username, "user1", "Checking filled username");
    is(fieldValues.password, "pass1", "Checking filled password");

    gBrowser.removeTab(tab1);
    gBrowser.removeTab(tab2);
  });
});

add_task(async function test_immediate_autofill_with_masterpassword() {
  // Set master password prompt timeout to 3s.
  // If this test goes intermittent, you likely have to increase this value.
  await SpecialPowers.pushPrefEnv({
    set: [["signon.masterPasswordReprompt.timeout_ms", 3000]],
  });

  LoginTestUtils.masterPassword.enable();
  await LoginTestUtils.reloadData();
  info(
    `Have enabled masterPassword, now isLoggedIn? ${Services.logins.isLoggedIn}`
  );

  registerCleanupFunction(async function() {
    LoginTestUtils.masterPassword.disable();
    await LoginTestUtils.reloadData();
  });

  let dialogResult, tab1Visibility, dialogObserved;

  // open 2 tabs
  const tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    INITIAL_URL
  );
  const tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    INITIAL_URL
  );

  info(
    "load a background login form tab with a matching saved login " +
      "and wait to see if the master password dialog is shown"
  );
  is(
    await getDocumentVisibilityState(tab2.linkedBrowser),
    "visible",
    "The second tab should be visible"
  );

  tab1Visibility = await getDocumentVisibilityState(tab1.linkedBrowser);
  is(tab1Visibility, "hidden", "The first tab should be backgrounded");

  dialogResult = { wasShown: false };
  dialogObserved = observeMasterPasswordDialog(tab1.ownerGlobal, dialogResult);

  // In this case we will try to autofill while hidden, so look for the passwordmgr-processed-form
  // to be observed
  let formProcessedPromise = listenForTestNotification("FormProcessed");
  await BrowserTestUtils.loadURI(tab1.linkedBrowser, FORM_URL);
  await Promise.all([formProcessedPromise, dialogObserved]);

  let wasProcessed = await formProcessedPromise;
  ok(
    wasProcessed,
    "Observer should be notified when form is loaded into a hidden document"
  );
  ok(
    dialogResult && dialogResult.wasShown,
    "MP Dialog should be shown when form is loaded into a hidden document"
  );

  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);
});
