# webaudio-benchmark

## Run

Just open `index.html`. Time are in milliseconds, lower is better.

## Adding new benchmarks

Look into `benchmarks.js`, it's pretty straightforward.

## Grafana dashboard

When adding new benchmarks, run `node generate-grafana-dashboard.js`, this
should overwrite `webaudio.json` so that the new benchmarks are registered in
grafana.


## License

MPL 2.0
