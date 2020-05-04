/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { updateAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);
const { RecipeRunner } = ChromeUtils.import(
  "resource://normandy/lib/RecipeRunner.jsm"
);

// Test that new build IDs trigger immediate recipe runs
add_task(async () => {
  updateAppInfo({
    appBuildID: "new-build-id",
    lastAppBuildID: "old-build-id",
  });
  const runStub = sinon.stub(RecipeRunner, "run");
  const registerTimerStub = sinon.stub(RecipeRunner, "registerTimer");
  sinon.stub(RecipeRunner, "watchPrefs");

  Services.prefs.setBoolPref("app.normandy.first_run", false);

  await RecipeRunner.init();
  Assert.deepEqual(
    runStub.args,
    [[{ trigger: "newBuildID" }]],
    "RecipeRunner.run is called immediately on a new build ID"
  );
  ok(registerTimerStub.called, "RecipeRunner.registerTimer registers a timer");

  sinon.restore();
});
