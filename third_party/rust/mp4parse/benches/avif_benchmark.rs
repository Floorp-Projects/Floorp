// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use criterion::{criterion_group, criterion_main, Criterion};
use std::fs::File;

fn criterion_benchmark(c: &mut Criterion) {
    c.bench_function("avif_largest", |b| b.iter(avif_largest));
}

criterion_group!(benches, criterion_benchmark);
criterion_main!(benches);

fn avif_largest() {
    let input = &mut File::open(
        "av1-avif/testFiles/Netflix/avif/cosmos_frame05000_yuv444_12bpc_bt2020_pq_qlossless.avif",
    )
    .expect("Unknown file");
    assert!(mp4parse::read_avif(input, mp4parse::ParseStrictness::Normal).is_ok());
}
