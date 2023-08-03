/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DOC = toDataURL(`<script>document.write(navigator.userAgent);</script>`);

add_task(async function invalidPlatform({ client }) {
  const { Emulation } = client;
  const userAgent = "Mozilla/5.0 (rv: 23) Romanesco/42.0\n";

  for (const platform of [null, true, 1, [], {}]) {
    await Assert.rejects(
      Emulation.setUserAgentOverride({ userAgent, platform }),
      /platform: string value expected/,
      `Fails with invalid type: ${platform}`
    );
  }
});

add_task(async function setAndResetUserAgent({ client }) {
  const { Emulation } = client;
  const userAgent = "Mozilla/5.0 (rv: 23) Romanesco/42.0";

  await loadURL(DOC);
  const originalUserAgent = await getNavigatorProperty("userAgent");

  isnot(originalUserAgent, userAgent, "Custom user agent hasn't been set");

  await Emulation.setUserAgentOverride({ userAgent });
  await loadURL(DOC);
  is(
    await getNavigatorProperty("userAgent"),
    userAgent,
    "Custom user agent has been set"
  );

  await Emulation.setUserAgentOverride({ userAgent: "" });
  await loadURL(DOC);
  is(
    await getNavigatorProperty("userAgent"),
    originalUserAgent,
    "Custom user agent has been reset"
  );
});

add_task(async function invalidUserAgent({ client }) {
  const { Emulation } = client;
  const userAgent = "Mozilla/5.0 (rv: 23) Romanesco/42.0\n";

  await loadURL(DOC);
  isnot(
    await getNavigatorProperty("userAgent"),
    userAgent,
    "Custom user agent hasn't been set"
  );

  await Assert.rejects(
    Emulation.setUserAgentOverride({ userAgent }),
    err => err.message.includes("Invalid characters found in userAgent"),
    "Invalid user agent format raised error"
  );
});

add_task(async function setAndResetPlatform({ client }) {
  const { Emulation } = client;
  const userAgent = "Mozilla/5.0 (rv: 23) Romanesco/42.0";
  const platform = "foobar";

  await loadURL(DOC);
  const originalUserAgent = await getNavigatorProperty("userAgent");
  const originalPlatform = await getNavigatorProperty("platform");

  isnot(userAgent, originalUserAgent, "Custom user agent hasn't been set");
  isnot(platform, originalPlatform, "Custom platform hasn't been set");

  await Emulation.setUserAgentOverride({ userAgent, platform });
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

  await Emulation.setUserAgentOverride({ userAgent: "", platform: "" });
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
});

add_task(async function notSetForNewContext({ client }) {
  const { Emulation, Target } = client;
  const userAgent = "Mozilla/5.0 (rv: 23) Romanesco/42.0";
  const platform = "foobar";

  await Emulation.setUserAgentOverride({ userAgent, platform });
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

  await openTab(Target, { activate: true });

  isnot(
    await getNavigatorProperty("userAgent"),
    userAgent,
    "Custom user agent has not been set"
  );
  isnot(
    await getNavigatorProperty("platform"),
    platform,
    "Custom platform has not been set"
  );
});

async function getNavigatorProperty(prop) {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [prop], _prop => {
    return content.navigator[_prop];
  });
}
