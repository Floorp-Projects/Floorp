const HTML_NS = "http://www.w3.org/1999/xhtml";

const INPUT_ID = "input1";
const FORM1_ID = "form1";
const FORM2_ID = "form2";
const CHANGE_INPUT_ID = "input2";
const BODY_INPUT_ID = "input3";

function test() {
  waitForExplicitFinish();
  let tab = gBrowser.selectedTab =
	gBrowser.addTab("data:text/html;charset=utf-8," +
			"<html><body>" +
			"<form id='" + FORM1_ID + "'><input id='" + CHANGE_INPUT_ID + "'></form>" +
			"<form id='" + FORM2_ID + "'></form>" +
			"</body></html>");
  tab.linkedBrowser.addEventListener("load", tabLoad, true);
}

function unexpectedContentEvent(evt) {
  ok(false, "Received a " + evt.type + " event on content");
}

var gDoc = null;

function tabLoad() {
  let tab = gBrowser.selectedTab;
  tab.linkedBrowser.removeEventListener("load", tabLoad, true);
  gDoc = gBrowser.selectedBrowser.contentDocument;
  // These events shouldn't escape to content.
  gDoc.addEventListener("DOMInputPasswordAdded", unexpectedContentEvent, false);
  gDoc.defaultView.setTimeout(test_inputAdd, 0);
}

function test_inputAdd() {
  gBrowser.addEventListener("DOMInputPasswordAdded", test_inputAddHandler, false);
  let input = gDoc.createElementNS(HTML_NS, "input");
  input.setAttribute("type", "password");
  input.setAttribute("id", INPUT_ID);
  input.setAttribute("data-test", "unique-attribute");
  gDoc.getElementById(FORM1_ID).appendChild(input);
  info("Done appending the input element");
}

function test_inputAddHandler(evt) {
  gBrowser.removeEventListener(evt.type, test_inputAddHandler, false);
  is(evt.target.id, INPUT_ID,
     evt.type + " event targets correct input element (added password element)");
  gDoc.defaultView.setTimeout(test_inputAddOutsideForm, 0);
}

function test_inputAddOutsideForm() {
  gBrowser.addEventListener("DOMInputPasswordAdded", test_inputAddOutsideFormHandler, false);
  let input = gDoc.createElementNS(HTML_NS, "input");
  input.setAttribute("type", "password");
  input.setAttribute("id", BODY_INPUT_ID);
  input.setAttribute("data-test", "unique-attribute");
  gDoc.body.appendChild(input);
  info("Done appending the input element to the body");
}

function test_inputAddOutsideFormHandler(evt) {
  gBrowser.removeEventListener(evt.type, test_inputAddOutsideFormHandler, false);
  is(evt.target.id, BODY_INPUT_ID,
     evt.type + " event targets correct input element (added password element outside form)");
  gDoc.defaultView.setTimeout(test_inputChangesType, 0);
}

function test_inputChangesType() {
  gBrowser.addEventListener("DOMInputPasswordAdded", test_inputChangesTypeHandler, false);
  let input = gDoc.getElementById(CHANGE_INPUT_ID);
  input.setAttribute("type", "password");
}

function test_inputChangesTypeHandler(evt) {
  gBrowser.removeEventListener(evt.type, test_inputChangesTypeHandler, false);
  is(evt.target.id, CHANGE_INPUT_ID,
     evt.type + " event targets correct input element (changed type)");
  gDoc.defaultView.setTimeout(completeTest, 0);
}

function completeTest() {
  ok(true, "Test completed");
  gDoc.removeEventListener("DOMInputPasswordAdded", unexpectedContentEvent, false);
  gBrowser.removeCurrentTab();
  finish();
}
