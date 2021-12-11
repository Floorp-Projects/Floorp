/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DOC = toDataURL(`<script>document.write(navigator.userAgent);</script>`);

// Network.setUserAgentOverride is a redirect to Emulation.setUserAgentOverride.
// Run at least one test for setting and resetting the user agent to make sure
// that the redirect works.

add_task(async function forwardToEmulation({ client }) {
  const { Network } = client;
  const userAgent = "Mozilla/5.0 (rv: 23) Romanesco/42.0";
  const platform = "foobar";

  await loadURL(DOC);
  const originalUserAgent = await getNavigatorProperty("userAgent");
  const originalPlatform = await getNavigatorProperty("platform");

  isnot(originalUserAgent, userAgent, "Custom user agent hasn't been set");
  isnot(originalPlatform, platform, "Custom platform hasn't been set");

  await Network.setUserAgentOverride({ userAgent, platform });
  await loadURL(DOC);
  is(
    await getNavigatorProperty("userAgent"),
    userAgent,
    "Custom user agent has been set"
  );
  is(
    await getNavigatorProperty("platform"),
    platform,
    "Custom platform has been set"
  );

  await Network.setUserAgentOverride({ userAgent: "", platform: "" });
  await loadURL(DOC);
  is(
    await getNavigatorProperty("userAgent"),
    originalUserAgent,
    "Custom user agent has been reset"
  );
  is(
    await getNavigatorProperty("platform"),
    originalPlatform,
    "Custom platform has been reset"
  );

  await Network.setUserAgentOverride({ userAgent, platform });
  await loadURL(DOC);
  is(
    await getNavigatorProperty("userAgent"),
    userAgent,
    "Custom user agent has been set"
  );
  is(
    await getNavigatorProperty("platform"),
    platform,
    "Custom platform has been set"
  );
});

async function getNavigatorProperty(prop) {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [prop], _prop => {
    return content.navigator[_prop];
  });
}
