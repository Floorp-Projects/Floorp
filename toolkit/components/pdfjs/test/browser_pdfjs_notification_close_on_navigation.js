/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

add_task(async function test_notification_is_removed_upon_navigation() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      await waitForPdfJSAnnotationLayer(
        browser,
        TESTROOT + "file_pdfjs_form.pdf"
      );

      // Trigger the PDFjs fallback notification bar
      await ContentTask.spawn(browser, null, async () => {
        content.wrappedJSObject.PDFViewerApplication.fallback();
      });

      let notification = await TestUtils.waitForCondition(
        () =>
          browser &&
          gBrowser.getNotificationBox(browser) &&
          gBrowser.getNotificationBox(browser).currentNotification,
        "waiting for notification"
      );
      ok(notification, "A notification should be shown");
      is(
        notification.getAttribute("value"),
        "pdfjs-fallback",
        "Notification should be pdfjs-fallback"
      );

      BrowserTestUtils.startLoadingURIString(browser, "https://example.com");

      await TestUtils.waitForCondition(
        () => !notification.parentNode,
        "notification should be removed upon navigation to example.com"
      );
    }
  );
});
