/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DOC = toDataURL(`<script>document.write(navigator.userAgent);</script>`);

add_task(async function setAndResetUserAgent({ Emulation }) {
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

add_task(async function invalidUserAgent({ Emulation }) {
  const userAgent = "Mozilla/5.0 (rv: 23) Romanesco/42.0\n";

  await loadURL(DOC);
  isnot(
    await getNavigatorProperty("userAgent"),
    userAgent,
    "Custom user agent hasn't been set"
  );

  let errorThrown = false;
  try {
    await Emulation.setUserAgentOverride({ userAgent });
  } catch (e) {
    errorThrown = true;
  }
  ok(errorThrown, "Invalid user agent format raised error");
});

add_task(async function notSetForNewContext({ Emulation, Target }) {
  const userAgent = "Mozilla/5.0 (rv: 23) Romanesco/42.0";

  await Emulation.setUserAgentOverride({ userAgent });
  await loadURL(DOC);
  is(
    await getNavigatorProperty("userAgent"),
    userAgent,
    "Custom user agent has been set"
  );

  const { targetInfo } = await openTab(Target);
  await Target.activateTarget({ targetId: targetInfo.targetId });

  isnot(
    await getNavigatorProperty("userAgent"),
    userAgent,
    "Custom user agent has been set"
  );
});

async function getNavigatorProperty(prop) {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [prop], _prop => {
    return content.navigator[_prop];
  });
}
