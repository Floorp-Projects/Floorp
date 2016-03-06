const consts = {
  HTML_NS: "http://www.w3.org/1999/xhtml",

  INPUT_ID: "input1",
  FORM1_ID: "form1",
  FORM2_ID: "form2",
  CHANGE_INPUT_ID: "input2",
  BODY_INPUT_ID: "input3",
};

function task(consts) {
  let resolve;
  let promise = new Promise(r => { resolve = r; });

  function unexpectedContentEvent(evt) {
    Assert.ok(false, "Received a " + evt.type + " event on content");
  }

  var gDoc = null;

  addEventListener("load", tabLoad, true);

  function tabLoad() {
    removeEventListener("load", tabLoad, true);
    gDoc = content.document;
    // These events shouldn't escape to content.
    gDoc.addEventListener("DOMInputPasswordAdded", unexpectedContentEvent, false);
    gDoc.defaultView.setTimeout(test_inputAdd, 0);
  }

  function test_inputAdd() {
    addEventListener("DOMInputPasswordAdded", test_inputAddHandler, false);
    let input = gDoc.createElementNS(consts.HTML_NS, "input");
    input.setAttribute("type", "password");
    input.setAttribute("id", consts.INPUT_ID);
    input.setAttribute("data-test", "unique-attribute");
    gDoc.getElementById(consts.FORM1_ID).appendChild(input);
    info("Done appending the input element");
  }

  function test_inputAddHandler(evt) {
    removeEventListener(evt.type, test_inputAddHandler, false);
    Assert.equal(evt.target.id, consts.INPUT_ID,
      evt.type + " event targets correct input element (added password element)");
    gDoc.defaultView.setTimeout(test_inputAddOutsideForm, 0);
  }

  function test_inputAddOutsideForm() {
    addEventListener("DOMInputPasswordAdded", test_inputAddOutsideFormHandler, false);
    let input = gDoc.createElementNS(consts.HTML_NS, "input");
    input.setAttribute("type", "password");
    input.setAttribute("id", consts.BODY_INPUT_ID);
    input.setAttribute("data-test", "unique-attribute");
    gDoc.body.appendChild(input);
    info("Done appending the input element to the body");
  }

  function test_inputAddOutsideFormHandler(evt) {
    removeEventListener(evt.type, test_inputAddOutsideFormHandler, false);
    Assert.equal(evt.target.id, consts.BODY_INPUT_ID,
      evt.type + " event targets correct input element (added password element outside form)");
    gDoc.defaultView.setTimeout(test_inputChangesType, 0);
  }

  function test_inputChangesType() {
    addEventListener("DOMInputPasswordAdded", test_inputChangesTypeHandler, false);
    let input = gDoc.getElementById(consts.CHANGE_INPUT_ID);
    input.setAttribute("type", "password");
  }

  function test_inputChangesTypeHandler(evt) {
    removeEventListener(evt.type, test_inputChangesTypeHandler, false);
    Assert.equal(evt.target.id, consts.CHANGE_INPUT_ID,
      evt.type + " event targets correct input element (changed type)");
    gDoc.defaultView.setTimeout(completeTest, 0);
  }

  function completeTest() {
    Assert.ok(true, "Test completed");
    gDoc.removeEventListener("DOMInputPasswordAdded", unexpectedContentEvent, false);
    resolve();
  }

  return promise;
}

add_task(function* () {
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  let promise = ContentTask.spawn(tab.linkedBrowser, consts, task);
  tab.linkedBrowser.loadURI("data:text/html;charset=utf-8," +
			    "<html><body>" +
			    "<form id='" + consts.FORM1_ID + "'>" +
                            "<input id='" + consts.CHANGE_INPUT_ID + "'></form>" +
			    "<form id='" + consts.FORM2_ID + "'></form>" +
			    "</body></html>");
  yield promise;
  gBrowser.removeCurrentTab();
});

