const consts = {
  HTML_NS: "http://www.w3.org/1999/xhtml",

  INPUT_ID: "input1",
  FORM1_ID: "form1",
  FORM2_ID: "form2",
  CHANGE_INPUT_ID: "input2",
  BODY_INPUT_ID: "input3",
};

function task(contentConsts) {
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
    removeEventListener("load", tabLoad, true);
    gDoc = content.document;
    // These events shouldn't escape to content.
    gDoc.addEventListener("DOMInputPasswordAdded", unexpectedContentEvent);
    gDoc.defaultView.setTimeout(test_inputAdd, 0);
  }

  function test_inputAdd() {
    addEventListener("DOMInputPasswordAdded", test_inputAddHandler, false);
    let input = gDoc.createElementNS(contentConsts.HTML_NS, "input");
    input.setAttribute("type", "password");
    input.setAttribute("id", contentConsts.INPUT_ID);
    input.setAttribute("data-test", "unique-attribute");
    gDoc.getElementById(contentConsts.FORM1_ID).appendChild(input);
    info("Done appending the input element");
  }

  function test_inputAddHandler(evt) {
    removeEventListener(evt.type, test_inputAddHandler, false);
    Assert.equal(
      evt.target.id,
      contentConsts.INPUT_ID,
      evt.type + " event targets correct input element (added password element)"
    );
    gDoc.defaultView.setTimeout(test_inputAddOutsideForm, 0);
  }

  function test_inputAddOutsideForm() {
    addEventListener(
      "DOMInputPasswordAdded",
      test_inputAddOutsideFormHandler,
      false
    );
    let input = gDoc.createElementNS(contentConsts.HTML_NS, "input");
    input.setAttribute("type", "password");
    input.setAttribute("id", contentConsts.BODY_INPUT_ID);
    input.setAttribute("data-test", "unique-attribute");
    gDoc.body.appendChild(input);
    info("Done appending the input element to the body");
  }

  function test_inputAddOutsideFormHandler(evt) {
    removeEventListener(evt.type, test_inputAddOutsideFormHandler, false);
    Assert.equal(
      evt.target.id,
      contentConsts.BODY_INPUT_ID,
      evt.type +
        " event targets correct input element (added password element outside form)"
    );
    gDoc.defaultView.setTimeout(test_inputChangesType, 0);
  }

  function test_inputChangesType() {
    addEventListener(
      "DOMInputPasswordAdded",
      test_inputChangesTypeHandler,
      false
    );
    let input = gDoc.getElementById(contentConsts.CHANGE_INPUT_ID);
    input.setAttribute("type", "password");
  }

  function test_inputChangesTypeHandler(evt) {
    removeEventListener(evt.type, test_inputChangesTypeHandler, false);
    Assert.equal(
      evt.target.id,
      contentConsts.CHANGE_INPUT_ID,
      evt.type + " event targets correct input element (changed type)"
    );
    gDoc.defaultView.setTimeout(completeTest, 0);
  }

  function completeTest() {
    Assert.ok(true, "Test completed");
    gDoc.removeEventListener("DOMInputPasswordAdded", unexpectedContentEvent);
    resolve();
  }

  return promise;
}

add_task(async function() {
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));
  let promise = ContentTask.spawn(tab.linkedBrowser, consts, task);
  BrowserTestUtils.loadURI(
    tab.linkedBrowser,
    "data:text/html;charset=utf-8," +
      "<html><body>" +
      "<form id='" +
      consts.FORM1_ID +
      "'>" +
      "<input id='" +
      consts.CHANGE_INPUT_ID +
      "'></form>" +
      "<form id='" +
      consts.FORM2_ID +
      "'></form>" +
      "</body></html>"
  );
  await promise;
  gBrowser.removeCurrentTab();
});
