# Glean Parser

Parser tools for Mozilla's Glean telemetry.

## Features

Parses the `metrics.yaml` files for the Glean telemetry SDK and produces
output for various integrations.

## Documentation

The full documentation is available
[here](https://mozilla.github.io/glean_parser/).

## Requirements

-   Python 3.6 (or later)

The following library requirements are installed automatically when
`glean_parser` is installed by `pip`.

-   appdirs
-   Click
-   diskcache
-   Jinja2
-   jsonschema
-   PyYAML

Additionally on Python 3.6:

-   iso8601

## Usage

```sh
$ glean_parser --help
```

Read in `metrics.yaml`, translate to Kotlin format, and
output to `output_dir`:

```sh
$ glean_parser translate -o output_dir -f kotlin metrics.yaml
```

Check a Glean ping against the ping schema:

```sh
$ glean_parser check < ping.json
```
