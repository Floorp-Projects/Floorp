# Nimbus Testing Helpers

In order to make testing easier we created some helpers that can be accessed by including

```js
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
```

## Testing your feature integrating with Nimbus

1. You need to create a recipe

```js
let recipe = ExperimentFakes.recipe("my-cool-experiment", {
  branches: [
    {
      slug: "treatment-branch",
      ratio: 1,
      feature: {
        featureId: "<YOUR FEATURE>",
        // The feature is on
        enabled: true,
        // If you defined `variables` in the MANIFEST
        // the `value` should match that schema
        value: null,
      },
    },
  ],
  bucketConfig: {
    start: 0,
    // Ensure 100% enrollment
    count: 10000,
    total: 10000,
    namespace: "my-mochitest",
    randomizationUnit: "normandy_id",
  },
});
```

2. Now with the newly created recipe you want the test to enroll in the experiment

```js
let {
  enrollmentPromise,
  doExperimentCleanup,
} = ExperimentFakes.enrollmentHelper(recipe);

// Await for enrollment to complete

await enrollmentPromise;

// Now you can assume the feature is enabled so you can
// test and that it's doing the right thing

// Assert.ok(It works!)

// Finishing up
await doExperimentCleanup();
```
