// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::time::Duration;

use criterion::{criterion_group, criterion_main, BatchSize::SmallInput, Criterion, Throughput};
use test_fixture::{
    boxed,
    sim::{
        connection::{ConnectionNode, ReceiveData, SendData},
        network::{Delay, TailDrop},
        Simulator,
    },
};

const ZERO: Duration = Duration::from_millis(0);
const JITTER: Duration = Duration::from_millis(10);
const TRANSFER_AMOUNT: usize = 1 << 22; // 4Mbyte

fn benchmark_transfer(c: &mut Criterion, label: &str, seed: &Option<impl AsRef<str>>) {
    let mut group = c.benchmark_group("transfer");
    group.throughput(Throughput::Bytes(u64::try_from(TRANSFER_AMOUNT).unwrap()));
    group.noise_threshold(0.03);
    group.bench_function(label, |b| {
        b.iter_batched(
            || {
                let nodes = boxed![
                    ConnectionNode::default_client(boxed![SendData::new(TRANSFER_AMOUNT)]),
                    TailDrop::dsl_uplink(),
                    Delay::new(ZERO..JITTER),
                    ConnectionNode::default_server(boxed![ReceiveData::new(TRANSFER_AMOUNT)]),
                    TailDrop::dsl_downlink(),
                    Delay::new(ZERO..JITTER),
                ];
                let mut sim = Simulator::new(label, nodes);
                if let Some(seed) = &seed {
                    sim.seed_str(seed);
                }
                sim.setup()
            },
            |sim| {
                sim.run();
            },
            SmallInput,
        );
    });
    group.finish();
}

fn benchmark_transfer_variable(c: &mut Criterion) {
    benchmark_transfer(
        c,
        "Run multiple transfers with varying seeds",
        &std::env::var("SIMULATION_SEED").ok(),
    );
}

fn benchmark_transfer_fixed(c: &mut Criterion) {
    benchmark_transfer(
        c,
        "Run multiple transfers with the same seed",
        &Some("62df6933ba1f543cece01db8f27fb2025529b27f93df39e19f006e1db3b8c843"),
    );
}

criterion_group! {
    name = transfer;
    config = Criterion::default().warm_up_time(Duration::from_secs(5)).measurement_time(Duration::from_secs(15));
    targets = benchmark_transfer_variable, benchmark_transfer_fixed
}
criterion_main!(transfer);
