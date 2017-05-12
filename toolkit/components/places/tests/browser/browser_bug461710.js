const kRed = "rgb(255, 0, 0)";
const kBlue = "rgb(0, 0, 255)";

const prefix = "http://example.com/tests/toolkit/components/places/tests/browser/461710_";

add_task(async function() {
  let contentPage = prefix + "iframe.html";
  let normalWindow = await BrowserTestUtils.openNewBrowserWindow();

  let browser = normalWindow.gBrowser.selectedBrowser;
  BrowserTestUtils.loadURI(browser, contentPage);
  await BrowserTestUtils.browserLoaded(browser, contentPage);

  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({private: true});

  browser = privateWindow.gBrowser.selectedBrowser;
  BrowserTestUtils.loadURI(browser, contentPage);
  await BrowserTestUtils.browserLoaded(browser, contentPage);

  let tests = [{
    win: normalWindow,
    topic: "uri-visit-saved",
    subtest: "visited_page.html"
  }, {
    win: normalWindow,
    topic: "visited-status-resolution",
    subtest: "link_page.html",
    color: kRed,
    message: "Visited link coloring should work outside of private mode"
  }, {
    win: privateWindow,
    topic: "visited-status-resolution",
    subtest: "link_page-2.html",
    color: kBlue,
    message: "Visited link coloring should not work inside of private mode"
  }, {
    win: normalWindow,
    topic: "visited-status-resolution",
    subtest: "link_page-3.html",
    color: kRed,
    message: "Visited link coloring should work outside of private mode"
  }];

  let visited_page_url = prefix + tests[0].subtest;
  for (let test of tests) {
    let promise = new Promise(resolve => {
      let uri = NetUtil.newURI(visited_page_url);
      Services.obs.addObserver(function observe(aSubject) {
        if (uri.equals(aSubject.QueryInterface(Ci.nsIURI))) {
          Services.obs.removeObserver(observe, test.topic);
          resolve();
        }
      }, test.topic);
    });
    ContentTask.spawn(test.win.gBrowser.selectedBrowser, prefix + test.subtest, async function(aSrc) {
      content.document.getElementById("iframe").src = aSrc;
    });
    await promise;

    if (test.color) {
      // In e10s waiting for visited-status-resolution is not enough to ensure links
      // have been updated, because it only tells us that messages to update links
      // have been dispatched. We must still wait for the actual links to update.
      await BrowserTestUtils.waitForCondition(async function() {
        let color = await ContentTask.spawn(test.win.gBrowser.selectedBrowser, null, async function() {
          let iframe = content.document.getElementById("iframe");
          let elem = iframe.contentDocument.getElementById("link");
          return content.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindowUtils)
                        .getVisitedDependentComputedStyle(elem, "", "color");
        });
        return (color == test.color);
      }, test.message);
      // The harness will consider the test as failed overall if there were no
      // passes or failures, so record it as a pass.
      ok(true, test.message);
    }
  }

  await BrowserTestUtils.closeWindow(normalWindow);
  await BrowserTestUtils.closeWindow(privateWindow);
});
