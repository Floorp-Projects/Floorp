// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use criterion::{criterion_group, criterion_main, Criterion}; // black_box
use neqo_transport::send_stream::RangeTracker;

const CHUNK: u64 = 1000;
const END: u64 = 100_000;
fn build_coalesce(len: u64) -> RangeTracker {
    let mut used = RangeTracker::default();
    let chunk = usize::try_from(CHUNK).expect("should fit");
    used.mark_acked(0, chunk);
    used.mark_sent(CHUNK, usize::try_from(END).expect("should fit"));
    // leave a gap or it will coalesce here
    for i in 2..=len {
        // These do not get immediately coalesced when marking since they're not at the end or start
        used.mark_acked(i * CHUNK, chunk);
    }
    used
}

fn coalesce(c: &mut Criterion, count: u64) {
    let chunk = usize::try_from(CHUNK).expect("should fit");
    c.bench_function(
        &format!("coalesce_acked_from_zero {count}+1 entries"),
        |b| {
            b.iter_batched_ref(
                || build_coalesce(count),
                |used| {
                    used.mark_acked(CHUNK, chunk);
                    let tail = (count + 1) * CHUNK;
                    used.mark_sent(tail, chunk);
                    used.mark_acked(tail, chunk);
                },
                criterion::BatchSize::SmallInput,
            );
        },
    );
}

fn benchmark_coalesce(c: &mut Criterion) {
    coalesce(c, 1);
    coalesce(c, 3);
    coalesce(c, 10);
    coalesce(c, 1000);
}

criterion_group!(benches, benchmark_coalesce);
criterion_main!(benches);
