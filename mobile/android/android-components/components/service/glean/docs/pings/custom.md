# Custom pings

Applications can define metrics that are sent in custom pings. Unlike the
built-in pings, custom pings are sent explicitly by the application.

## Sending metrics in a custom ping

To send a metric on a custom ping, you must first add the custom ping's name to
the `send_in_pings` parameter in the `metrics.yaml` file.

For example, to define a new metric to record the default search engine, which
is sent in a custom ping called `search`:

```YAML
search.default:
  name:
    type: string
    description: >
      The name of the default search engine.
    send_in_pings:
      - search
```

If this metric should also be sent in the default ping for the given metric
type, you can add the special value `default` to `send_in_pings`:

```YAML
    send_in_pings:
      - search
      - default
```

## Sending a custom ping

To send a custom ping, use `Glean.sendPings`. It is up to the application to
call this method at the schedule and cadence that is appropriate for the ping.

```kotlin
Glean.sendPings(listOf("search"))
```
