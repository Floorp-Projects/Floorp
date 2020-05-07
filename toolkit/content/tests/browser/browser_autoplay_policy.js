/**
 * This test is used to ensure we can get the correct document's autoplay policy
 * under different situations.
 * Spec discussion : https://github.com/WICG/autoplay/issues/1
 */
const PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/file_empty.html";

function setupTestPreferences(isAllowedAutoplay, isAllowedMuted) {
  let autoplayDefault = SpecialPowers.Ci.nsIAutoplay.ALLOWED;
  if (!isAllowedAutoplay) {
    autoplayDefault = isAllowedMuted
      ? SpecialPowers.Ci.nsIAutoplay.BLOCKED
      : SpecialPowers.Ci.nsIAutoplay.BLOCKED_ALL;
  }
  return SpecialPowers.pushPrefEnv({
    set: [
      ["dom.media.autoplay.autoplay-policy-api", true],
      ["media.autoplay.default", autoplayDefault],
      ["media.autoplay.blocking_policy", 0],
    ],
  });
}

async function checkAutoplayPolicy(expectedAutoplayPolicy) {
  is(
    content.document.autoplayPolicy,
    expectedAutoplayPolicy,
    `Autoplay policy is correct.`
  );
}

add_task(async function testAutoplayPolicy() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE,
    },
    async browser => {
      info(`- Allow all kinds of media to autoplay -`);
      let isAllowedAutoplay = true;
      let isAllowedMuted = true;
      await setupTestPreferences(isAllowedAutoplay, isAllowedMuted);
      await SpecialPowers.spawn(browser, ["allowed"], checkAutoplayPolicy);

      info(
        `- Allow all kinds of media to autoplay even if changing the pref for muted media -`
      );
      isAllowedAutoplay = true;
      isAllowedMuted = false;
      await setupTestPreferences(isAllowedAutoplay, isAllowedMuted);
      await SpecialPowers.spawn(browser, ["allowed"], checkAutoplayPolicy);

      info(`- Disable autoplay for audible media -`);
      isAllowedAutoplay = false;
      isAllowedMuted = true;
      await setupTestPreferences(isAllowedAutoplay, isAllowedMuted);
      await SpecialPowers.spawn(
        browser,
        ["allowed-muted"],
        checkAutoplayPolicy
      );

      info(`- Disable autoplay for all kinds of media -`);
      isAllowedAutoplay = false;
      isAllowedMuted = false;
      await setupTestPreferences(isAllowedAutoplay, isAllowedMuted);
      await SpecialPowers.spawn(browser, ["disallowed"], checkAutoplayPolicy);

      info(
        `- Simulate user gesture activation which would allow all kinds of media to autoplay -`
      );
      await SpecialPowers.spawn(browser, [], async () => {
        content.document.notifyUserGestureActivation();
      });
      await SpecialPowers.spawn(browser, ["allowed"], checkAutoplayPolicy);
    }
  );
});
