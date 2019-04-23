# Custom pings

Applications can define metrics that are sent in custom pings. Unlike the
built-in pings, custom pings are sent explicitly by the application.

## Defining a custom ping

Custom pings must be defined in a `pings.yaml` file, which is in the same
directory alongside your app's `metrics.yaml` file.

Each ping has the following parameters:

- `description` (required): A human-readable description of the ping.
- `include_client_id` (required): A boolean indicating whether to include the
  `client_id` in the [`client_info` section](pings.md#The-client_info-section)).

For example, to define a custom ping called `search` specifically for search
information:

```YAML
search:
  description: >
    A ping to record search data.
  include_client_id: false
```

## Sending metrics in a custom ping

To send a metric on a custom ping, you add the custom ping's name to
the `send_in_pings` parameter in the `metrics.yaml` file.

For example, to define a new metric to record the default search engine, which
is sent in a custom ping called `search`, put `search` in the `send_in_pings`
parameter.  Note that it is an error to specify a ping in `send_in_pings` that
does not also have an entry

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

To send a custom ping, call the `send` method on the `PingType` object that
glean generated for your ping.

For example, to send the custom ping defined above:

```kotlin
import org.mozilla.yourApplication.GleanMetrics.Pings
Pings.search.send()
```
