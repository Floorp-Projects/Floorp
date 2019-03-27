# Adding new metrics

All metrics that your project collects must be defined in a `metrics.yaml`
file. This file should be at the root of the application or library module (the same
directory as the `build.gradle` file you updated). The format of that file is
documented [here](https://mozilla.github.io/glean_parser/metrics-yaml.html).

When adding a new metric, the workflow is:
* Decide on which [metric type](#Choosing-a-metric-type) you want to use.
* Add a new entry to [`metrics.yaml`](#The-metrics-yaml-file).
* Add code to your project to record into the metric by calling glean.

**Important**: as stated [here](../../README.md#before-using-the-library), any new data collection requires
documentation and data-review. This is also required for any new metric automatically collected
by glean.

# The `metrics.yaml` file

The `metrics.yaml` file defines the metrics your application or library will
send. They are organized into categories.

Therefore, the overall organization is:

```YAML
# Required to indicate this is a `metrics.yaml` file
$schema: moz://mozilla.org/schemas/glean/metrics/1-0-0

toolbar:
  click:
    type: event
    description: |
      Event to record toolbar clicks.
    notification_emails:
      - CHANGE-ME@test-only.com
    bugs:
      - 123456789
    data_reviews:
      - http://test-only.com/path/to/data-review
    expires:
      - 2019-06-01  # <-- Update to a date in the future
    
  metric2:
    ...
    
category2.subcategory:  # Categories can contain subcategories
  metric3:
    ...

```

The details of the metric parameters are described below in [Common metric
parameters](#Common-metric-parameters).

The `metrics.yaml` file is used to generate `Kotlin` code that becomes the
public API to access your application's metrics.

## Metric naming

Category and metric names in the `metrics.yaml` are in `snake_case`, but given
the Kotlin coding standards defined by
[ktlint](https://github.com/pinterest/ktlint), these identifiers must be
`camelCase` in Kotlin. For example, the metric defined in the `metrics.yaml` as:


```YAML
views:
  login_opened:
    ...
```

is accessible in Kotlin as:

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Views
Views.loginOpened...
```

# Choosing a metric type

There are different metrics to choose from, depending on what you want to achieve:

* [Events](#Events): Records events e.g. individual occurences of user actions, say every time a view was open and from where.
* [Booleans](#Booleans): Records a single truth value, for example "is a11y enabled?"
* [Strings](#Strings): Records a single Unicode string value, for example the name of the OS.
* [String Lists](#String-lists): Records a list of Unicode string values, for example the list of enabled search engines.
* [Counters](#Counters): Used to count how often something happens, for example, how often a certain button was pressed.
* [Timespans](#Timespans): Used to measure how much time is spent in a single task.
* [Timing Distributions](#Timing-Distributions): Used to record the distribution of multiple time measurements.
* [Datetimes](#Datetimes): Used to record an absolute date and time, such as the time the user first ran the application.
* [UUIDs](#UUIDs): Used to record universally unique identifiers (UUIDs), such as a client ID.
* [Labeled Metrics](#Labeled-metrics): Used to record multiple metrics of the same type under different string labels, say every time you want to get a count of different error types in one metric.

## Events

Events allow recording of e.g. individual occurences of user actions, say every time a view was open and from where.
Each time you record an event, it records a timestamp, the event's name and a set of custom values.

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

Now you can use the event from the application's code. Note that an `enum` has
been generated for handling the `extra_keys`: it has the same name as the event
metric, with `Keys` added.

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Views

Views.loginOpened.record(mapOf(Views.loginOpenedKeys.sourceOfLogin to "toolbar"))
```

There are test APIs available too, for example:
```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Views
Glean.enableTestingMode()

// Was any event recorded?
assertTrue(Views.loginOpened.testHasValue())
// Get a List of the recorded events.
val snapshot = Views.loginOpened.testGetValue()
// Check that two events were recorded.
assertEquals(2, snapshot.size)
val first = snapshot.single()
assertEquals("login_opened", first.name)
```

Event timestamps use a system timer that is guaranteed to be monotonic only
within a particular boot of the device. Therefore, if there are any unsent
recorded events on disk when the application starts, any pings containing those
events are sent immediately, so that glean can start over using a new timer and
events based on different timers are never sent within the same ping.

## Booleans

Booleans are used for simple flags, for example "is a11y enabled"?.

Say you're adding a boolean to record whether a11y is enabled on the device. 
First you need to add an entry for the boolean to the `metrics.yaml` file:

```YAML
flags:
  a11y_enabled:
    type: boolean
    description: >
      Records whether a11y is enabled on the device.
    ...
```

Now you can use the boolean from the application's code:
```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Flags

Flags.a11yEnabled.set(System.isAccesibilityEnabled())
```

There are test APIs available too:

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Flags
Glean.enableTestingMode()

// Was anything recorded?
assertTrue(Flags.a11yEnabled.testHasValue())
// Does it have the expected value?
assertTrue(Flags.a11yEnabled.testGetValue())
```

## Strings

Used to record a single string value, say the name of the default search engine in a browser.

Say you're adding a metric to find out what the default search in a browser is. First you need to add an entry for the metric to the `metrics.yaml` file:

```YAML
search.default:
  name:
    type: string
    description: >
      The name of the default search engine.
    ...
```

Now you can use the string from the applications code:
```Kotlin
import org.mozilla.yourApplication.GleanMetrics.SearchDefault

// Record a value into the metric.
SearchDefault.name.set("duck duck go")
// If it changed later, you can record the new value:
SearchDefault.name.set("wikipedia")
```

There are test APIs available too:

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.SearchDefault

Glean.enableTestingMode()
// Was anything recorded?
assertTrue(SearchDefault.name.testHasValue())
// Does the string metric have the expected value?
assertEquals("wikipedia", SearchDefault.name.testGetValue())
```

## String lists

Strings lists are used for recording a list of Unicode string values, such as
the names of the enabled search engines.

First, you need to add an entry for the string list to the `metrics.yaml` file:

```YAML
search:
  engines:
    type: string_list
    description: >
      Records the name of the enabled search engines.
    ...
```

Now you can use the string list from the application's code:
```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Search

// Add them one at a time
engines.forEach {
  Search.engines.add(it)
}

// Set them in one go
Search.engines.set(engines)
```

**Note**: Be careful using arbitrary strings and make sure they can't
accidentally contain identifying data (like directory paths or user input).

There are test APIs available too:

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Search
Glean.enableTestingMode()

// Was anything recorded?
assertTrue(Search.engines.testHasValue())
// Does it have the expected value?
assertEquals(listOf("Google", "DuckDuckGo"), Search.engines.testGetValue())
```

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

Now you can use the counter from the application's code:

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Controls

Controls.refreshPressed.add() // Adds 1 to the counter.
Controls.refreshPressed.add(5) // Adds 5 to the counter.
```

There are test APIs available too:

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Controls
Glean.enableTestingMode()

// Was anything recorded?
assertTrue(Controls.refreshPressed.testHasValue())
// Does the counter have the expected value?
assertEquals(6, Controls.refreshPressed.testGetValue())
```

## Timespans

Timespans are used to make a summed measurement of how much time is spent in a
particular task. To measure the distribution of multiple timespans, see [Timing
Distributions](#Timing-Distributions). To record absolute times, see
[Datetimes](#Datetimes).

### Metric-specific parameters

Timespans have a required `time_unit` parameter to specify the smallest unit
of resolution that the timespan will record. The allowed values for `time_unit` are:

   `nanosecond`, `microsecond`, `millisecond`, `second`, `minute`, `hour`, and
   `day`

Consider the resolution that is required by your metric, and use the largest
possible value that will provide useful information so as to not leak too much
fine-grained information from the client. It is important to note that the value
sent in the ping is truncated down to the nearest unit. Therefore, a measurement
of 500 nanoseconds will be truncated to 0 microseconds.

### Usage

Say you're adding a new timespan for the time spent decoding images. First you need
to add an entry for the counter to the `metrics.yaml` file:

```YAML
rendering:
  image_decode_time:
    type: timespan
    description: >
      Measures the time spent decoding images.
    time_unit: microseconds
    ...
```

Now you can use the timespan from the application's code:

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Rendering

Rendering.imageDecodeTime.start() // start the timer
try {
   // ... decode an image ...
} catch (e: Exception) {
   Rendering.imageDecodeTime.cancel() // cancel the timer -- nothing is recorded
}
Rendering.imageDecodeTime.stopAndSum() // stop the timer
```

The time reported in the telemetry ping will be the sum of all of these
timespans recorded during the lifetime of the ping.

There are test APIs available too:

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Rendering
Glean.enableTestingMode()

// Was anything recorded?
assertTrue(Rendering.imageDecodeTime.testHasValue())
// Does the timer have the expected value
assertTrue(Rendering.imageDecodeTime.testGetValue() > 0)
```

## Timing Distributions

Timing distributions are used to accumulate and store time measurement, for analyzing distributions of the timing data.

### Metric-specific parameters

Timing distributions have a required `time_unit` parameter to specify the resolution and range of the values that are recorded. The allowed values for `time_unit` are:

   `nanosecond`, `microsecond`, `millisecond`, `second`, `minute`, `hour`, and
   `day`

Which range of values is recorded in detail depends on the `time_unit`, e.g. for
milliseconds all values greater 60000 are recorded as overflow values.

### Usage

If you wanted to create a timing distribution to measure page load times, first you need to add an 
entry for it to the `metrics.yaml` file:

```YAML
pages:
  page_load:
    type: timing_distribution
    time_unit: millisecond
    description: >
      Counts how long each page takes to load
    ...
```


Now that the timing distribution is defined in `metrics.yaml` you can use the metric to record data
in the application code:

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Pages

// ...
pages.pageLoad.accumulate(1L) // Accumulates a sample of 1 millisecond
pages.pageLoad.accumulate(10L) // Accumulates a sample of 10 milliseconds
```

There are test APIs available too.  For convenience, properties `sum` and `count` are exposed to 
facilitate validating that data was recorded correctly.  Continuing the `pageLoad` example above,
at this point the metric should have a `sum == 11` and a `count == 2`:

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Pages
Glean.enableTestingMode()

// Was anything recorded?
assertTrue(pages.pageLoad.testHasValue())

// Get snapshot
val snapshot = pages.pageLoad.testGetValue()

// Does the sum have the expected value?
assertEquals(11, snapshot.sum)

// Usually you don't know the exact timing values, but how many should have been recorded.
assertEquals(2L, snapshot.count())
```

## Datetimes

Datetimes are used to record an absolute date and time, for example the date and time that the application was first run.

The device's offset from UTC is recorded and sent with the datetime value in the ping.

### Metric-specific properties

Datetimes have a required `time_unit` parameter to specify the smallest unit
of resolution that the timespan will record. The allowed values for `time_unit` are:

   `nanosecond`, `microsecond`, `millisecond`, `second`, `minute`, `hour`, and
   `day`

Carefully consider the required resolution for recording your metric, and choose
the coarsest resolution possible.

### Usage

You first need to add an entry for it to the `metrics.yaml` file:

```YAML
install:
  first_run:
    type: datetime 
    time_unit: day 
    description: >
      Records the date when the application was first run
    ...
```

Now that the datetime is defined in `metrics.yaml` you can use the metric to
record datetimes in the application's code:

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Install

Install.firstRun.set() // Records "now"
Install.firstRun.set(Calendar(2019, 3, 25)) // Records a custom datetime
```

There are test APIs available too.

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.Install
Glean.enableTestingMode()

// Was anything recorded?
assertTrue(Install.firstRun.testHasValue())
// Was it the expected value?
// NOTE: Datetimes always include a timezone offset from UTC, hence the 
// "-05:00" suffix.
assertEquals("2019-03-25-05:00", Install.firstRun.testGetValueAsString())
```

## UUIDs

UUIDs are used to record values that uniquely identify some entity, such as a client id.

You first need to add an entry for it to the `metrics.yaml` file:

```YAML
user:
  client_id:
    type: uuid
    description: >
      A unique identifier for the client's profile
    ...
```

Now that the UUID is defined in `metrics.yaml`, you can use the metric to record
values in the application's code.

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.User

User.clientId.generateAndSet() // Generate a new UUID and record it
User.clientId.set(UUID.randomUUID())  // Set a UUID explicitly
```

There are test APIs available too.

```Kotlin
import org.mozilla.yourApplication.GleanMetrics.User
Glean.enableTestingMode()

// Was anything recorded?
assertTrue(User.clientId.testHasValue())
// Was it the expected value? 
assertEquals(uuid, User.clientId.testGetValue())
```

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

# Common metric parameters

All metric types must include the following required parameters:

- `description`: **Required.** A textual description of the metric for humans.
  It should describe what the metric does, what it means for analysts, and its
  edge cases or any other helpful information.
  
- `notification_emails`: **Required.** A list of email addresses to notify for
  important events with the metric or when people with context or ownership for
  the metric need to be contacted.
  
- `bugs`: **Required.** A list of bugs (e.g. Bugzilla or Github) that are
  relevant to this metric. For example, bugs that track its original
  implementation or later changes to it. If a number, it is an ID to an issue in
  the default tracker (`bugzilla.mozilla.org`). If a string, it must be a URI to
  a bug page in a tracker.
  
- `data_reviews`: **Required.** A list of URIs to any data collection reviews
  relevant to the metric.
  
- `expires`: **Required.** May be one of the following values:
  - `<build date>`: An ISO date `yyyy-mm-dd` in UTC on which the metric expires.
    For example, `2019-03-13`. This date is checked at build time. Except in
    special cases, this form should be used so that the metric automatically
    "sunsets" after a period of time.
  - `never`: This metric never expires.
  - `expired`: This metric is manually expired.
  
All metric types also support the following optional parameters:

- `lifetime`: Defines the lifetime of the metric. Different lifetimes affect
  when the metrics value is reset.
  - `ping` (default): The metric is reset each time it is sent in a ping.
  - `user`: The metric is part of the user's profile.
  - `application`: The metric is related to an application run, and is reset
    when the application restarts.
    
- `send_in_pings`: Defines which pings the metric should be sent on. If not
  specified, the metric is sent on the "default ping", which is the `events`
  ping for events and the `metrics` ping for everything else. Most metrics don't
  need to specify this unless they are sent on custom pings.
  
- `disabled`: (default: `false`) Data collection for this metric is disabled.

- `version`: (default: 0) The version of the metric. A monotonically increasing
  integer value. This should be bumped if the metric changes in a
  backward-incompatible way.
  
For clarity, these common parameters are ommitted from the examples about
specific metric types below.


