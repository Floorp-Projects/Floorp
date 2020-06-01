const OUTER_URL =
  "https://test1.example.com:443" + DIRECTORY_PATH + "form_crossframe.html";

async function acceptPasswordSave() {
  let notif = await getCaptureDoorhangerThatMayOpen("password-save");
  let promiseNewSavedPassword = TestUtils.topicObserved(
    "LoginStats:NewSavedPassword",
    (subject, data) => subject == gBrowser.selectedBrowser
  );
  clickDoorhangerButton(notif, REMEMBER_BUTTON);
  await promiseNewSavedPassword;
}

function checkFormFields(browsingContext, prefix, username, password) {
  return SpecialPowers.spawn(
    browsingContext,
    [prefix, username, password],
    (formPrefix, expectedUsername, expectedPassword) => {
      let doc = content.document;
      Assert.equal(
        doc.getElementById(formPrefix + "-username").value,
        expectedUsername,
        "username matches"
      );
      Assert.equal(
        doc.getElementById(formPrefix + "-password").value,
        expectedPassword,
        "password matches"
      );
    }
  );
}

function listenForNotifications(count, expectedFormOrigin) {
  return new Promise(resolve => {
    let notifications = [];
    LoginManagerParent.setListenerForTests((msg, data) => {
      if (msg == "FormProcessed") {
        notifications.push("FormProcessed: " + data.browsingContext.id);
      } else if (msg == "FormSubmit") {
        is(
          data.origin,
          expectedFormOrigin,
          "Message origin should match expected"
        );
        notifications.push("FormSubmit: " + data.data.usernameField.name);
      }
      if (notifications.length == count) {
        resolve(notifications);
      }
    });
  });
}

async function verifyNotifications(notifyPromise, expected) {
  let actual = await notifyPromise;

  is(actual.length, expected.length, "Extra notification(s) sent");
  let expectedItem;
  while ((expectedItem = expected.pop())) {
    let index = actual.indexOf(expectedItem);
    if (index >= 0) {
      actual.splice(index, 1);
    } else {
      ok(false, "Expected notification '" + expectedItem + "' not sent");
    }
  }
}

/*
 * In this test, a frame is loaded with a document that contains a username
 * and password field. This frame also contains another child iframe that
 * itself contains a username and password field. This inner frame is loaded
 * from a different domain than the first.
 *
 * locationMode should be false to submit forms, or true to click a button
 * which changes the location instead. The latter should still save the
 * username and password.
 */
async function submitSomeCrossSiteFrames(locationMode) {
  info("Check with location mode " + locationMode);
  let notifyPromise = listenForNotifications(2);

  let firsttab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    OUTER_URL
  );

  let outerFrameBC = firsttab.linkedBrowser.browsingContext;
  let innerFrameBC = outerFrameBC.children[0];

  await verifyNotifications(notifyPromise, [
    "FormProcessed: " + outerFrameBC.id,
    "FormProcessed: " + innerFrameBC.id,
  ]);

  // Fill in the username and password for both the outer and inner frame
  // and submit the inner frame.
  notifyPromise = listenForNotifications(1, "https://test2.example.org");
  info("submit page after changing inner form");

  await SpecialPowers.spawn(outerFrameBC, [], () => {
    let doc = content.document;
    doc.getElementById("outer-username").setUserInput("outer");
    doc.getElementById("outer-password").setUserInput("outerpass");
  });

  await SpecialPowers.spawn(innerFrameBC, [locationMode], doClick => {
    let doc = content.document;
    doc.getElementById("inner-username").setUserInput("inner");
    doc.getElementById("inner-password").setUserInput("innerpass");
    if (doClick) {
      doc.getElementById("inner-gobutton").click();
    } else {
      doc.getElementById("inner-form").submit();
    }
  });

  await acceptPasswordSave();

  await verifyNotifications(notifyPromise, ["FormSubmit: username"]);

  // Next, open a second tab with the same page in it to verify that the data gets filled properly.
  notifyPromise = listenForNotifications(2);
  let secondtab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    OUTER_URL
  );

  let outerFrameBC2 = secondtab.linkedBrowser.browsingContext;
  let innerFrameBC2 = outerFrameBC2.children[0];
  await verifyNotifications(notifyPromise, [
    "FormProcessed: " + outerFrameBC2.id,
    "FormProcessed: " + innerFrameBC2.id,
  ]);

  await checkFormFields(outerFrameBC2, "outer", "", "");
  await checkFormFields(innerFrameBC2, "inner", "inner", "innerpass");

  // Next, change the username and password fields in the outer frame and submit.
  notifyPromise = listenForNotifications(1, "https://test1.example.com");
  info("submit page after changing outer form");

  await SpecialPowers.spawn(outerFrameBC2, [locationMode], doClick => {
    let doc = content.document;
    doc.getElementById("outer-username").setUserInput("outer2");
    doc.getElementById("outer-password").setUserInput("outerpass2");
    if (doClick) {
      doc.getElementById("outer-gobutton").click();
    } else {
      doc.getElementById("outer-form").submit();
    }

    doc.getElementById("outer-form").submit();
  });

  await acceptPasswordSave();
  await verifyNotifications(notifyPromise, ["FormSubmit: outer-username"]);

  // Finally, open a third tab with the same page in it to verify that the data gets filled properly.
  notifyPromise = listenForNotifications(2);
  let thirdtab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    OUTER_URL
  );

  let outerFrameBC3 = thirdtab.linkedBrowser.browsingContext;
  let innerFrameBC3 = outerFrameBC3.children[0];
  await verifyNotifications(notifyPromise, [
    "FormProcessed: " + outerFrameBC3.id,
    "FormProcessed: " + innerFrameBC3.id,
  ]);

  await checkFormFields(outerFrameBC3, "outer", "outer2", "outerpass2");
  await checkFormFields(innerFrameBC3, "inner", "inner", "innerpass");

  LoginManagerParent.setListenerForTests(null);

  await BrowserTestUtils.removeTab(firsttab);
  await BrowserTestUtils.removeTab(secondtab);
  await BrowserTestUtils.removeTab(thirdtab);

  LoginTestUtils.clearData();
}

add_task(async function cross_site_frames_submit() {
  await submitSomeCrossSiteFrames(false);
});

add_task(async function cross_site_frames_changelocation() {
  await submitSomeCrossSiteFrames(true);
});
