/**
 * Test that autocomplete is properly attached to a username field which gets
 * focused before DOMContentLoaded in a new browser and process.
 */

"use strict";

add_task(function setup() {
  let nsLoginInfo = Components.Constructor(
    "@mozilla.org/login-manager/loginInfo;1",
    Ci.nsILoginInfo,
    "init"
  );
  ok(nsLoginInfo != null, "nsLoginInfo constructor");

  info("Adding two logins to get autocomplete instead of autofill");
  let login1 = new nsLoginInfo(
    "https://example.com",
    "https://autocomplete:8888",
    null,
    "tempuser1",
    "temppass1"
  );

  let login2 = new nsLoginInfo(
    "https://example.com",
    "https://autocomplete:8888",
    null,
    "testuser2",
    "testpass2"
  );

  Services.logins.addLogin(login1);
  Services.logins.addLogin(login2);
});

add_task(async function test_autocompleteFromUsername() {
  let autocompletePopup = document.getElementById("PopupAutoComplete");
  let autocompletePopupShown = BrowserTestUtils.waitForEvent(
    autocompletePopup,
    "popupshown"
  );

  const URL = `https://example.com${DIRECTORY_PATH}file_focus_before_DOMContentLoaded.sjs`;

  let newTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: URL,
    forceNewProcess: true,
  });

  await SpecialPowers.spawn(
    newTab.linkedBrowser,
    [],
    function checkInitialValues() {
      let doc = content.document;
      let uname = doc.querySelector("#uname");
      let pword = doc.querySelector("#pword");

      ok(uname, "Username field found");
      ok(pword, "Password field found");

      is(doc.activeElement, uname, "#uname element should be focused");
      is(uname.value, "", "Checking username is empty");
      is(pword.value, "", "Checking password is empty");
    }
  );

  await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, newTab.linkedBrowser);
  await autocompletePopupShown;

  let richlistbox = autocompletePopup.richlistbox;
  is(
    richlistbox.localName,
    "richlistbox",
    "The richlistbox should be the first anonymous node"
  );
  for (let i = 0; i < autocompletePopup.view.matchCount; i++) {
    if (
      richlistbox.selectedItem &&
      richlistbox.selectedItem.textContent.includes("tempuser1")
    ) {
      break;
    }
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, newTab.linkedBrowser);
  }

  await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, newTab.linkedBrowser);

  await SpecialPowers.spawn(newTab.linkedBrowser, [], function checkFill() {
    let doc = content.document;
    let uname = doc.querySelector("#uname");
    let pword = doc.querySelector("#pword");

    is(uname.value, "tempuser1", "Checking username is filled");
    is(pword.value, "temppass1", "Checking password is filled");
  });

  BrowserTestUtils.removeTab(newTab);
});
