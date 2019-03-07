# The `metrics` ping

## Description
The `metrics` ping is intended for all of the metrics that are explicitly set by the application or
are included in the application's `metrics.yaml` file.

## Scheduling
The `metrics` ping is automatically sent when the application enters the [background](pings.md#defining-background-state)

It is also required that the throttling time interval has elapsed, which is currently 24 hours.  
This limits `metrics` pings to be sent not more than once per day. The schedule is only checked 
when the application enters the background state so pings may be sent after an interval longer 
than 24 hours but never shorter than 24 hours.

## Contents
The `metrics` ping contains all of the metrics defined in `metrics.yaml` that don't specify a ping or 
where `default` is specified in their [`send in pings`](https://mozilla.github.io/glean_parser/metrics-yaml.html#send-in-pings) property.

Additionally, error metrics in the `glean.error` category are included in the `metrics` ping.

The `metrics` ping shall also include the common [`ping_info`](pings.md#the-ping_info-section) section found in all pings.

### Querying ping contents
A quick note about querying ping contents (i.e. for https://sql.telemetry.mozilla.org):  Each metric
in the metrics ping is organized by its metric type, and uses a namespace of 'glean.metrics'. For 
instance, in order to select a String field called `test` you would use `metrics.string['glean.metrics.test']`. 
