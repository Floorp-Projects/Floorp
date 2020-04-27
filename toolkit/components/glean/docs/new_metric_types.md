# Adding a New Metric Type

This document covers how to add a new metric type to FOG.
You should only have to do this if a new metric type is added to the
[Glean SDK](https://mozilla.github.io/glean/book/user/metrics/index.html)
and it is needed in Firefox Desktop.

## IPC

To Be Implemented.

## Rust

FOG is being used to test out the ergonomics of a public Rust API,
so has been granted special permission to implement directly atop `glean_core`.
For now.

The Rust API is implemented in the
[`metrics` module of the `api` crate](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/api/src/metrics).

Each metric gets its own file, mimicking the structure in
[`glean_core`](https://github.com/mozilla/glean/tree/master/glean-core/src/metrics).

Every method on the metric type is public for now,
including test methods.

## C++

To Be Implemented.

## JS

To Be Implemented.
