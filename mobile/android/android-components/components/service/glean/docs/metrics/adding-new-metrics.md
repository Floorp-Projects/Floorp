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
      source: The source from which the login view was opened, e.g. "toolbar".
```

Now you can use the event from the applications code:
```Kotlin
import org.mozilla.samples.glean.GleanMetrics.Views

// ...
Views.loginOpened.record(mapOf("source" to "toolbar"))
```
