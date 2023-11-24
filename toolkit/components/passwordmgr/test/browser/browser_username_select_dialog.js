/*
 * Test username selection dialog, on password update from a p-only form,
 * when there are multiple saved logins on the domain.
 */

// Copied from prompt_common.js. TODO: share the code.
function getSelectDialogDoc() {
  // Trudge through all the open windows, until we find the one
  // that has selectDialog.xhtml loaded.
  // var enumerator = Services.wm.getEnumerator("navigator:browser");
  for (let { docShell } of Services.wm.getEnumerator(null)) {
    var containedDocShells = docShell.getAllDocShellsInSubtree(
      docShell.typeChrome,
      docShell.ENUMERATE_FORWARDS
    );
    for (let childDocShell of containedDocShells) {
      // We don't want it if it's not done loading.
      if (childDocShell.busyFlags != Ci.nsIDocShell.BUSY_FLAGS_NONE) {
        continue;
      }
      var childDoc = childDocShell.docViewer.DOMDocument;

      if (
        childDoc.location.href == "chrome://global/content/selectDialog.xhtml"
      ) {
        return childDoc;
      }
    }
  }

  return null;
}

let nsLoginInfo = new Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  Ci.nsILoginInfo,
  "init"
);
let login1 = new nsLoginInfo(
  "https://example.com",
  "https://example.com",
  null,
  "notifyu1",
  "notifyp1",
  "user",
  "pass"
);
let login1B = new nsLoginInfo(
  "https://example.com",
  "https://example.com",
  null,
  "notifyu1B",
  "notifyp1B",
  "user",
  "pass"
);

add_task(async function test_changeUPLoginOnPUpdateForm_accept() {
  info(
    "Select an u+p login from multiple logins, on password update form, and accept."
  );
  await Services.logins.addLogins([login1, login1B]);

  let selectDialogPromise = TestUtils.topicObserved("select-dialog-loaded");

  await testSubmittingLoginForm(
    "subtst_notifications_change_p.html",
    async function (fieldValues) {
      Assert.equal(fieldValues.username, "null", "Checking submitted username");
      Assert.equal(
        fieldValues.password,
        "pass2",
        "Checking submitted password"
      );

      info("Waiting for select dialog to appear.");
      let doc = (await selectDialogPromise)[0].document;
      let dialog = doc.getElementsByTagName("dialog")[0];
      let listbox = doc.getElementById("list");

      Assert.equal(listbox.selectedIndex, 0, "Checking selected index");
      Assert.equal(listbox.itemCount, 2, "Checking selected length");
      ["notifyu1", "notifyu1B"].forEach((username, i) => {
        Assert.equal(
          listbox.getItemAtIndex(i).label,
          username,
          "Check username selection on dialog"
        );
      });

      dialog.acceptDialog();

      await TestUtils.waitForCondition(() => {
        return !getSelectDialogDoc();
      }, "Wait for selection dialog to disappear.");
    }
  );

  let logins = await Services.logins.getAllLogins();
  Assert.equal(logins.length, 2, "Should have 2 logins");

  let login = SpecialPowers.wrap(logins[0]).QueryInterface(Ci.nsILoginMetaInfo);
  Assert.equal(login.username, "notifyu1", "Check the username unchanged");
  Assert.equal(login.password, "pass2", "Check the password changed");
  Assert.equal(login.timesUsed, 2, "Check times used");

  login = SpecialPowers.wrap(logins[1]).QueryInterface(Ci.nsILoginMetaInfo);
  Assert.equal(login.username, "notifyu1B", "Check the username unchanged");
  Assert.equal(login.password, "notifyp1B", "Check the password unchanged");
  Assert.equal(login.timesUsed, 1, "Check times used");

  // cleanup
  login1.password = "pass2";
  Services.logins.removeLogin(login1);
  login1.password = "notifyp1";

  Services.logins.removeLogin(login1B);
});

add_task(async function test_changeUPLoginOnPUpdateForm_cancel() {
  info(
    "Select an u+p login from multiple logins, on password update form, and cancel."
  );
  await Services.logins.addLogins([login1, login1B]);

  let selectDialogPromise = TestUtils.topicObserved("select-dialog-loaded");

  await testSubmittingLoginForm(
    "subtst_notifications_change_p.html",
    async function (fieldValues) {
      Assert.equal(fieldValues.username, "null", "Checking submitted username");
      Assert.equal(
        fieldValues.password,
        "pass2",
        "Checking submitted password"
      );

      info("Waiting for select dialog to appear.");
      let doc = (await selectDialogPromise)[0].document;
      let dialog = doc.getElementsByTagName("dialog")[0];
      let listbox = doc.getElementById("list");

      Assert.equal(listbox.selectedIndex, 0, "Checking selected index");
      Assert.equal(listbox.itemCount, 2, "Checking selected length");
      ["notifyu1", "notifyu1B"].forEach((username, i) => {
        Assert.equal(
          listbox.getItemAtIndex(i).label,
          username,
          "Check username selection on dialog"
        );
      });

      dialog.cancelDialog();

      await TestUtils.waitForCondition(() => {
        return !getSelectDialogDoc();
      }, "Wait for selection dialog to disappear.");
    }
  );

  let logins = await Services.logins.getAllLogins();
  Assert.equal(logins.length, 2, "Should have 2 logins");

  let login = SpecialPowers.wrap(logins[0]).QueryInterface(Ci.nsILoginMetaInfo);
  Assert.equal(login.username, "notifyu1", "Check the username unchanged");
  Assert.equal(login.password, "notifyp1", "Check the password unchanged");
  Assert.equal(login.timesUsed, 1, "Check times used");

  login = SpecialPowers.wrap(logins[1]).QueryInterface(Ci.nsILoginMetaInfo);
  Assert.equal(login.username, "notifyu1B", "Check the username unchanged");
  Assert.equal(login.password, "notifyp1B", "Check the password unchanged");
  Assert.equal(login.timesUsed, 1, "Check times used");

  // cleanup
  Services.logins.removeLogin(login1);
  Services.logins.removeLogin(login1B);
});
