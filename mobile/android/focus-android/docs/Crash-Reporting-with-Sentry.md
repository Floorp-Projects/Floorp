# Crash Reporting with Sentry

> **If there is anything in this document that is not clear, is incorrect, or that requires more detail, please file a request through a [Github](https://github.com/mozilla-mobile/focus-android/issues).**

Focus Android uses [Sentry](https://sentry.io) for crash and exception reporting. This kind of reporting gives Mozilla invaluable insight as to why Focus crashes or incorrectly behaves. It is one of the key methods we use to improve the product in terms of stability.

This page explains how Sentry works, how the various parts interact and what kind of data it sends back to Mozilla.

## High-Level Summary
[Sentry](https://sentry.io) is an open source crash reporting and aggregation platform. Both the client SDK, [github.com/getsentry/sentry-java](https://github.com/getsentry/sentry-java), and the server, [github.com/getsentry/sentry](https://github.com/getsentry/sentry), are open source.

The server is hosted and maintained by Mozilla. There are no third-parties involved, all crash reports to directly from Focus Android to the Sentry server hosted by Mozilla.

On the client side Sentry is invisible. There are no parts to interact with. It reports crashes and fatal errors back to Mozilla in the background. Sentry is enabled when the *Send Usage Data* switch in the *Focus* settings is enabled by the user. By default this switch is enabled in Focus and is an *opt-out* mechanism. In Klar, by default this switch is disabled and is an *opt-in* mechanism.

On the server side there is a dashboard that the Focus team uses to look at incoming crash reports. The dashboard lets us inspect the crash report in detail and for example see where in the application the crash happened, what version of the application was used and what version of Android OS was active. Below is an overview of all the attributes that are part of a crash report.

## Sentry Reports

A typical Sentry crash report contains three categories of data: device, application, crash. It also contains some metadata about the crash report:
```
 "id": "6ae18611d6c649529a5eda0e48f42cb4",
// ...
 "datetime": "2018-03-30T23:55:03.000000Z",
// ...
 "received": 1522454183.0,
```

To clarify, `id` is a unique identifier for this crash report, *not a unique identifier for the user sending the report.* We explicitly disable the ability to uniquely identify users from their crash reports.

### Device Information

Sentry collects basic information about the device the application is running on. Both static (device type) and dynamic (memory in use, device orientation).

```
"contexts": {
    "device": {
      "screen_resolution": "1920x1080",
      "battery_level": 100.0,
      "orientation": "landscape",
      "family": "AFTN",
      "model_id": "NS6212",
      "type": "device",
      "low_memory": false,
      "simulator": false,
      "free_storage": 3967590400,
      "storage_size": 5735825408,
      "screen_dpi": 320,
      "free_memory": 543588352,
      "memory_size": 1392164864,
      "online": true,
      "charging": true,
      "model": "AFTN",
      "screen_density": 2.0,
      "arch": "armeabi-v7a",
      "brand": "Amazon",
      "manufacturer": "Amazon"
    },
// ...
    "os": {
      "rooted": false,
      "kernel_version": "Linux version 3.14.29 (build@14-use1b-b-42) (gcc version 4.9.2 20140904 (prerelease) (crosstool-NG linaro-1.13.1-4.9-2014.09 - Linaro GCC 4.9-2014.09) ) #1 SMP PREEMPT Fri Jan 19 00:36:45 UTC 2018",
      "version": "7.1.2",
      "build": "NS6212",
      "type": "os",
      "name": "Android"
    }
},
```

### Application Information

Sentry collects basic information about the Focus app.

```
    "app": {
      "app_identifier": "org.mozilla.focus",
      "app_name": "Focus",
      "app_start_time": "2018-03-30T16:55:03Z",
      "app_version": "2.1",
      "type": "app",
      "app_build": 11
// ...
  "sdk": {
    "client_ip": "63.245.222.193",
    "version": "1.7.2-02be9",
    "name": "sentry-java"
```

### Crash Information

#### Stack trace

Every crash report contains a *stack trace*, which shows what functions in the Focus code led to this crash. It includes names of Android framework functions and Focus functions. Here's an excerpt of three lines from the stack trace:

```
  "sentry.interfaces.Exception": {
    "exc_omitted": null,
    "values": [
      {
        "stacktrace": {
          "frames": [
            {
              "function": "main",
              "abs_path": "ZygoteInit.java",
              "module": "com.android.internal.os.ZygoteInit",
              "in_app": false,
              "lineno": 801,
              "filename": "ZygoteInit.java"
            },
            {
              "function": "run",
              "abs_path": "ZygoteInit.java",
              "module": "com.android.internal.os.ZygoteInit$MethodAndArgsCaller",
              "in_app": false,
              "lineno": 911,
              "filename": "ZygoteInit.java"
            },
            {
              "function": "invoke",
              "abs_path": "Method.java",
              "in_app": false,
              "module": "java.lang.reflect.Method",
              "filename": "Method.java"
},
```

#### Exception message
The first line of every stack trace in every crash report contains a *reason* - why did this crash happen. This reason is provided by the developers who wrote the code that decide the app is in an error state. These developers include the Focus team at Mozilla, the Android framework, the Java programming language, and any libraries Mozilla bundles to develop Focus.

Java, the Android framework, and Mozilla are diligent about making sure that no personally identifiable information is put in any of these messages. We keep them technical and to the point. We at Mozilla stay on top of our dependencies to ensure they're not including personally identifiable information as well.

Here's an example message generated by Java:
```
java.lang.StringIndexOutOfBoundsException: length=0; regionStart=20; regionLength=20
```

Example of a Focus generated message:
```
java.lang.StringIndexOutOfBoundsException: Cannot create negative-length String
```

#### Raw data dump
In the explanations above, some redundant fields and field considered less important were omitted for brevity. To review these omissions, [this is an example of the raw data the server receives](https://gist.github.com/mcomella/50622aef817b40a20714b8550fb19991). This is up-to-date as of March 30, 2018.

## For developers
For developer documentation such as how to enable Sentry in your builds, see [`SentryWrapper.kt`](https://github.com/mozilla-mobile/focus-android/blob/master/app/src/main/java/org/mozilla/focus/telemetry/SentryWrapper.kt) in the code base.
