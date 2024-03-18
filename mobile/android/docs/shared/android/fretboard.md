# Fretboard
This getting started guide is designed to introduce you to adding A/B experiments to your Android apps using the Fretboard Android Component as we use it at Mozilla. This document summarizes and supplements the official Fretboard documentation.

- [What is Fretboard](#what-is-fretboard)
- [Integrating Fretboard Into A Project](#integrating-fretboard-into-a-project)
- [Setting Up An Experiment Source](#experiment-source)
- [Synchronizing Experiments from the Source](#synchronizing-experiments-from-the-source)
- [Experiment Filters](#experiment-filters)
- [Experiment Overrides](#experiment-overrides)
- [Experiment Schema for Kinto](#experiment-schema-for-kinto)
- [Fretboard README](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/README.md)
- [Fretboard Blog Announcement](https://mozilla-mobile.github.io/android-components/2018/08/10/fretboard.html)

## What is Fretboard
Fretboard is an open source A/B testing framework. It was designed to replace KeepSafe's Switchboard library and server, which was used in the older Firefox for Android, codenamed "Fennec".

## Integrating Fretboard Into a Project

### Add Fretboard to Build.Gradle
First add Fretboard to your build.gradle file:

`implementation "org.mozilla.components:fretboard:{latest-version}"`

You can [find the latest version here](https://github.com/mozilla-mobile/android-components/releases/latest).

### Create a Fretboard Instance
Learn how to set up your Fretboard instance in the [Fretboard documentation](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/README.md#creating-fretboard-instance).

## Setting Up An Experiment Source
You will need to choose an experiment source. Fretboard currently supports two choices for experiment sources. It can either read from a [Kinto](https://github.com/Kinto/kinto) server or from a JSON file on local disk. Using Kinto has the benefit of offering buckets, so you can split your users into an A and a B population.

## Synchronizing Experiments from the Source
There are two calls used for synchronizing experiments. One, Fretboard.loadExperiments() which reads current experiment values from a file on disk and a second call, Fretboard.updateExperiments() that goes out to the ExperimentSource. Both calls should not be made from Android main/UI thread. Any one of the multithreading methods on Android should be used to perform the work asynchronously from a background thread: Kotlin Coroutines, RxJava, AsyncTasks, Executors, etc.

You may want to call loadExperiments() before calling updateExperiments() so archived values from past runs are loaded quickly from disk before waiting for experiment values to be updated from the network.

You can learn about these calls from the [Fretboard documentation](https://github.com/mozilla-mobile/android-components/tree/master/components/service/fretboard#fetching-experiments-from-disk).

## Experiment Filters
In addition to separating users into buckets, you may decide to limit your experiment to a user population with specific characteristics, such as a particular app version, app ID, country, language, or device manufacturer. This is done using experiment filters, which is described in the [Fretboard documentation](https://github.com/mozilla-mobile/android-components/tree/master/components/service/fretboard#filters)

You may even create your own filters by defining a custom ValuesProvider.

## Experiment Overrides
Fretboard Overrides offer a means of locally changing which experiments are enabled for the current device. You can set an override and the server configuration will no longer choose which experiments are set. These values are stored locally in Android shared properties, so they will be wiped when a user clears app data.

There are methods to both set and clear overrides using either blocking or non-blocking calls. If you clear an override, then that device will resume receiving experiments from the ExperimentSource. Using a blocking call will ensure that the override is set before the next statement gets executed, but it is forbidden and will cause an error to use blocking calls from Android's main/UI thread.

Learn more about overrides in the [Fretboard documentation](https://github.com/mozilla-mobile/android-components/tree/master/components/service/fretboard#setting-override-values).

## Experiment Schema For Kinto
Below is an example JSON schema for Kinto, which can be used to provide the default filters for Fretboard experiments. You will have to change this slightly if you define your own filters or use more buckets than 0 through 100.

```json
  {
   "type": "object",
   "required": [
     "name",
     "match",
     "buckets"
   ],
   "properties": {
     "name": {
       "type": "string",
       "title": "Name"
     },
     "match": {
       "type": "object",
       "title": "Matching",
       "properties": {
         "lang": {
           "type": "string",
           "description": "Language, pulled from the default locale (e.g. eng)"
         },
         "appId": {
           "type": "string",
           "title": "Android App ID",
           "description": "^org.mozilla.fennec|org.mozilla.firefox_beta|org.mozilla.firefox$"
         },
         "device": {
           "type": "string",
           "description": "Android device name"
         },
         "country": {
           "type": "string",
           "description": "Country, pulled from the default locale (e.g. USA)"
         },
         "regions": {
           "type": "array",
           "items": {
             "type": "string",
             "title": "Regions",
             "default": "",
             "minLength": 0,
             "description": "Similar to a GeoIP lookup"
           },
           "title": "Regions",
           "default": [],
           "description": "Compared with GeoIP lookup.",
           "uniqueItems": true
         },
         "version": {
           "type": "string",
           "title": "Android App Version",
           "description": "A regexp on the Android app version number (e.g. 47.0a1, 46.0)"
         },
         "userAgent": {
           "type": "string",
           "description": "Browser User-Agent regexp. i.e: Firefox/46.0"
         },
         "manufacturer": {
           "type": "string",
           "description": "Android device manufacturer"
         }
       }
     },
     "buckets": {
       "type": "object",
       "title": "Buckets",
       "required": [
         "min",
         "max"
       ],
       "properties": {
         "max": {
           "type": "string",
           "pattern": "^0|100|[1-9][0-9]?$",
           "minLength": 1
         },
         "min": {
           "type": "string",
           "pattern": "^0|100|[1-9][0-9]?$",
           "minLength": 1
         }
       }
     },
     "description": {
       "type": "string",
       "title": "Description"
     }
   }
 }
 ```
