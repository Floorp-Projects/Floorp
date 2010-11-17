/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Patrick Walton <pcwalton@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/toolkit/components/console/" +
                 "hudservice/tests/browser/test-bug-597136-external-script-" +
                 "errors.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoaded, true);
}

function tabLoaded(aEvent) {
  browser.removeEventListener("load", tabLoaded, true);
  openConsole();

  browser.addEventListener("load", contentLoaded, true);
  content.location.reload();
}

function contentLoaded(aEvent) {
  browser.removeEventListener("load", contentLoaded, true);

  let button = content.document.querySelector("button");
  EventUtils.sendMouseEvent({ type: "click" }, button, content);
  executeSoon(buttonClicked);
}

function buttonClicked() {
  let hudId = HUDService.getHudIdByWindow(content);
  let outputNode = HUDService.getOutputNodeById(hudId);

  const successMsg = "the error from the external script was logged";
  const errorMsg = "the error from the external script was not logged";

  testLogEntry(outputNode, "bogus", { success: successMsg, err: errorMsg });

  finishTest();
}

