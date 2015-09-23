/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const frameSource = `<a href="about:mozilla">good</a>`;
const source = `<html><iframe srcdoc='${frameSource}' id="f"></iframe></html>`;

add_task(function*() {
  let url = `data:text/html,${source}`;
  yield BrowserTestUtils.withNewTab({ gBrowser, url }, checkFrameSource);
});

function* checkFrameSource() {
  let sourceTab = yield openViewFrameSourceTab("#f");
  registerCleanupFunction(function() {
    gBrowser.removeTab(sourceTab);
  });

  yield waitForSourceLoaded(sourceTab);

  let browser = gBrowser.selectedBrowser;
  let textContent = yield ContentTask.spawn(browser, {}, function*() {
    return content.document.body.textContent;
  });
  is(textContent, frameSource, "Correct content loaded");
  let id = yield ContentTask.spawn(browser, {}, function*() {
    return content.document.body.id;
  });
  is(id, "viewsource", "View source mode enabled")
}
