const TEST_URL_PATH = "/browser/toolkit/components/passwordmgr/test/browser/";
const INITIAL_URL = `about:blank`;
const FORM_URL = `https://example.org${TEST_URL_PATH}form_basic.html`;
const FORMLESS_URL = `https://example.org${TEST_URL_PATH}formless_basic.html`;
const testUrls = [FORM_URL, FORMLESS_URL];

async function getDocumentVisibilityState(browser) {
  let visibility = await ContentTask.spawn(browser, null, async function() {
    return content.document.visibilityState;
  });
  return visibility;
}

async function addContentObserver(browser, topic) {
  // add an observer.
  await ContentTask.spawn(browser, [topic], function(contentTopic) {
    this.gObserver = {
      wasObserved: false,
      observe: () => {
        content.wasObserved = true;
      },
    };
    Services.obs.addObserver(this.gObserver, contentTopic);
  });
}

async function getContentObserverResult(browser, topic) {
  let result = await ContentTask.spawn(browser, [topic], async function(contentTopic) {
    const {TestUtils} = ChromeUtils.import("resource://testing-common/TestUtils.jsm");
    try {
      await TestUtils.waitForCondition(() => {
        return content.wasObserved;
      }, `Wait for "passwordmgr-processed-form"`);
    } catch (ex) {
      content.wasObserved = false;
    }
    Services.obs.removeObserver(this.gObserver, "passwordmgr-processed-form");
    return content.wasObserved;
  });
  return result;
}

// Waits for the master password prompt and cancels it.
function observeMasterPasswordDialog(window, result) {
  let closedPromise;
  function topicObserver(subject) {
    if (subject.Dialog.args.title == "Password Required") {
      result.wasShown = true;
      subject.Dialog.ui.button1.click();
      closedPromise = BrowserTestUtils.waitForEvent(window, "DOMModalDialogClosed");
    }
  }
  Services.obs.addObserver(topicObserver, "common-dialog-loaded");

  let waited = TestUtils.waitForCondition(() => {
    return result.wasShown;
  }, "Wait for master password dialog");

  return Promise.all([waited, closedPromise]).catch(ex => {
    info(`observeMasterPasswordDialog, caught exception from topicObserved: ${ex}`);
  }).finally(() => {
    Services.obs.removeObserver(topicObserver, "common-dialog-loaded");
  });
}

add_task(async function setup() {
  Services.logins.removeAllLogins();
  let login = LoginTestUtils.testData.formLogin({
    hostname: "http://example.org",
    formSubmitURL: "http://example.org",
    username: "user1",
    password: "pass1",
  });
  Services.logins.addLogin(login);
});

add_task(async function test_processed_form_fired() {
  // Sanity check. If this doesnt work any results for the subsequent tasks are suspect
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, INITIAL_URL);
  let tab1Visibility = await getDocumentVisibilityState(tab1.linkedBrowser);
  is(tab1Visibility, "visible", "The first tab should be foreground");

  await addContentObserver(tab1.linkedBrowser, "passwordmgr-processed-form");
  await BrowserTestUtils.loadURI(tab1.linkedBrowser, FORM_URL);
  let result = await getContentObserverResult(tab1.linkedBrowser, "passwordmgr-processed-form");
  ok(result, "Observer should be notified when form is loaded into a visible document");
  gBrowser.removeTab(tab1);
});

testUrls.forEach(testUrl => {
  add_task(async function test_defer_autofill_until_visible() {
    let result, tab1Visibility;
    // open 2 tabs
    const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, INITIAL_URL);
    const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, INITIAL_URL);

    // confirm document is hidden
    tab1Visibility = await getDocumentVisibilityState(tab1.linkedBrowser);
    is(tab1Visibility, "hidden", "The first tab should be backgrounded");

    // we shouldn't even try to autofill while hidden, so look for the passwordmgr-processed-form
    // to be observed rather than any result of filling the form
    await addContentObserver(tab1.linkedBrowser, "passwordmgr-processed-form");
    await BrowserTestUtils.loadURI(tab1.linkedBrowser, testUrl);
    result = await getContentObserverResult(tab1.linkedBrowser, "passwordmgr-processed-form");
    ok(!result, "Observer should not be notified when form is loaded into a hidden document");

    // Add the observer before switching tab
    await addContentObserver(tab1.linkedBrowser, "passwordmgr-processed-form");
    await BrowserTestUtils.switchTab(gBrowser, tab1);
    result = await getContentObserverResult(tab1.linkedBrowser, "passwordmgr-processed-form");
    tab1Visibility = await getDocumentVisibilityState(tab1.linkedBrowser);
    is(tab1Visibility, "visible", "The first tab should be foreground");
    ok(result, "Observer should be notified when input's document becomes visible");

    // the form should have been autofilled with the login
    let fieldValues = await ContentTask.spawn(tab1.linkedBrowser, null, function() {
      let doc = content.document;
      return {
        username: doc.getElementById("form-basic-username").value,
        password: doc.getElementById("form-basic-password").value,
      };
    });
    is(fieldValues.username, "user1", "Checking filled username");
    is(fieldValues.password, "pass1", "Checking filled password");

    gBrowser.removeTab(tab1);
    gBrowser.removeTab(tab2);
  });
});

add_task(async function test_immediate_autofill_with_masterpassword() {
  // Set master password prompt timeout to 3s.
  // If this test goes intermittent, you likely have to increase this value.
  await SpecialPowers.pushPrefEnv({set: [["signon.masterPasswordReprompt.timeout_ms", 3000]]});

  LoginTestUtils.masterPassword.enable();
  await LoginTestUtils.reloadData();
  info(`Have enabled masterPassword, now isLoggedIn? ${Services.logins.isLoggedIn}`);

  registerCleanupFunction(async function() {
    LoginTestUtils.masterPassword.disable();
    await LoginTestUtils.reloadData();
  });

  let dialogResult, tab1Visibility, dialogObserved;

  // open 2 tabs
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, INITIAL_URL);
  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, INITIAL_URL);

  info("load a background login form tab with a matching saved login " +
       "and wait to see if the master password dialog is shown");
  is(await getDocumentVisibilityState(tab2.linkedBrowser),
     "visible", "The second tab should be visible");

  tab1Visibility = await getDocumentVisibilityState(tab1.linkedBrowser);
  is(tab1Visibility, "hidden", "The first tab should be backgrounded");

  dialogResult = { wasShown: false };
  dialogObserved = observeMasterPasswordDialog(tab1.ownerGlobal, dialogResult);

  // In this case we will try to autofill while hidden, so look for the passwordmgr-processed-form
  // to be observed
  await addContentObserver(tab1.linkedBrowser, "passwordmgr-processed-form");
  await BrowserTestUtils.loadURI(tab1.linkedBrowser, FORM_URL);
  let wasProcessed = getContentObserverResult(tab1.linkedBrowser, "passwordmgr-processed-form");
  await Promise.all([dialogObserved, wasProcessed]);

  ok(wasProcessed, "Observer should be notified when form is loaded into a hidden document");
  ok(dialogResult && dialogResult.wasShown, "MP Dialog should be shown when form is loaded into a hidden document");

  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);
});
