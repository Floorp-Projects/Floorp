/**
 * Bug 1663192 - Testing for ensuring the top-level window in a fire url is
 *               treated as first-party.
 */

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.cookie.cookieBehavior", 1],
      ["network.cookie.cookieBehavior.pbmode", 1],
    ],
  });
});

add_task(async function () {
  let dir = getChromeDir(getResolvedURI(gTestPath));
  dir.append("file_localStorage.html");
  const uriString = Services.io.newFileURI(dir).spec;
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, uriString);

  await SpecialPowers.spawn(tab.linkedBrowser, [], function () {
    let result = content.document.getElementById("result");

    is(
      result.textContent,
      "PASS",
      "The localStorage is accessible in top-level window"
    );

    let loadInfo = content.docShell.currentDocumentChannel.loadInfo;

    ok(
      !loadInfo.isThirdPartyContextToTopWindow,
      "The top-level window shouldn't be third-party"
    );
  });

  BrowserTestUtils.removeTab(tab);
});
