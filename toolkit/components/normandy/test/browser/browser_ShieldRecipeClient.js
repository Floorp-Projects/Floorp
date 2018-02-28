"use strict";

ChromeUtils.import("resource://normandy-content/AboutPages.jsm", this);
ChromeUtils.import("resource://normandy/lib/AddonStudies.jsm", this);
ChromeUtils.import("resource://normandy/lib/PreferenceExperiments.jsm", this);
ChromeUtils.import("resource://normandy/lib/RecipeRunner.jsm", this);
ChromeUtils.import("resource://normandy/lib/ShieldRecipeClient.jsm", this);
ChromeUtils.import("resource://normandy/lib/TelemetryEvents.jsm", this);

function withStubInits(testFunction) {
  return decorate(
    withStub(AboutPages, "init"),
    withStub(AddonStudies, "init"),
    withStub(PreferenceExperiments, "init"),
    withStub(RecipeRunner, "init"),
    withStub(TelemetryEvents, "init"),
    testFunction
  );
}

decorate_task(
  withStubInits,
  async function testStartup() {
    const initObserved = TestUtils.topicObserved("shield-init-complete");
    await ShieldRecipeClient.startup();
    ok(AboutPages.init.called, "startup calls AboutPages.init");
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
    await initObserved;
  }
);

decorate_task(
  withStubInits,
  async function testStartupPrefInitFail() {
    PreferenceExperiments.init.returns(Promise.reject(new Error("oh no")));

    await ShieldRecipeClient.startup();
    ok(AboutPages.init.called, "startup calls AboutPages.init");
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
    ok(TelemetryEvents.init.called, "startup calls TelemetryEvents.init");
  }
);

decorate_task(
  withStubInits,
  async function testStartupAboutPagesInitFail() {
    AboutPages.init.returns(Promise.reject(new Error("oh no")));

    await ShieldRecipeClient.startup();
    ok(AboutPages.init.called, "startup calls AboutPages.init");
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
    ok(TelemetryEvents.init.called, "startup calls TelemetryEvents.init");
  }
);

decorate_task(
  withStubInits,
  async function testStartupAddonStudiesInitFail() {
    AddonStudies.init.returns(Promise.reject(new Error("oh no")));

    await ShieldRecipeClient.startup();
    ok(AboutPages.init.called, "startup calls AboutPages.init");
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
    ok(TelemetryEvents.init.called, "startup calls TelemetryEvents.init");
  }
);

decorate_task(
  withStubInits,
  async function testStartupTelemetryEventsInitFail() {
    TelemetryEvents.init.throws();

    await ShieldRecipeClient.startup();
    ok(AboutPages.init.called, "startup calls AboutPages.init");
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
    ok(TelemetryEvents.init.called, "startup calls TelemetryEvents.init");
  }
);
