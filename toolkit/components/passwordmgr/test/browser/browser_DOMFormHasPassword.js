const ids = {
  INPUT_ID: "input1",
  FORM1_ID: "form1",
  FORM2_ID: "form2",
  CHANGE_INPUT_ID: "input2",
};

function task(contentIds) {
  let resolve;
  let promise = new Promise(r => { resolve = r; });

  function unexpectedContentEvent(evt) {
    ok(false, "Received a " + evt.type + " event on content");
  }

  var gDoc = null;

  addEventListener("load", tabLoad, true);

  function tabLoad() {
    if (content.location.href == "about:blank")
      return;
    removeEventListener("load", tabLoad, true);

    gDoc = content.document;
    gDoc.addEventListener("DOMFormHasPassword", unexpectedContentEvent);
    gDoc.defaultView.setTimeout(test_inputAdd, 0);
  }

  function test_inputAdd() {
    addEventListener("DOMFormHasPassword", test_inputAddHandler, false);
    let input = gDoc.createElementNS("http://www.w3.org/1999/xhtml", "input");
    input.setAttribute("type", "password");
    input.setAttribute("id", contentIds.INPUT_ID);
    input.setAttribute("data-test", "unique-attribute");
    gDoc.getElementById(contentIds.FORM1_ID).appendChild(input);
  }

  function test_inputAddHandler(evt) {
    removeEventListener(evt.type, test_inputAddHandler, false);
    is(evt.target.id, contentIds.FORM1_ID,
       evt.type + " event targets correct form element (added password element)");
    gDoc.defaultView.setTimeout(test_inputChangeForm, 0);
  }

  function test_inputChangeForm() {
    addEventListener("DOMFormHasPassword", test_inputChangeFormHandler, false);
    let input = gDoc.getElementById(contentIds.INPUT_ID);
    input.setAttribute("form", contentIds.FORM2_ID);
  }

  function test_inputChangeFormHandler(evt) {
    removeEventListener(evt.type, test_inputChangeFormHandler, false);
    is(evt.target.id, contentIds.FORM2_ID,
       evt.type + " event targets correct form element (changed form)");
    gDoc.defaultView.setTimeout(test_inputChangesType, 0);
  }

  function test_inputChangesType() {
    addEventListener("DOMFormHasPassword", test_inputChangesTypeHandler, false);
    let input = gDoc.getElementById(contentIds.CHANGE_INPUT_ID);
    input.setAttribute("type", "password");
  }

  function test_inputChangesTypeHandler(evt) {
    removeEventListener(evt.type, test_inputChangesTypeHandler, false);
    is(evt.target.id, contentIds.FORM1_ID,
       evt.type + " event targets correct form element (changed type)");
    gDoc.defaultView.setTimeout(finish, 0);
  }

  function finish() {
    gDoc.removeEventListener("DOMFormHasPassword", unexpectedContentEvent);
    resolve();
  }

  return promise;
}

add_task(async function() {
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  let promise = ContentTask.spawn(tab.linkedBrowser, ids, task);
  tab.linkedBrowser.loadURI("data:text/html;charset=utf-8," +
			    "<html><body>" +
			    "<form id='" + ids.FORM1_ID + "'>" +
                            "<input id='" + ids.CHANGE_INPUT_ID + "'></form>" +
			    "<form id='" + ids.FORM2_ID + "'></form>" +
			    "</body></html>");
  await promise;

  ok(true, "Test completed");
  gBrowser.removeCurrentTab();
});
