# [Android Components](../../../README.md) > Service > Experiments

An Android SDK for running experiments on user segments in multiple branches.

Contents:
- [Usage](#usage)
- [Testing experiments](#testing-experiments)
- [Experiments format for Kinto](#experiments-format-for-kinto)

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:service-experiments:{latest-version}"
```

### Initializing the Experiments library

In order to use the library, first you have to initialize it by calling `Experiments.initialize()`. 
You do this once per app launch (typically in your `Application` class `onCreate` method). You 
simply have to call `Experiments.initialize()` and provide the `applicationContext` (and optionally 
a `Configuration` object), like this:

```Kotlin
class SampleApp : Application() {
    override fun onCreate() {
        Experiments.initialize(
            applicationContext,
            configuration // This is optional, e.g. for overriding the fetch client.
        )
    }
}
```

This library makes use of [Glean](https://mozilla.github.io/glean/book/index.html) for reporting 
experiment enrollment. If Glean is not used and initialized by the application, the recording 
methods are a no-op.

### Updating of experiments

The library updates its list of experiments automatically and asynchronously from Kinto on library 
initialization. As this is asynchronous, it will not have immediate effect.

Afterwards, the list of experiments will be updated in the background every 6 hours.

### Checking if a user is part of an experiment

In order to check if a user is part of a specific experiment, `Experiments` provides a 
Kotlin-friendly `withExperiment` API. You pass the id of the experiment you want to check and if the 
client is in the experiment, you get the selected branch name passed:

```Kotlin
Experiments.withExperiment("button-color-experiment") { branchName ->
    when(branchName) {
      "red" -> button.setBackgroundColor(Color.RED)
      "control" -> button.setBackgroundColor(DEFAULT_COLOR)
    }
}
```

## Testing experiments

### Testing on the dev server

For any technical tests, we do have a Kinto dev server available, which can be freely configured & gets reset daily.
The admin interface is [here](https://kinto.dev.mozaws.net/v1/admin/). For setting up a testing setup we can:
- [Create a collection in the main bucket](https://kinto.dev.mozaws.net/v1/admin/#/buckets/main/collections/create).
  - The *collection id* should be `mobile-experiments`.
  - The *JSON schema* should have [this content](KintoSchema.md#JSON-Schema)
  - The *UI schema* should have [this content](KintoSchema.md#UI-Schema)
  - The *Records list columns* should contain `id` and `description`.
  - Click *Create collection*
- In the [`mobile-experiments` record list](https://kinto.dev.mozaws.net/v1/admin/#/buckets/main/collections/mobile-experiments/records), create new entries for experiments as needed.
- In the mobile application, use the debug commands below to switch to the `dev` endpoint.

### ExperimentsDebugActivity usage

Experiments exports the [`ExperimentsDebugActivity`](src/main/java/mozilla/components/service/experiments/debug/ExperimentsDebugActivity.kt)
that can be used to trigger functionality or toggle debug features on or off. Users can invoke this special activity, at
run-time, using the following [`adb`](https://developer.android.com/studio/command-line/adb) command:

`adb shell am start -n [applicationId]/mozilla.components.service.experiments.debug.ExperimentsDebugActivity [extra keys]`

In the above:

- `[applicationId]` is the product's application id as defined in the manifest
  file and/or build script. For the Glean sample application, this is
  `org.mozilla.samples.glean` for a release build and
  `org.mozilla.samples.glean.debug` for a debug build.

- `[extra keys]` is a list of extra keys to be passed to the debug activity. See the
  [documentation](https://developer.android.com/studio/command-line/adb#IntentSpec)
  for the command line switches used to pass the extra keys. These are the
  currently supported keys:

    |key|type|description|
    |---|----|-----------|
    | updateExperiments | boolean (--ez) | Forces the experiments updater to run and fetch experiments immediately. You must pass `true` or `false` following this switch but either will update experiments if this is present |
    | setKintoInstance | string (--es) | Sets the Kinto instance to the specified instance ("dev", "staging", "prod" only) |
    | overrideExperiment | string (--es) | Used to pass in the experiment to override and must be followed by the `branch` command below. |
    | branch | string (--es) | Used to set the branch for overriding the experiment and is meant to be used with the `overrideExperiment` command above. |
    | clearAllOverrides | boolean (--ez) | Clears any overrides that have been set.  You must pass `true` or `false` following this switch but either will clear the overrides if this is present. Should **not** be used in combination with the `overrideExperiment` command |

#### Examples
 
To direct a release build of the Glean sample application to (1) update experiments immediately and (2) change the Kinto instance to the staging instance, the following command can be used:

```
adb shell am start -n org.mozilla.samples.glean.debug/mozilla.components.service.experiments.debug.ExperimentsDebugActivity \
  --ez updateExperiments true \
  --es setKintoInstance staging
```

To direct a release build of the Glean sample application to override the experiment and branch, the following command can be used:

```
adb shell am start -n org.mozilla.samples.glean.debug/mozilla.components.service.experiments.debug.ExperimentsDebugActivity --es overrideExperiment testExperiment --es branch testBranch
```

## Experiments format for Kinto

The library loads its list of experiments from Kinto. Kinto provides data in the following JSON format:
```javascript
{
    // The whole list of experiments lives in a Kinto "collection".
    "data": [
        // Each experiments data is described in a "record".
        {
            "id":"some-experiment",
            // ... 
            "last_modified":1523549895713
        },
        // ... more records.
    ]
}
```

An individual experiment record looks e.g. like this:

```javascript
{
  "id": "button-color-expirement",
  "description": "The button color experiments tests ... per bug XYZ.",
  // Enroll 50% of the population in the experiment.
  "buckets": {
    "start": 0,
    "count": 500
  },
  // Distribute enrolled clients among two branches.
  "branches": [
    {
      "name": "control",
      "ratio": 1
    },
    {
      "name": "red-button-color",
      "ratio": 2 // Two thirds of the enrolled clients get red buttons.
    }
  ],
  "match": {
    "app_id": "^org.mozilla.firefox${'$'}",
    "locale_language": "eng|zho|deu"
    // Possibly more matchers...
  }
}
```

### Experiment fields

The experiments records in Kinto contain the following properties:

| Name                      | Type   | Required | Description                                                                                                                                     | Example                                                        |
|---------------------------|--------|----------|-------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------|
| id                        | string |     x    | The unique id of the experiment.                                                                                                                | `"login-button-color-test"`                                    |
| description               | string |     x    | A detailed description of the experiment.                                                                                                       |                                                                |
| last_modified             | number |     x    | Timestamp of when this experiment was last modified. This is provided by Kinto.                                                                 |                                                                |
| schema                    | number |     x    | Timestamp of when the schema was last modified. This is provided by Kinto.                                                                      |                                                                |
| buckets                   | object |     x    | Object containing a bucket range to match users. Every user is in one of 1000 buckets (0-999).                                                  |                                                                |
|  buckets.start            | number |     x    | The minimum bucket to match.                                                                                                                    |                                                                |
| buckets.count             | number |     x    | The number of buckets to match from start. If (start + count >= 999), evaluation will wrap around.                                              |                                                                |
| branches                  | object |     x    | Object containing the parameters for ratios for randomized enrollment into different branches.                                                  |                                                                |
| branches[i].name          | string |     x    | The name of that branch.                                                                                                                        | `"control"` or `"red-button"`                                  |
| branches[i].ratio         | number |     x    | The weight to randomly distribute enrolled clients among the branches.                                                                          |                                                                |
| match                     | object |     x    | Object containing the filter parameters to match specific user groups.                                                                          |                                                                |
| match.app_id              | regex  |          | The app ID (package name)                                                                                                                       | "^org.mozilla.firefox_beta${'$'}|^org.mozilla.firefox${'$'}"   |
| match.device_manufacturer | regex  |          | The Android device manufacturer.                                                                                                                |                                                                |
| match.device_model        | regex  |          | The Android device model.                                                                                                                       |                                                                |
| match.locale_country      |        |          | The default locales country.                                                                                                                    | "USA|DEU"                                                      |
| match.locale_language     |        |          | The default locales language.                                                                                                                   | "eng|zho|deu"                                                  |
| match.app_display_version | regex  |          | The application version.                                                                                                                        | `"1.0.0"`                                                      |
| match.app_min_version     | string |          | The minimum application version, inclusive.                                                                                                     | `"1.0.1"`                                                      |
| match.app_max_version     | string |          | The maximum application version, inclusive.                                                                                                     | `"1.4.2"`                                                      |
| match.regions             | array  |          | Array of strings. Not currently supported. Custom regions, different from the one from the default locale (like a GeoIP, or something similar). | `["USA", "GBR"]`                                               |
| match.debug_tags          | array  |          | Array of strings. Debug tags to match only specific client for QA of experiments launch & targeting.                                            | `["john-test-1"]`                                              |

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
