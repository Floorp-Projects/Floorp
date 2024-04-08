# Suggest benchmarking code

Use `cargo suggest-bench` to run these benchmarks.

The main benchmarking code lives here, while the criterion integration code lives in the `benches/`
directory.

## Benchmarks

### ingest-[provider-type]

Time it takes to ingest all suggestions for a provider type on an empty database.
The bechmark downloads network resources in advance in order to exclude the network request time
from these measurements.

### Benchmarks it would be nice to have

- Ingestion with synthetic data.  This would isolate the benchmark from changes to the RS database.
- Fetching suggestions

## cargo suggest-debug-ingestion-sizes

Run this to get row counts for all database tables.  This can be very useful for improving
benchmarks, since targeting the tables with the largest number of rows will usually lead to the
largest improvements.

The command also prints out the size of all remote-settings attachments, which can be good to
optimize on its own since it represents the amount of data user's need to download.
