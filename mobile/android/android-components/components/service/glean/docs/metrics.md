# The `metrics` ping
The `metrics` ping contains all of the metrics defined in `metrics.yaml` that don't specify a ping or 
where `default` is specified in their [`send in pings`](https://mozilla.github.io/glean_parser/metrics-yaml.html#send-in-pings) property.  
The ping shall also include the common ping data found in all pings.

The `metrics` ping is automatically sent when the application is moved to the *background* provided
that the scheduled time interval has elapsed, which is currently 24 hours.  This limits `metrics`
pings to be sent not more than once per day. The schedule is only checked when the application
enters the background state so pings may be sent after an interval longer than 24 hours but never
shorter than 24 hours.
