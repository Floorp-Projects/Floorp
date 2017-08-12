"use strict";

const methodData = [{
  supportedMethods: ["basic-card"],
}];
const details = {
  total: {
    label: "Total due",
    amount: { currency: "USD", value: "60.00" },
  },
};

add_task(async function test_show_abort_dialog() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    // start by creating a PaymentRequest, and show it
    await ContentTask.spawn(browser, {methodData, details}, ContentTasks.createAndShowRequest);

    // get a reference to the UI dialog and the requestId
    let win = await getDialogWindow();
    let requestId = requestIdForWindow(win);
    ok(requestId, "requestId should be defined");
    ok(!win.closed, "dialog should not be closed");

    // abort the payment request
    await ContentTask.spawn(browser, null, async() => content.rq.abort());
    ok(win.closed, "dialog should be closed");
  });
});
