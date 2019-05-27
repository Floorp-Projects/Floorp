# [Android Components](../../../README.md) > Support > Base

Base or core component containing building blocks and interfaces for other components.

Usually this component never needs to be added to application projects manually. Other components may have a transitive dependency on some of the classes and interfaces in this component.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:support-base:{latest-version}"
```

## Logging

The base component offers helpers for logging for component (and app) code that allows the app to be in control what gets logged and how.

### Setup

A log messages are routed through the `Log` object which doesn't process any logs itself. Instead it forwards the calls to `LogSink` implementations. The base component includes the `AndroidLogSink` class which implements `LogSink` and forwards log messages to Android's system log.

```Kotlin
// Forward logs to Android's system log.
// Most apps want to do that at least for debug builds:
Log.addSink(AndroidLogSink())

// A default tag can be set. This tag will be used whenever no tag is provided when logging a message.
Log.addSink(AndroidLogSink("MyAwesomeApp"))

// Log a message (See "Logger" section for more convenient ways to log)
Log.log(tag = "Test", priority = Log.Priority.DEBUG, message = "Hello World!")

// Set the minimum log level. All log messages with a lower level will be ignored.
Log.logLevel = Log.Priority.WARN
```

An application can add multiple `LogSink` implementations to save logs to disk, send them to a crash reporting service or display them inside the app.

### Logger

The `Log` class only offers a low-level logging call. The `logger` sub package contains classes that wrap `Log` and provide a more convenient API for logging.

```Kotlin
class MyClass {
   // All log calls on this instance will use the tag MyClassLogger
   val logger = Logger("MyClassLogger")

   fun doSomething() {
     // Will log a DEBUG message with tag MyClassLogger
     logger.debug("Hello World")
   }
   
   fun couldThrow() {
     try {
       // ..
     } catch (e: IllegalStateException) {
       // Will log a ERROR message with stack trace and tag MyClassLogger
       logger.error("Oops!", e)
     )
   }
   
   fun generic() {
     // You can also use the Logger class directly if no custom tag is needed:
     Logger.info("Hello World!")
   }
}
```

## Notifications

Android's APIs require a unique `Int` id for showing and cancelling notifications. Agreeing on unique ids over multiple components and app code without any conflicts is hard. For this reason this component contains a `NotificationIds` object that allocates unique, stable ids based on a provided `String` tag.

```kotlin
// Get a unique id for the provided tag
val id = NotificationIds.getIdForTag(context, "mozac.my.feature")

// Extension methods for showing and cancelling notifications
NotificationManagerCompat
    .from(context)
    .notify(context, "mozac.my.feature", notification)

NotificationManagerCompat
    .from(context)
    .cancel(context, "mozac.my.feature")
```

## Facts

A `Fact` is a generic "event" that a component emitted.

Facts are not meant to implement application logic based on them. Instead they can be observed as a stream of "user/app events" inside components that can be analyzed or forwarded to external telemetry services.

By default nothing happens with `Fact` instances. An app needs to register a `FactProcessor` that will receive all emitted `Fact` objects.

The base component comes with a `LogFactProcessor` that will print all emitted `Fact` instances to a `Logger`.

```kotlin
// Either install processors on the Facts object:
Facts.registerProcessor(LogFactProcessor())

// Or use the extension method:
LogFactProcessor()
    .register()
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
