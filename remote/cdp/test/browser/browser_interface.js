/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function navigator_webdriver({ client }) {
  const { Runtime } = client;

  const url = toDataURL("default-test-page");
  await loadURL(url);

  await enableRuntime(client);

  const { result } = await Runtime.evaluate({
    expression: "navigator.webdriver",
  });

  is(result.type, "boolean", "The returned type is correct");
  is(result.value, true, "navigator.webdriver is enabled");
});
