/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function moreSettingsHonorPref() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();

    ok(!helper.get("more-settings").open, "More settings is closed");

    await helper.openMoreSettings();
    ok(
      Services.prefs.getBoolPref("print.more-settings.open"),
      "More settings pref has been flipped to true"
    );

    await helper.closeDialog();

    await helper.startPrint();

    ok(helper.get("more-settings").open, "More settings is open");

    helper.click(helper.get("more-settings").firstElementChild);
    await helper.awaitAnimationFrame();
    ok(
      !Services.prefs.getBoolPref("print.more-settings.open"),
      "More settings pref has been flipped to false"
    );

    await helper.closeDialog();

    await helper.startPrint();

    ok(!helper.get("more-settings").open, "More settings is closed");

    await helper.closeDialog();
  });
});
