/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "data:text/html,<div style='font-size:3em;" +
  "foobarCssParser:baz'>test CSS parser filter</div>"

function onContentLoaded()
{
  browser.removeEventListener("load", arguments.callee, true);

  let HUD = HUDService.getDisplayByURISpec(content.location.href);
  let hudId = HUD.getAttribute("id");
  let filterBox = HUD.querySelector(".hud-filter-box");
  let outputNode = HUD.querySelector(".hud-output-node");

  let warningFound = "the unknown CSS property warning is displayed";
  let warningNotFound = "could not find the unknown CSS property warning";

  testLogEntry(outputNode, "foobarCssParser",
    { success: warningFound, err: warningNotFound }, true);

  HUDService.setFilterState(hudId, "cssparser", false);

  warningNotFound = "the unknown CSS property warning is not displayed, " +
    "after filtering";
  warningFound = "the unknown CSS property warning is still displayed, " +
    "after filtering";

  testLogEntry(outputNode, "foobarCssParser",
    { success: warningNotFound, err: warningFound }, true, true);

  finishTest();
}

/**
 * Unit test for bug 589162:
 * CSS filtering on the console does not work
 */
function test()
{
  addTab(TEST_URI);
  browser.addEventListener("load", function() {
    browser.removeEventListener("load", arguments.callee, true);

    openConsole();
    browser.addEventListener("load", onContentLoaded, true);
    content.location.reload();
  }, true);
}

