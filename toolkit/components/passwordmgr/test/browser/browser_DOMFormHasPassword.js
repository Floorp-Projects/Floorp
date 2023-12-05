const ids = {
  INPUT_ID: "input1",
  FORM1_ID: "form1",
  FORM2_ID: "form2",
  CHANGE_INPUT_ID: "input2",
};

function task(contentIds) {
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
    gDoc.addEventListener("DOMFormHasPassword", unexpectedContentEvent);
    addEventListener("DOMFormHasPassword", unexpectedContentEvent);
    gDoc.defaultView.setTimeout(test_inputAdd, 0);
  }

  function test_inputAdd() {
    addEventListener("DOMFormHasPassword", test_inputAddHandler, {
      once: true,
      capture: true,
    });
    let input = gDoc.createElementNS("http://www.w3.org/1999/xhtml", "input");
    input.setAttribute("type", "password");
    input.setAttribute("id", contentIds.INPUT_ID);
    input.setAttribute("data-test", "unique-attribute");
    gDoc.getElementById(contentIds.FORM1_ID).appendChild(input);
  }

  function test_inputAddHandler(evt) {
    evt.stopPropagation();
    Assert.equal(
      evt.target.id,
      contentIds.FORM1_ID,
      evt.type + " event targets correct form element (added password element)"
    );
    gDoc.defaultView.setTimeout(test_inputChangeForm, 0);
  }

  function test_inputChangeForm() {
    addEventListener("DOMFormHasPassword", test_inputChangeFormHandler, {
      once: true,
      capture: true,
    });
    let input = gDoc.getElementById(contentIds.INPUT_ID);
    input.setAttribute("form", contentIds.FORM2_ID);
  }

  function test_inputChangeFormHandler(evt) {
    evt.stopPropagation();
    Assert.equal(
      evt.target.id,
      contentIds.FORM2_ID,
      evt.type + " event targets correct form element (changed form)"
    );
    gDoc.defaultView.setTimeout(test_inputChangesType, 0);
  }

  function test_inputChangesType() {
    addEventListener("DOMFormHasPassword", test_inputChangesTypeHandler, {
      once: true,
      capture: true,
    });
    let input = gDoc.getElementById(contentIds.CHANGE_INPUT_ID);
    input.setAttribute("type", "password");
  }

  function test_inputChangesTypeHandler(evt) {
    evt.stopPropagation();
    Assert.equal(
      evt.target.id,
      contentIds.FORM1_ID,
      evt.type + " event targets correct form element (changed type)"
    );
    gDoc.defaultView.setTimeout(finish, 0);
  }

  function finish() {
    removeEventListener("DOMFormHasPassword", unexpectedContentEvent);
    gDoc.removeEventListener("DOMFormHasPassword", unexpectedContentEvent);
    resolve();
  }

  return promise;
}

add_task(async function test_disconnectedInputs() {
  const tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));
  await ContentTask.spawn(tab.linkedBrowser, [], async () => {
    const unexpectedEvent = evt => {
      Assert.ok(
        false,
        `${evt.type} should not be fired for disconnected forms.`
      );
    };

    addEventListener("DOMFormHasPassword", unexpectedEvent);

    const form = content.document.createElement("form");
    const passwordInput = content.document.createElement("input");
    passwordInput.setAttribute("type", "password");
    form.appendChild(passwordInput);

    // Delay the execution for a bit to allow time for any asynchronously
    // dispatched 'DOMFormHasPassword' events to be processed.
    // This is necessary because such events might not be triggered immediately,
    // and we want to ensure that if they are dispatched, they are captured
    // before we remove the event listener.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 50));
    removeEventListener("DOMFormHasPassword", unexpectedEvent);
  });

  Assert.ok(true, "Test completed");
  gBrowser.removeCurrentTab();
});

add_task(async function () {
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));

  let promise = ContentTask.spawn(tab.linkedBrowser, ids, task);
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    `data:text/html;charset=utf-8,
      <html><body>
      <form id="${ids.FORM1_ID}">
        <input id="${ids.CHANGE_INPUT_ID}">
      </form>
      <form id="${ids.FORM2_ID}"></form>
      </body></html>`
  );
  await promise;

  Assert.ok(true, "Test completed");
  gBrowser.removeCurrentTab();
});
