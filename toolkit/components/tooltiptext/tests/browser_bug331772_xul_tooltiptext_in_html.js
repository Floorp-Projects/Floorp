/**
 * Tests that the tooltiptext attribute is used for XUL elements in an HTML doc.
 */
add_task(function*() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://mochi.test:8888/browser/toolkit/components/tooltiptext/tests/xul_tooltiptext.xhtml",
  }, function*(browser) {
    yield ContentTask.spawn(browser, "", function() {
      let textObj = {};
      let tttp = Cc["@mozilla.org/embedcomp/default-tooltiptextprovider;1"]
                 .getService(Ci.nsITooltipTextProvider);
      let xulToolbarButton = content.document.getElementById("xulToolbarButton");
      ok(tttp.getNodeText(xulToolbarButton, textObj, {}), "should get tooltiptext");
      is(textObj.value, "XUL tooltiptext");
    });
  });
});

