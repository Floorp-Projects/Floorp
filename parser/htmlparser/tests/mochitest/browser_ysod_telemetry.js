"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

add_task(async function test_popup_opened() {
  Services.telemetry.clearEvents();
  Services.telemetry.setEventRecordingEnabled("ysod", true);

  const PAGE_URL = getRootDirectory(gTestPath) + "broken_xml.xhtml";
  let viewSourceTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    PAGE_URL
  );

  let content = await SpecialPowers.spawn(
    viewSourceTab.linkedBrowser,
    [],
    async function() {
      return content.document.documentElement.innerHTML;
    }
  );

  ok(content.includes("XML"), "Document shows XML error");

  TelemetryTestUtils.assertEvents(
    [
      {
        method: "shown",
        object: "ysod",
        value: PAGE_URL.substr(0, 80),
        extra: { error_code: "11", location: "3:12" },
      },
    ],
    {
      category: "ysod",
    },
    { process: "parent" }
  );
  BrowserTestUtils.removeTab(viewSourceTab);
});
