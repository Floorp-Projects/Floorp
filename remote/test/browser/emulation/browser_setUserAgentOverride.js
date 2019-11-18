/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DOC = toDataURL(`<script>document.write(navigator.userAgent);</script>`);

add_task(async function setAndResetUserAgent({ Emulation }) {
  const userAgent = "foo bar";

  await loadURL(DOC);
  isnot(
    await getNavigatorProperty("userAgent"),
    userAgent,
    "Custom user agent hasn't been set"
  );

  try {
    await Emulation.setUserAgentOverride({ userAgent });
    await loadURL(DOC);
    is(
      await getNavigatorProperty("userAgent"),
      userAgent,
      "Custom user agent has been set"
    );

    await Emulation.setUserAgentOverride({ userAgent: "" });
    await loadURL(DOC);
    isnot(
      await getNavigatorProperty("userAgent"),
      userAgent,
      "Custom user agent hasn't been set anymore"
    );
  } finally {
    Services.prefs.clearUserPref("general.useragent.override");
  }
});

add_task(async function invalidUserAgent({ Emulation }) {
  const userAgent = "foobar\n";

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

async function getNavigatorProperty(prop) {
  return ContentTask.spawn(gBrowser.selectedBrowser, prop, _prop => {
    return content.navigator[_prop];
  });
}
