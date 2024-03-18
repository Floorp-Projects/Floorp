samples_glean_library is a toy library that uses glean to record metrics. It
exists simply to show how libraries that are neither the application or glean
itself can record metrics, and those metrics will be stored and sent as part of
the [main pings](../../../components/service/glean/docs/pings.md) that glean
provides.
