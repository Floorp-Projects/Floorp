# Adding new metrics

All metrics that your project collects must be defined in a `metrics.yaml`
file. This file should be at the root of the application or library module (the same
directory as the `build.gradle` file you updated). The format of that file is
documented [here](https://mozilla.github.io/glean_parser/metrics-yaml.html).

When adding a new metric, the workflow is:
* Decide on which metric type you want to use.
* Add a new entry to metrics.yaml.
* Add code to your project to record into the metric by calling Glean.

**Important**: as stated [here](../../README.md#before-using-the-library), any new data collection requires
documentation and data-review. This is also required for any new metric automatically collected
by glean.

# Choosing a metric type

There are different metrics to choose from, depending on what you want to achieve:

* [Events](#Events): These allow recording of e.g. individual occurences of user actions, say every time a view was open and from where.
* [Counters](#Counters): Used to count how often something happens, say how often a certain button was pressed.
* [Labeled metrics](#labeled-metrics): Used to record multiple metrics of the same type under different string labels, say every time you want to get a count of different error types in one metric.

## Events

Events allow recording of e.g. individual occurences of user actions, say every time a view was open and from where.
Each time you record an event, it records a timestamp, the events name and a set of custom values.

Say you're adding a new event for when a view is shown. First you need to add an entry for the event to the `metrics.yaml` file:

```YAML
views:
  login_opened:
    type: event
    description: >
      Recorded when the login view is opened.
    ...
    extra_keys:
      source_of_login: 
        description: The source from which the login view was opened, e.g. "toolbar".
```

Now you can use the event from the applications code. Note that an `enum` has
been generated for handling the `extra_keys`: it has the same name as the event
metric, with `Keys` added. The enumeration values will be converted from
`snake_case` to `camelCase` for use in Kotlin.

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Views

// ...
Views.loginOpened.record(mapOf(Views.loginOpenedKeys.sourceOfLogin to "toolbar"))
```

There are test APIs available too, for example:
```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Views

// Was any event recorded?
assertTrue(Views.loginOpened.testHasValue())
// Get a List of the recorded events.
val snapshot = Views.loginOpened.testGetValue()
// Check that two events were recorded.
assertEquals(2, snapshot.size)
val first = snapshot.single()
assertEquals("login_opened", first.name)
```

NOTE: Using the testing API requires calling `Glean.enableTestingMode()` first,
such as from a `@Before` method.

## Counters

Used to count how often something happens, say how often a certain button was pressed.
A counter always starts from `0`. Each time you record to a counter, its value is incremented.

Say you're adding a new counter for how often the refresh button is pressed. First you need to add an entry for the counter to the `metrics.yaml` file:

```YAML
controls:
  refresh_pressed:
    type: counter
    description: >
      Counts how often the refresh button is pressed.
    ...
```

Now you can use the event from the applications code:
```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Controls

// ...
Controls.refreshPressed.add() // Adds 1 to the counter.
Controls.refreshPressed.add(5) // Adds 5 to the counter.
```

There are test APIs available too:

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Controls

// Was anything recorded?
assertTrue(Controls.refreshPressed.testHasValue())
// Does the counter have the expected value?
assertEquals(6, Controls.refreshPressed.testGetValue())
```

NOTE: Using the testing API requires calling `Glean.enableTestingMode()` first,
such as from a `@Before` method.

## Labeled metrics

Some metrics can be used as *labeled* variants. This means that for a single metric entry you define in `metrics.yaml`, you can record into multiple metrics under the same name, each identified by a different string label.

This is useful when you need to break down metrics by a label known at build time or run time. For example:
- When you want to count a different set of subviews that users interact with, you could use `viewCount["view1"].add()` and `viewCount["view2"].add()`.
- When you want to count errors that might occur for a feature, you could use `errorCount[errorName].add()`.

**Note**: Be careful with using arbitrary strings as labels and make sure they can't accidentally contain identifying data (like directory paths or user input).

Say you're adding a new counter for errors that can occur when loading a resource from a REST API. First you need to add an entry for the counter to the `metrics.yaml` file:

```YAML
updater:
  load_error:
    type: counter
    labeled: true # This makes it a labeled counter.
    labels: # This is optional, if provided it limits the set of labels you can use.
    - timeout
    - not_found
    description: >
      Counts the different types of load errors that can occur.
    ...
```

Now you can use the labeled counter from the applications code:
```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Updater

// ...
Updater.loadError["timeout"].add() // Adds 1 to the "timeout" counter.
Updater.loadError["not_found"].add(2)
```

There are test APIs available too:

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Updater
Glean.enableTestingMode()

// Was anything recorded?
assertTrue(Updater.loadError["timeout"].testHasValue())
assertTrue(Updater.loadError["not_found"].testHasValue())
// Does the counter have the expected value?
assertEquals(2, Updater.loadError["not_found"].testGetValue())
```
