# Nimbus Integration and Migration Guide for Desktop Front-End Experiments

This guide will help you integrate `ExperimentAPI.jsm` in your Desktop front-end code to run Nimbus experiments, while still being able to use preferences for default values and local development.

In order to use `ExperimentAPI.jsm` your code must be able to import `jsm`s in the parent process or a privileged child process.

## Register a new feature

A feature is just some area of code instrumented for experiments – it can be as small as a single function or as complex as a whole about: page. You need to choose an identifier for your feature (e.g. "aboutnewtab").

```javascript
// In ExperimentAPI.jsm

const MANIFEST = {
  // Our feature name
  aboutwelcome: {
    description: "The about:welcome page",
    // Control if the feature is on or off
    enabledFallbackPref: "browser.aboutwelcome.enabled",
    variables: {
      // Additional (optional) values that we can control
      // The name of these variables is up to you
      skipFocus: {
        type: "boolean",
        fallbackPref: "browser.aboutwelcome.skipFocus",
      },
    },
  },
};

// In firefox.js
pref("browser.aboutwelcome.enable", true);
pref("skipFocus", false);
```

> By setting fallback preferences for Nimbus features, you will be able to still run Normandy roll-outs and experiments while you are partially migrated. We do not recommend running Nimbus and Normandy experiments on the same feature/preference simultaneously.

## How to use an Experiment Feature

Import `ExperimentFeature` from `ExperimentAPI.jsm` and instantiate an instance

```jsx
XPCOMUtils.defineLazyGetter(this, "feature", () => {
  const { ExperimentFeature } = ChromeUtils.import(
    "resource://nimbus/ExperimentAPI.jsm"
  );
  // Here we use the same name we defined in the MANIFEST
  return new ExperimentFeature("aboutwelcome");
});
```

Access feature values:

```jsx
if (feature.isEnabled() {
  // Do something!
  // This is controllbed by the `enabledFallbackPref` defined in the MANIFEST
}

// props: { skipFocus: boolean }
const props = feature.getValue();
renderSomeUI(props);
```

Defaults values inline:

```jsx
feature.isEnabled({ defaultValue: true });

const { skipFocus } = feature.getValue() || {};
```

Listen to changes:

```jsx
// Listen to changes, including to fallback prefs.
feature.on(() => {
  updateUI(feature.getValue());
});
```
