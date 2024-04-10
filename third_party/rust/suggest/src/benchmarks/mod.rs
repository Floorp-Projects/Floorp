/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Benchmarking support
//!
//! Benchmarks are split up into two parts: the functions to be benchmarked live here, which the benchmarking code itself lives in `benches/bench.rs`.
//! It's easier to write benchmarking code inside the main crate, where we have access to private items.
//! However, it's easier to integrate with Cargo and criterion if benchmarks live in a separate crate.
//!
//! All benchmarks are defined as structs that implement either the [Benchmark] or [BenchmarkWithInput]

pub mod client;
pub mod ingest;

/// Trait for simple benchmarks
///
/// This supports simple benchmarks that don't require any input.  Note: global setup can be done
/// in the `new()` method for the struct.
pub trait Benchmark {
    /// Perform the operations that we're benchmarking.
    fn benchmarked_code(&self);
}

/// Trait for benchmarks that require input
///
/// This will run using Criterion's `iter_batched` function.  Criterion will create a batch of
/// inputs, then pass each one to benchmark.
///
/// This supports simple benchmarks that don't require any input.  Note: global setup can be done
/// in the `new()` method for the struct.
pub trait BenchmarkWithInput {
    type Input;

    /// Generate the input (this is not included in the benchmark time)
    fn generate_input(&self) -> Self::Input;

    /// Perform the operations that we're benchmarking.
    fn benchmarked_code(&self, input: Self::Input);
}
