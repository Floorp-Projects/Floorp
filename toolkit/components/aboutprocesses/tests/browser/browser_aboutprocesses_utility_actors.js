/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test about:processes preparation of utility actor names.
add_task(async function testUtilityActorNames() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      opening: "about:processes",
      waitForLoad: true,
    },
    browser => {
      const View = browser.contentWindow.View;
      const unknownActorName = "unknown";
      const kDontExistFluentName =
        View.utilityActorNameToFluentName("i-dont-exist");
      const unknownFluentName =
        View.utilityActorNameToFluentName(unknownActorName);

      Assert.equal(
        unknownFluentName,
        kDontExistFluentName,
        "Anything is unknown"
      );

      for (let actorName of ChromeUtils.getAllPossibleUtilityActorNames()) {
        const fluentName = View.utilityActorNameToFluentName(actorName);
        if (actorName === unknownActorName) {
          Assert.ok(
            fluentName === unknownFluentName,
            `Actor name ${actorName} is expected unknown ${fluentName}`
          );
        } else {
          Assert.ok(
            fluentName !== unknownFluentName,
            `Actor name ${actorName} is known ${fluentName}`
          );
        }
      }
    }
  );
});
