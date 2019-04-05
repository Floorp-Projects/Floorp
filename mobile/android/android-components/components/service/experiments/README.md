# [Android Components](../../../README.md) > Service > Experiments

An Android SDK for running experiments on user segments in multiple branches.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:service-experiments:{latest-version}"
```

### Initializing the Experiments library
In order to use the library, first you have to initialize it by calling `Experiments.initialize()`. You do this once per app launch 
(typically in your `Application` class `onCreate` method). You simply have to call `Experiments.initialize()` and
provide the `applicationContext` (and optionally a `Configuration` object), like this:

```Kotlin
class SampleApp : Application() {
    override fun onCreate() {
        // Glean needs to be initialized first.
        Glean.initialize(/* ... */)
        Experiments.initialize(
            applicationContext,
            configuration // This is optional, e.g. for overriding the fetch client.
        )
    }
}
```

Note that this library depends on the Glean library, which has to be initialized first. See the [Glean README](../glean/README.md) for more details.

### Updating of experiments

The library updates it's list of experiments automatically and async from Kinto on startup. As this is asynchronous, it will not have immediate effect.

### Checking if a user is part of an experiment
In order to check if a user is part of a specific experiment, `Experiments` provides two APIs: a Kotlin-friendly
`withExperiment` API and a more Java-like `isInExperiment`. In both cases you pass the id of the experiment you want to check:

```Kotlin
Experiments.withExperiment("first-experiment-name") {
    someButton.setBackgroundColor(Color.RED)
}

otherButton.isEnabled = Experiments.isInExperiment("first-experiment-name")
```

### Filters
`Experiments` allows you to specify the following filters:
- Buckets: Every user is in one of 100 buckets (0-99). For every experiment you can set up a min and max value (0 <= min <= max <= 100). The bounds are [min, max).
    - Both max and min are optional. For example, specifying only min = 0 or only max = 100 includes all users
    - 0-100 includes all users (as opposed to 0-99)
    - 0-0 includes no users (as opposed to just bucket 0)
    - 0-1 includes just bucket 0
    - Users will always stay in the same bucket. An experiment targeting 0-25 will always target the same 25% of users
- appId (regex): The app ID (package name)
- version (regex): The app version
- country (regex): country, pulled from the default locale
- lang (regex): language, pulled from the default locale
- device (regex): Android device name
- manufacturer (regex): Android device manufacturer
- *(TBD: region: custom region, different from the one from the default locale (like a GeoIP, or something similar).)*
- *(TBD: release channel: release channel of the app (alpha, beta, etc))*

### Experiments format for Kinto
The provided implementation for Kinto expects the experiments in the following JSON format:
```json
{
    "data":[
        {
            "name": "",
            "match":{
                "lang":"",
                "appId":"",
                "regions":[],
                "country":"",
                "version":"",
                "device":"",
                "manufacturer":"",
                "region":"",
                "release_channel":""
            },
            "buckets": {
                "min": "0",
                "max": "100"
            },
            "description":"",
            "id":"",
            "last_modified":1523549895713
        }
    ]
}
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
