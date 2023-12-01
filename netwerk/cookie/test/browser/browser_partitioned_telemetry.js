/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URL =
  "https://example.com/browser/netwerk/cookie/test/browser/file_empty.html";

async function validateTelemetryValues(
  { setCookies, setForeigns, setPartitioneds, setForeignPartitioneds },
  message
) {
  await Services.fog.testFlushAllChildren();
  let setCookieTelemetry = Glean.networking.setCookie.testGetValue();
  is(
    setCookieTelemetry ?? undefined,
    setCookies,
    message + " - all set cookies"
  );
  let foreignTelemetry = Glean.networking.setCookieForeign.testGetValue();
  is(
    foreignTelemetry?.numerator,
    setForeigns,
    message + " - foreign set cookies"
  );
  is(
    foreignTelemetry?.denominator,
    setCookies,
    message + " - foreign set cookies denominator"
  );
  let partitonedTelemetry =
    Glean.networking.setCookiePartitioned.testGetValue();
  is(
    partitonedTelemetry?.numerator,
    setPartitioneds,
    message + " - partitioned set cookies"
  );
  is(
    partitonedTelemetry?.denominator,
    setCookies,
    message + " - partitioned set cookies denominator"
  );
  let foreignPartitonedTelemetry =
    Glean.networking.setCookieForeignPartitioned.testGetValue();
  is(
    foreignPartitonedTelemetry?.numerator,
    setForeignPartitioneds,
    message + " - foreign partitioned set cookies"
  );
  is(
    foreignPartitonedTelemetry?.denominator,
    setCookies,
    message + " - foreign partitioned set cookies denominator"
  );
}

add_task(async () => {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  await validateTelemetryValues({}, "initially empty");

  // open a browser window for the test
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_URL);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  // Set cookies with Javascript
  await SpecialPowers.spawn(browser, [], function () {
    content.document.cookie = "a=1; Partitioned; SameSite=None; Secure";
    content.document.cookie = "b=2; SameSite=None; Secure";
  });
  await validateTelemetryValues(
    {
      setCookies: 2,
      setForeigns: 0,
      setPartitioneds: 1,
      setForeignPartitioneds: 0,
    },
    "javascript cookie"
  );

  // Set cookies with HTTP
  await SpecialPowers.spawn(browser, [], async function () {
    await content.fetch("partitioned.sjs");
  });
  await validateTelemetryValues(
    {
      setCookies: 4,
      setForeigns: 0,
      setPartitioneds: 2,
      setForeignPartitioneds: 0,
    },
    "same site fetch"
  );

  // Set cookies with cross-site HTTP
  await SpecialPowers.spawn(browser, [], async function () {
    await content.fetch(
      "https://example.org/browser/netwerk/cookie/test/browser/partitioned.sjs",
      { credentials: "include" }
    );
  });
  await validateTelemetryValues(
    {
      setCookies: 6,
      setForeigns: 2,
      setPartitioneds: 3,
      setForeignPartitioneds: 1,
    },
    "foreign fetch"
  );

  // Set cookies with cross-site HTTP redirect
  await SpecialPowers.spawn(browser, [], async function () {
    await content.fetch(
      encodeURI(
        "https://example.org/browser/netwerk/cookie/test/browser/partitioned.sjs?redirect=https://example.com/browser/netwerk/cookie/test/browser/partitioned.sjs?nocookie"
      ),
      { credentials: "include" }
    );
  });

  await validateTelemetryValues(
    {
      setCookies: 8,
      setForeigns: 4,
      setPartitioneds: 4,
      setForeignPartitioneds: 2,
    },
    "foreign fetch redirect"
  );

  // remove the tab
  gBrowser.removeTab(tab);
});
