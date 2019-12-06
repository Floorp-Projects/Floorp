const kRed = "rgb(255, 0, 0)";
const kBlue = "rgb(0, 0, 255)";

const prefix =
  "http://example.com/tests/toolkit/components/places/tests/browser/461710_";

add_task(async function() {
  registerCleanupFunction(PlacesUtils.history.clear);
  let normalWindow = await BrowserTestUtils.openNewBrowserWindow();
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let tests = [
    {
      private: false,
      topic: "uri-visit-saved",
      subtest: "visited_page.html",
    },
    {
      private: false,
      topic: "visited-status-resolution",
      subtest: "link_page.html",
      color: kRed,
      message: "Visited link coloring should work outside of private mode",
    },
    {
      private: true,
      topic: "visited-status-resolution",
      subtest: "link_page-2.html",
      color: kBlue,
      message: "Visited link coloring should not work inside of private mode",
    },
    {
      private: false,
      topic: "visited-status-resolution",
      subtest: "link_page-3.html",
      color: kRed,
      message: "Visited link coloring should work outside of private mode",
    },
  ];

  let uri = Services.io.newURI(prefix + tests[0].subtest);
  for (let test of tests) {
    info(test.subtest);
    let promise = TestUtils.topicObserved(test.topic, subject =>
      uri.equals(subject.QueryInterface(Ci.nsIURI))
    );
    await BrowserTestUtils.withNewTab(
      {
        gBrowser: test.private ? privateWindow.gBrowser : normalWindow.gBrowser,
        url: prefix + test.subtest,
      },
      async function(browser) {
        await promise;
        if (test.color) {
          // In e10s waiting for visited-status-resolution is not enough to ensure links
          // have been updated, because it only tells us that messages to update links
          // have been dispatched. We must still wait for the actual links to update.
          await TestUtils.waitForCondition(async function() {
            let color = await SpecialPowers.spawn(
              browser,
              [],
              async function() {
                let elem = content.document.getElementById("link");
                return content.windowUtils.getVisitedDependentComputedStyle(
                  elem,
                  "",
                  "color"
                );
              }
            );
            return color == test.color;
          }, test.message);
          // The harness will consider the test as failed overall if there were no
          // passes or failures, so record it as a pass.
          ok(true, test.message);
        }
      }
    );
  }

  let promisePBExit = TestUtils.topicObserved("last-pb-context-exited");
  await BrowserTestUtils.closeWindow(privateWindow);
  await promisePBExit;
  await BrowserTestUtils.closeWindow(normalWindow);
});
