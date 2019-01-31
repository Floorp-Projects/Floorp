"use strict";

async function submitForm(browser, formAction, selectorValues) {
  function contentSubmitForm([contentFormAction, contentSelectorValues]) {
    let doc = content.document;
    let form = doc.getElementById("form");
    form.action = contentFormAction;
    for (let [sel, value] of Object.entries(contentSelectorValues)) {
      try {
        doc.querySelector(sel).value = value;
      } catch (ex) {
        throw new Error(`submitForm: Couldn't set value of field at: ${sel}`);
      }
    }
    form.submit();
  }
  await ContentTask.spawn(browser, [formAction, selectorValues], contentSubmitForm);
  let result = await getFormSubmissionResult(browser, formAction);
  return result;
}

async function getFormSubmissionResult(browser, resultUrl) {
  let fieldValues = await ContentTask.spawn(browser, [resultUrl], async function(contentResultUrl) {
    await ContentTaskUtils.waitForCondition(() => {
      return content.location.pathname.endsWith(contentResultUrl) &&
        content.document.readyState == "complete";
    }, `Wait for form submission load (${contentResultUrl})`);
    let username = content.document.getElementById("user").textContent;
    let password = content.document.getElementById("pass").textContent;
    return {
      username,
      password,
    };
  });
  return fieldValues;
}

let nsLoginInfo = new Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                                             Ci.nsILoginInfo, "init");
let login = new nsLoginInfo("https://example.com", "https://example.com", null,
                            "notifyu1", "notifyp1", "user", "pass");
let form1Url = `https://example.com/${DIRECTORY_PATH}subtst_privbrowsing_1.html`;
let form2Url = `https://example.com/${DIRECTORY_PATH}subtst_privbrowsing_2.html`;

let normalWin;
let privateWin;

// XXX: Note that tasks are currently run in sequence. Some tests may assume the state
// resulting from successful or unsuccessful logins in previous tasks

add_task(async function test_setup() {
  normalWin = await BrowserTestUtils.openNewBrowserWindow({private: false});
  privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  Services.logins.removeAllLogins();
});

add_task(async function test_normal_popup_notification_1() {
  info("test 1: run outside of private mode, popup notification should appear");
  await BrowserTestUtils.withNewTab({
    gBrowser: normalWin.gBrowser,
    url: form1Url,
  }, async function(browser) {
    let fieldValues = await submitForm(browser, "formsubmit.sjs", {
      "#user": "notifyu1",
      "#pass": "notifyp1",
    });
    is(fieldValues.username, "notifyu1", "Checking submitted username");
    is(fieldValues.password, "notifyp1", "Checking submitted password");

    let notif = getCaptureDoorhanger("password-save", PopupNotifications, browser);
    ok(notif, "got notification popup");
    if (notif) {
      notif.remove();
    }
  });
});

add_task(async function test_private_popup_notification_2() {
  info("test 2: run inside of private mode, popup notification should not appear");
  await BrowserTestUtils.withNewTab({
    gBrowser: privateWin.gBrowser,
    url: form1Url,
  }, async function(browser) {
    let fieldValues = await submitForm(browser, "formsubmit.sjs", {
      "#user": "notifyu1",
      "#pass": "notifyp1",
    });
    is(fieldValues.username, "notifyu1", "Checking submitted username");
    is(fieldValues.password, "notifyp1", "Checking submitted password");

    let notif = getCaptureDoorhanger("password-save", PopupNotifications, browser);
    ok(!notif, "Expected no notification popup");
    if (notif) {
      notif.remove();
    }
  });
});

add_task(async function test_normal_popup_notification_3() {
  info("test 3: run outside of private mode, popup notification should appear");
  await BrowserTestUtils.withNewTab({
    gBrowser: normalWin.gBrowser,
    url: form1Url,
  }, async function(browser) {
    let fieldValues = await submitForm(browser, "formsubmit.sjs", {
      "#user": "notifyu1",
      "#pass": "notifyp1",
    });
    is(fieldValues.username, "notifyu1", "Checking submitted username");
    is(fieldValues.password, "notifyp1", "Checking submitted password");

    let notif = getCaptureDoorhanger("password-save", PopupNotifications, browser);
    ok(notif, "got notification popup");
    if (notif) {
      notif.remove();
    }
  });
});

add_task(async function test_normal_new_password_4() {
  info("test 4: run with a login, outside of private mode," +
       " add a new password: popup notification should appear");
  Services.logins.removeAllLogins();
  Services.logins.addLogin(login);
  // Sanity check the HTTP login exists.
  is(Services.logins.getAllLogins().length, 1, "Should have the HTTP login");

  await BrowserTestUtils.withNewTab({
    gBrowser: normalWin.gBrowser,
    url: form2Url,
  }, async function(browser) {
    let fieldValues = await submitForm(browser, "formsubmit.sjs", {
      "#pass": "notifyp1",
      "#newpass": "notifyp2",
    });
    let notif = getCaptureDoorhanger("password-change", PopupNotifications, browser);
    ok(notif, "got notification popup");
    if (notif) {
      notif.remove();
    }
  });
});

add_task(async function test_private_new_password_5() {
  info("test 5: run with a login, in private mode," +
      "add a new password: popup notification should not appear");
  await BrowserTestUtils.withNewTab({
    gBrowser: privateWin.gBrowser,
    url: form2Url,
  }, async function(browser) {
    let fieldValues = await submitForm(browser, "formsubmit.sjs", {
      "#pass": "notifyp1",
      "#newpass": "notifyp2",
    });
    is(fieldValues.password, "notifyp1", "Checking submitted password");
    let notif = getCaptureDoorhanger("password-change", PopupNotifications, browser);
    ok(!notif, "Expected no notification popup");
    if (notif) {
      notif.remove();
    }
  });
});

add_task(async function test_normal_with_login_6() {
  info("test 6: run with a login, outside of private mode, " +
      "submit with an existing password (from test 4): popup notification should appear");
  await BrowserTestUtils.withNewTab({
    gBrowser: normalWin.gBrowser,
    url: form2Url,
  }, async function(browser) {
    let fieldValues = await submitForm(browser, "formsubmit.sjs", {
      "#pass": "notifyp1",
      "#newpass": "notifyp2",
    });
    let notif = getCaptureDoorhanger("password-change", PopupNotifications, browser);
    ok(notif, "got notification popup");
    if (notif) {
      notif.remove();
    }
    Services.logins.removeLogin(login);
  });
});

add_task(async function test_normal_autofilled_7() {
  info("test 7: verify that the user/pass pair was autofilled");
  Services.logins.addLogin(login);

  // Sanity check the HTTP login exists.
  is(Services.logins.getAllLogins().length, 1, "Should have the HTTP login");

  await BrowserTestUtils.withNewTab({
    gBrowser: normalWin.gBrowser,
    url: "about:blank",
  }, async function(browser) {
    // Add the observer before loading the form page
    let formFilled = ContentTask.spawn(browser, null, async function() {
      const {TestUtils} = ChromeUtils.import("resource://testing-common/TestUtils.jsm");
      await TestUtils.topicObserved("passwordmgr-processed-form");
      info("passwordmgr-processed-form observed");
      await Promise.resolve();
    });

    info("withNewTab loading form uri");
    await BrowserTestUtils.loadURI(browser, form1Url);
    await formFilled;
    info("withNewTab callback, form was filled");

    // the form should have been autofilled, so submit without updating field values
    let fieldValues = await submitForm(browser, "formsubmit.sjs", {});
    is(fieldValues.username, "notifyu1", "Checking submitted username");
    is(fieldValues.password, "notifyp1", "Checking submitted password");
  });
});

add_task(async function test_private_not_autofilled_8() {
  info("test 8: verify that the user/pass pair was not autofilled");
  // Sanity check the HTTP login exists.
  is(Services.logins.getAllLogins().length, 1, "Should have the HTTP login");

  await BrowserTestUtils.withNewTab({
    gBrowser: privateWin.gBrowser,
    url: form1Url,
  }, async function(browser) {
    let fieldValues = await submitForm(browser, "formsubmit.sjs", {});
    ok(!fieldValues.username, "Checking submitted username");
    ok(!fieldValues.password, "Checking submitted password");
  });
});

add_task(async function test_private_autocomplete_9() {
  info("test 9: verify that the user/pass pair was available for autocomplete");
  // Sanity check the HTTP login exists.
  is(Services.logins.getAllLogins().length, 1, "Should have the HTTP login");

  await BrowserTestUtils.withNewTab({
    gBrowser: privateWin.gBrowser,
    url: form1Url,
  }, async function(browser) {
    let popup = document.getElementById("PopupAutoComplete");
    ok(popup, "Got popup");

    let promiseShown = BrowserTestUtils.waitForEvent(popup, "popupshown");

    // focus the user field. This should trigger the autocomplete menu
    await ContentTask.spawn(browser, null, async function() {
      content.document.getElementById("user").focus();
    });
    await promiseShown;
    ok(promiseShown, "autocomplete shown");

    let promiseFormInput = ContentTask.spawn(browser, null, async function() {
      let doc = content.document;
      await new Promise(resolve => {
        doc.getElementById("form").addEventListener("input", resolve, { once: true });
      });
    });
    info("sending keys");
    // select the item and hit enter to fill the form
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
    await promiseFormInput;

    let fieldValues = await submitForm(browser, "formsubmit.sjs", {});
    is(fieldValues.username, "notifyu1", "Checking submitted username");
    is(fieldValues.password, "notifyp1", "Checking submitted password");
  });
}).skip(); // Bug 1523777

add_task(async function test_normal_autofilled_10() {
  info("test 10: verify that the user/pass pair does get autofilled in non-private window");
  // Sanity check the HTTP login exists.
  is(Services.logins.getAllLogins().length, 1, "Should have the HTTP login");

  await BrowserTestUtils.withNewTab({
    gBrowser: normalWin.gBrowser,
    url: form1Url,
  }, async function(browser) {
    let fieldValues = await submitForm(browser, "formsubmit.sjs", {});
    is(fieldValues.username, "notifyu1", "Checking submitted username");
    is(fieldValues.password, "notifyp1", "Checking submitted password");
  });
});

add_task(async function test_cleanup() {
  Services.logins.removeAllLogins();
  await BrowserTestUtils.closeWindow(normalWin);
  await BrowserTestUtils.closeWindow(privateWin);
});
