/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that if a page captured in the background attempts to set a cookie,
 * that cookie is not saved for subsequent requests.
 */
add_task(async function thumbnails_bg_no_cookies_stored() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.usercontext.about_newtab_segregation.enabled", true]],
  });
  let url = bgTestPageURL({
    setRedCookie: true,
    iframe: bgTestPageURL({ setRedCookie: true }),
    xhr: bgTestPageURL({ setRedCookie: true }),
  });
  ok(!thumbnailExists(url), "Thumbnail file should not exist before capture.");
  await bgCapture(url);
  ok(thumbnailExists(url), "Thumbnail file should exist after capture.");
  removeThumbnail(url);
  // now load it up in a browser - it should *not* be red, otherwise the
  // cookie above was saved.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async browser => {
      // The root element of the page shouldn't be red.
      await SpecialPowers.spawn(browser, [], function() {
        Assert.notEqual(
          content.document.documentElement.style.backgroundColor,
          "rgb(255, 0, 0)",
          "The page shouldn't be red."
        );
      });
    }
  );
});
