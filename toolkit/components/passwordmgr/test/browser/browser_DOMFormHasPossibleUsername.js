const ids = {
  INPUT_ID: "input1",
  FORM1_ID: "form1",
  FORM2_ID: "form2",
  CHANGE_INPUT_ID: "input2",
  INPUT_TYPE: "",
};

function task({ contentIds, expected }) {
  let resolve;
  let promise = new Promise(r => {
    resolve = r;
  });

  function unexpectedContentEvent(evt) {
    Assert.ok(false, "Received a " + evt.type + " event on content");
  }

  var gDoc = null;

  addEventListener("load", tabLoad, true);

  function tabLoad() {
    if (content.location.href == "about:blank") {
      return;
    }
    removeEventListener("load", tabLoad, true);

    gDoc = content.document;
    gDoc.addEventListener("DOMFormHasPossibleUsername", unexpectedContentEvent);
    gDoc.defaultView.setTimeout(test_inputAdd, 0);
  }

  function test_inputAdd() {
    if (expected) {
      addEventListener(
        "DOMFormHasPossibleUsername",
        test_inputAddHandler,
        false
      );
    } else {
      addEventListener(
        "DOMFormHasPossibleUsername",
        unexpectedContentEvent,
        false
      );
      gDoc.defaultView.setTimeout(test_inputAddHandler, 0);
    }
    let input = gDoc.createElementNS("http://www.w3.org/1999/xhtml", "input");
    input.setAttribute("type", contentIds.INPUT_TYPE);
    input.setAttribute("id", contentIds.INPUT_ID);
    input.setAttribute("data-test", "unique-attribute");
    gDoc.getElementById(contentIds.FORM1_ID).appendChild(input);
  }

  function test_inputAddHandler(evt) {
    if (expected) {
      removeEventListener(evt.type, test_inputAddHandler, false);
      Assert.equal(
        evt.target.id,
        contentIds.FORM1_ID,
        evt.type +
          " event targets correct form element (added possible username element)"
      );
    }
    gDoc.defaultView.setTimeout(test_inputChangeForm, 0);
  }

  function test_inputChangeForm() {
    if (expected) {
      addEventListener(
        "DOMFormHasPossibleUsername",
        test_inputChangeFormHandler,
        false
      );
    } else {
      gDoc.defaultView.setTimeout(test_inputChangeFormHandler, 0);
    }
    let input = gDoc.getElementById(contentIds.INPUT_ID);
    input.setAttribute("form", contentIds.FORM2_ID);
  }

  function test_inputChangeFormHandler(evt) {
    if (expected) {
      removeEventListener(evt.type, test_inputChangeFormHandler, false);
      Assert.equal(
        evt.target.id,
        contentIds.FORM2_ID,
        evt.type + " event targets correct form element (changed form)"
      );
    }
    gDoc.defaultView.setTimeout(test_inputChangesType, 0);
  }

  function test_inputChangesType() {
    if (expected) {
      addEventListener(
        "DOMFormHasPossibleUsername",
        test_inputChangesTypeHandler,
        false
      );
    } else {
      gDoc.defaultView.setTimeout(test_inputChangesTypeHandler, 0);
    }
    let input = gDoc.getElementById(contentIds.CHANGE_INPUT_ID);
    input.setAttribute("type", contentIds.INPUT_TYPE);
  }

  function test_inputChangesTypeHandler(evt) {
    if (expected) {
      removeEventListener(evt.type, test_inputChangesTypeHandler, false);
      Assert.equal(
        evt.target.id,
        contentIds.FORM1_ID,
        evt.type + " event targets correct form element (changed type)"
      );
    }
    gDoc.defaultView.setTimeout(finish, 0);
  }

  function finish() {
    if (!expected) {
      removeEventListener("DOMFormHasPossibleUsername", unexpectedContentEvent);
    }
    gDoc.removeEventListener(
      "DOMFormHasPossibleUsername",
      unexpectedContentEvent
    );
    resolve();
  }

  return promise;
}

add_task(async function test_initialize() {
  Services.prefs.setBoolPref("signon.usernameOnlyForm.enabled", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("signon.usernameOnlyForm.enabled");
  });
});

add_task(async function test_usernameOnlyForm() {
  for (let type of ["text", "email"]) {
    let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));

    ids.INPUT_TYPE = type;
    let promise = ContentTask.spawn(
      tab.linkedBrowser,
      { contentIds: ids, expected: true },
      task
    );
    BrowserTestUtils.loadURIString(
      tab.linkedBrowser,
      "data:text/html;charset=utf-8," +
        "<html><body>" +
        "<form id='" +
        ids.FORM1_ID +
        "'>" +
        "<input id='" +
        ids.CHANGE_INPUT_ID +
        "'></form>" +
        "<form id='" +
        ids.FORM2_ID +
        "'></form>" +
        "</body></html>"
    );
    await promise;

    Assert.ok(true, "Test completed");
    gBrowser.removeCurrentTab();
  }
});

add_task(async function test_nonSupportedInputType() {
  for (let type of ["url", "tel", "number"]) {
    let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));

    ids.INPUT_TYPE = type;
    let promise = ContentTask.spawn(
      tab.linkedBrowser,
      { contentIds: ids, expected: false },
      task
    );
    BrowserTestUtils.loadURIString(
      tab.linkedBrowser,
      "data:text/html;charset=utf-8," +
        "<html><body>" +
        "<form id='" +
        ids.FORM1_ID +
        "'>" +
        "<input id='" +
        ids.CHANGE_INPUT_ID +
        "'></form>" +
        "<form id='" +
        ids.FORM2_ID +
        "'></form>" +
        "</body></html>"
    );
    await promise;

    Assert.ok(true, "Test completed");
    gBrowser.removeCurrentTab();
  }
});

add_task(async function test_usernameOnlyFormPrefOff() {
  Services.prefs.setBoolPref("signon.usernameOnlyForm.enabled", false);

  for (let type of ["text", "email"]) {
    let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));

    ids.INPUT_TYPE = type;
    let promise = ContentTask.spawn(
      tab.linkedBrowser,
      { contentIds: ids, expected: false },
      task
    );
    BrowserTestUtils.loadURIString(
      tab.linkedBrowser,
      "data:text/html;charset=utf-8," +
        "<html><body>" +
        "<form id='" +
        ids.FORM1_ID +
        "'>" +
        "<input id='" +
        ids.CHANGE_INPUT_ID +
        "'></form>" +
        "<form id='" +
        ids.FORM2_ID +
        "'></form>" +
        "</body></html>"
    );
    await promise;

    Assert.ok(true, "Test completed");
    gBrowser.removeCurrentTab();
  }

  Services.prefs.clearUserPref("signon.usernameOnlyForm.enabled");
});
