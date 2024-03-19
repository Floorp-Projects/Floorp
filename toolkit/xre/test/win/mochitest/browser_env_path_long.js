// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

// Test that the browser starts even when PATH would expand to a detrimentally
// long value.
add_task(async function test() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    function () {
      ok(
        true,
        "Browser should start even with potentially pathologically long PATH."
      );
    }
  );
});
