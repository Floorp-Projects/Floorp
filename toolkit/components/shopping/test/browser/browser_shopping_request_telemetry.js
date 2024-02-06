/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const INVALID_RESPONSE = "https://example.com/Some-Product/dp/INVALID123";
const SERVICE_UNAVAILABLE = "https://example.com/Some-Product/dp/HTTPERR503";
const TOO_MANY_REQUESTS = "https://example.com/Some-Product/dp/HTTPERR429";

function assertEventMatches(gleanEvents, requiredValues) {
  if (!gleanEvents?.length) {
    return Assert.ok(
      !!gleanEvents?.length,
      `${requiredValues?.name} event recorded`
    );
  }
  let limitedEvent = Object.assign({}, gleanEvents[0]);
  for (let k of Object.keys(limitedEvent)) {
    if (!requiredValues.hasOwnProperty(k)) {
      delete limitedEvent[k];
    }
  }
  return Assert.deepEqual(limitedEvent, requiredValues);
}

async function testProductURL(url) {
  await BrowserTestUtils.withNewTab(
    {
      url,
      gBrowser,
    },
    async browser => {
      let sidebar = gBrowser
        .getPanel(browser)
        .querySelector("shopping-sidebar");

      await promiseSidebarUpdated(sidebar, url);
    }
  );
}

add_task(async function test_shopping_server_failure_telemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
    ],
  });
  Services.fog.testResetFOG();

  await testProductURL(SERVICE_UNAVAILABLE);

  await Services.fog.testFlushAllChildren();

  const events = Glean.shoppingProduct.serverFailure.testGetValue();
  assertEventMatches(events, {
    category: "shopping_product",
    name: "server_failure",
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_shopping_request_failure_telemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
    ],
  });
  Services.fog.testResetFOG();

  await testProductURL(TOO_MANY_REQUESTS);

  await Services.fog.testFlushAllChildren();

  const events = Glean.shoppingProduct.requestFailure.testGetValue();
  assertEventMatches(events, {
    category: "shopping_product",
    name: "request_failure",
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_shopping_request_retried_telemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
    ],
  });
  Services.fog.testResetFOG();

  await testProductURL(SERVICE_UNAVAILABLE);

  await Services.fog.testFlushAllChildren();

  const events = Glean.shoppingProduct.requestRetried.testGetValue();
  assertEventMatches(events, {
    category: "shopping_product",
    name: "request_retried",
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_shopping_response_invalid_telemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
    ],
  });
  Services.fog.testResetFOG();

  await testProductURL(INVALID_RESPONSE);

  await Services.fog.testFlushAllChildren();
  const events = Glean.shoppingProduct.invalidResponse.testGetValue();
  assertEventMatches(events, {
    category: "shopping_product",
    name: "invalid_response",
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_shopping_ohttp_invalid_telemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.shopping.ohttpRelayURL", "https://example.com/api"],
      ["toolkit.shopping.ohttpConfigURL", "https://example.com/config"],
    ],
  });
  Services.fog.testResetFOG();

  await testProductURL(PRODUCT_TEST_URL);

  await Services.fog.testFlushAllChildren();

  const events = Glean.shoppingProduct.invalidOhttpConfig.testGetValue();
  assertEventMatches(events, {
    category: "shopping_product",
    name: "invalid_ohttp_config",
  });

  await SpecialPowers.popPrefEnv();
});
