# [Android Components](../../../README.md) > Service > Fretboard

An Android framework for segmenting users in order to run A/B tests and rollout features gradually.

## Usage

### Setting up the dependency

Use gradle to download the library from JCenter:

```Groovy
implementation "org.mozilla.components:fretboard:{latest-version}
```

### Creating Fretboard instance
In order to use the library, first you have to create a new `Fretboard` instance. You do this once per app launch 
(typically in your `Application` class `onCreate` method). You simply have to instantiate the `Fretboard` class and
provide the `ExperimentStorage` and `ExperimentSource` implementations, like this:

```Kotlin
class SampleApp : Application() {
    lateinit var fretboard: Fretboard

    override fun onCreate() {
        fretboard = Fretboard(
            experimentSource,
            experimentStorage
        )
    }
}
```

#### Using Kinto as experiment source
Fretboard includes a default source implementation for a Kinto backend, which you can use like this:

```Kotlin
val fretboard = Fretboard(
    KintoExperimentSource(baseUrl, bucketName, collectionName),
    experimentStorage
)
```

#### Using a JSON file as experiment storage
Fretboard includes support for flat JSON files as storage mechanism out of the box:

```Kotlin
val fretboard = Fretboard(
    experimentSource,
    FlatFileExperimentStorage(File(context.filesDir, "experiments.json"))
)
```

### Fetching experiments from disk
After instantiating `Fretboard`, in order to load the list of already downloaded
experiments from disk, you need to call the `loadExperiments` method (don't call it
on the UI thread, this example uses a coroutine):

```Kotlin
launch(CommonPool) {
    fretboard.loadExperiments()
}
```

### Updating experiment list
Fretboard provides two ways of updating the downloaded experiment list from the server: the first one is to directly
call `updateExperiments` on a `Fretboard` instance, which forces experiments to be updated immediately and synchronously
(do not call this on the main thread), like this:

```Kotlin
fretboard.updateExperiments()
```

The second one is to use the provided `JobScheduler`-based scheduler, like this:
```Kotlin
val scheduler = JobSchedulerSyncScheduler(context)
scheduler.schedule(EXPERIMENTS_JOB_ID, ComponentName(this, ExperimentsSyncService::class.java))
```

Where `ExperimentsSyncService` is a subclass of `SyncJob` you create like this, providing the `Fretboard` instance via the
`getFretboard` method:

```Kotlin
class ExperimentsSyncService : SyncJob() {
    override fun getFretboard(): Fretboard {
        return fretboard
    }
}
```

And then you have to register it on the manifest, just like any other `JobService`:

```xml
<service android:name=".ExperimentsSyncService"
         android:exported="false"
         android:permission="android.permission.BIND_JOB_SERVICE">
```

### Checking if a user is part of an experiment
In order to check if a user is part of a specific experiment, Fretboard provides two APIs: a Kotlin-friendly
`withExperiment` API and a more Java-like `isInExperiment`. In both cases you pass an instance of `ExperimentDescriptor`
with the id of the experiment you want to check:

```Kotlin
val descriptor = ExperimentDescriptor("first-experiment-id")
fretboard.withExperiment(descriptor) {
    someButton.setBackgroundColor(Color.RED)
}

otherButton.isEnabled = fretboard.isInExperiment(descriptor)
```

### Getting experiment metadata
Fretboard allows experiments to carry associated metadata, which can be retrieved using the kotlin-friendly 
`withExperiment` API or the more Java-like `getExperiment` API, like this:

```Kotlin
val descriptor = ExperimentDescriptor("first-experiment-id")
fretboard.withExperiment(descriptor) {
    toolbar.setColor(Color.parseColor(it.payload?.get("color") as String))
}
textView.setText(fretboard.getExperiment(descriptor)?.payload?.get("message"))
```

### Setting override values
Fretboard allows you to force activate / deactivate a specific experiment via `setOverride`, you
simply have to pass true to activate it, false to deactivate:

```Kotlin
val descriptor = ExperimentDescriptor("first-experiment-id")
fretboard.setOverride(context, descriptor, true)
```

You can also clear an override for an experiment or all overrides:

```Kotlin
val descriptor = ExperimentDescriptor("first-experiment-id")
fretboard.clearOverride(context, descriptor)
fretboard.clearAllOverrides(context)
```

### Filters
Fretboard allows you to specify the following filters:
- Buckets: Every user is in one of 100 buckets (0-99). For every experiment you can set up a min and max value (0 <= min <= max <= 100). The bounds are [min, max).
- appId (regex): The app ID (package name)
- version (regex): The app version
- country (regex): country, pulled from the default locale
- lang (regex): language, pulled from the default locale
- device (regex): Android device name
- manufacturer (regex): Android device manufacturer
- region: custom region, different from the one from the default locale (like a GeoIP, or something similar).
For this to work you must provide a RegionProvider implementation when creating the `Fretboard` instance.
- release channel: release channel of the app (alpha, beta, etc)

### Specifying custom values for filters
Additionally, Fretboard allows you to specify a custom `ValuesProvider` object in order to return a custom region,
different from the one of the current locale (perhaps doing a GeoIP or something like that), or the app
relase channel (alpha, beta, etc). It also allows you to override the values for other experiment properties
(such as the appId, country, etc):

```Kotlin
val fretboard = Fretboard(
    experimentSource,
    experimentStorage,
    object : ValuesProvider {
        override fun getRegion() {
            return custom_region
        }

        override fun getReleaseChannel() {
            return app_channel
        }
    }
)
```

### Creating a custom experiment source
You can create a custom experiment source simply by implementing the `ExperimentSource` interface:

```Kotlin
class MyExperimentSource : ExperimentSource {
    override fun getExperiments(snapshot: ExperimentsSnapshot): ExperimentsSnapshot {
        // ...
        return updatedSnapshot
    }
}
```

The `getExperiments` method takes an `ExperimentsSnapshot` object, which contains the list of already downloaded experiments and 
a last_modified date, and returns another `ExperimentsSnapshot` object with the updated list of experiments.

As the `getExperiments` receives the list of experiments from storage and a last_modified date, it allows you
to do diff requests, if your storage mechanism supports it (like Kinto does).

### Creating a custom experiment storage
You can create a custom experiment storage simply by implementing the `ExperimentStorage` interface, overriding
the save and retrieve methods, which use `ExperimentsSnapshot` objects with the list of experiments and a last_modified date:

```Kotlin
class MyExperimentStorage : ExperimentStorage {
    override fun save(snapshot: ExperimentsSnapshot) {
        // save snapshot to disk
    }

    override fun retrieve(): ExperimentsSnapshot {
        // load snapshot from disk
        return snapshot
    }
}
```

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
