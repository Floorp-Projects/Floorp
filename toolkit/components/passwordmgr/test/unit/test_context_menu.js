/*
 * Test the password manager context menu.
 */

"use strict";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/LoginManagerContextMenu.jsm");

XPCOMUtils.defineLazyGetter(this, "_stringBundle", function() {
  return Services.strings.
         createBundle("chrome://passwordmgr/locale/passwordmgr.properties");
});

/**
 * Prepare data for the following tests.
 */
add_task(function* test_initialize() {
  for (let login of loginList()) {
    Services.logins.addLogin(login);
  }
});

/**
 * Tests if the LoginManagerContextMenu returns the correct login items.
 */
add_task(function* test_contextMenuAddAndRemoveLogins() {
  const DOCUMENT_CONTENT = "<form><input id='pw' type=password></form>";
  const INPUT_QUERY = "input[type='password']";

  let testHostnames = [
    "http://www.example.com",
    "http://www2.example.com",
    "http://www3.example.com",
    "http://empty.example.com",
  ];

  for (let hostname of testHostnames) {
    do_print("test for hostname: " + hostname);
    // Get expected logins for this test.
    let logins = getExpectedLogins(hostname);

    // Create the logins menuitems fragment.
    let {fragment, document} = createLoginsFragment(hostname, DOCUMENT_CONTENT, INPUT_QUERY);

    if (!logins.length) {
      Assert.ok(fragment === null, "Null returned. No logins where found.");
      continue;
    }
    let items = [...fragment.querySelectorAll("menuitem")];

    // Check if the items are those expected to be listed.
    Assert.ok(checkLoginItems(logins, items), "All expected logins found.");
    document.body.appendChild(fragment);

    // Try to clear the fragment.
    LoginManagerContextMenu.clearLoginsFromMenu(document);
    Assert.equal(fragment.querySelectorAll("menuitem").length, 0, "All items correctly cleared.");
  }

  Services.logins.removeAllLogins();
});

/**
 * Create a fragment with a menuitem for each login.
 */
function createLoginsFragment(url, content, elementQuery) {
  const CHROME_URL = "chrome://mock-chrome";

  // Create a mock document.
  let document = MockDocument.createTestDocument(CHROME_URL, content);
  let inputElement = document.querySelector(elementQuery);
  MockDocument.mockOwnerDocumentProperty(inputElement, document, url);

  // We also need a simple mock Browser object for this test.
  let browser = {
    ownerDocument: document
  };

  let URI = Services.io.newURI(url, null, null);
  return {
    document,
    fragment: LoginManagerContextMenu.addLoginsToMenu(inputElement, browser, URI),
  };
}

/**
 * Check if every login have it's corresponding menuitem.
 * Duplicates and empty usernames have a date appended.
 */
function checkLoginItems(logins, items) {
  function findDuplicates(unfilteredLoginList) {
    var seen = new Set();
    var duplicates = new Set();
    for (let login of unfilteredLoginList) {
      if (seen.has(login.username)) {
        duplicates.add(login.username);
      }
      seen.add(login.username);
    }
    return duplicates;
  }
  let duplicates = findDuplicates(logins);

  let dateAndTimeFormatter = new Intl.DateTimeFormat(undefined,
                             { day: "numeric", month: "short", year: "numeric" });
  for (let login of logins) {
    if (login.username && !duplicates.has(login.username)) {
      // If login is not duplicate and we can't find an item for it, fail.
      if (!items.find(item => item.label == login.username)) {
        return false;
      }
      continue;
    }

    let meta = login.QueryInterface(Ci.nsILoginMetaInfo);
    let time = dateAndTimeFormatter.format(new Date(meta.timePasswordChanged));
    // If login is duplicate, check if we have a login item with appended date.
    if (login.username && !items.find(item => item.label == login.username + " (" + time + ")")) {
      return false;
    }
    // If login is empty, check if we have a login item with appended date.
    if (!login.username &&
        !items.find(item => item.label == _stringBundle.GetStringFromName("noUsername") + " (" + time + ")")) {
      return false;
    }
  }
  return true;
}

/**
 * Gets the list of expected logins for a hostname.
 */
function getExpectedLogins(hostname) {
  return Services.logins.getAllLogins().filter(entry => entry["hostname"] === hostname);
}

function loginList() {
  return [
      new LoginInfo("http://www.example.com", "http://www.example.com", null,
                    "username1", "password",
                    "form_field_username", "form_field_password"),

      new LoginInfo("http://www.example.com", "http://www.example.com", null,
                    "username2", "password",
                    "form_field_username", "form_field_password"),

      new LoginInfo("http://www2.example.com", "http://www.example.com", null,
                    "username", "password",
                    "form_field_username", "form_field_password"),
      new LoginInfo("http://www2.example.com", "http://www2.example.com", null,
                    "username", "password2",
                    "form_field_username", "form_field_password"),
      new LoginInfo("http://www2.example.com", "http://www2.example.com", null,
                    "username2", "password2",
                    "form_field_username", "form_field_password"),

      new LoginInfo("http://www3.example.com", "http://www.example.com", null,
                    "", "password",
                    "form_field_username", "form_field_password"),
      new LoginInfo("http://www3.example.com", "http://www3.example.com", null,
                    "", "password2",
                    "form_field_username", "form_field_password"),
  ];
}
