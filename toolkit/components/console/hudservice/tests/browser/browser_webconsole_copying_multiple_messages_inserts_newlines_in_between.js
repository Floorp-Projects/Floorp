/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Patrick Walton <pcwalton@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that copying multiple messages inserts newlines in between.

const TEST_URI = "http://example.com/";

function test()
{
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", onLoad, false);
}

function onLoad() {
  browser.removeEventListener("DOMContentLoaded", onLoad,
                                               false);
  executeSoon(testNewlines);
}

function testNewlines() {
  openConsole();
  hudId = HUDService.displaysIndex()[0];
  ok(hudId != null, "we have the HUD ID");

  HUDService.clearDisplay(hudId);

  let contentWindow = browser.contentWindow;
  let console = contentWindow.wrappedJSObject.console;
  ok(console != null, "we have the console object");

  for (let i = 0; i < 20; i++) {
    console.log("Hello world!");
  }

  let hudNode = HUDService.getOutputNodeById(hudId);
  let outputNode = hudNode.querySelector(".hud-output-node");
  ok(outputNode != null, "we have the output node");

  outputNode.selectAll();
  outputNode.focus();

  let clipboardTexts = [];
  for (let i = 0; i < outputNode.itemCount; i++) {
    let item = outputNode.getItemAtIndex(i);
    clipboardTexts.push("[" + ConsoleUtils.timestampString(item.timestamp) +
                        "] " + item.clipboardText);
  }

  waitForClipboard(clipboardTexts.join("\n"),
                   function() { goDoCommand("cmd_copy"); },
                   finishTest, finishTest);
}

