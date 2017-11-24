/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const frameSource = `<a href="about:mozilla">good</a>`;
const source = `<html><iframe srcdoc='${frameSource}' id="f"></iframe></html>`;

add_task(async function() {
  let url = `data:text/html,${source}`;
  await BrowserTestUtils.withNewTab({ gBrowser, url }, checkFrameSource);
});

async function checkFrameSource() {
  let sourceTab = await openViewFrameSourceTab("#f");
  registerCleanupFunction(function() {
    gBrowser.removeTab(sourceTab);
  });

  await waitForSourceLoaded(sourceTab);

  let browser = gBrowser.selectedBrowser;
  let textContent = await ContentTask.spawn(browser, {}, async function() {
    return content.document.body.textContent;
  });
  is(textContent, frameSource, "Correct content loaded");
  let id = await ContentTask.spawn(browser, {}, async function() {
    return content.document.body.id;
  });
  is(id, "viewsource", "View source mode enabled");
}
