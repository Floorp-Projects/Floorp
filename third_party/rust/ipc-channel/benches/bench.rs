#![feature(test)]

extern crate crossbeam;
extern crate ipc_channel;
extern crate test;

use ipc_channel::platform;

use std::sync::{mpsc, Mutex};

/// Allows doing multiple inner iterations per bench.iter() run.
///
/// This is mostly to amortise the overhead of spawning a thread in the benchmark
/// when sending larger messages (that might be fragmented).
///
/// Note that you need to compensate the displayed results
/// for the proportionally longer runs yourself,
/// as the benchmark framework doesn't know about the inner iterations...
const ITERATIONS: usize = 1;

#[bench]
fn create_channel(b: &mut test::Bencher) {
    b.iter(|| {
        for _ in 0..ITERATIONS {
            platform::channel().unwrap();
        }
    });
}

fn bench_size(b: &mut test::Bencher, size: usize) {
    let data: Vec<u8> = (0..size).map(|i| (i % 251) as u8).collect();
    let (tx, rx) = platform::channel().unwrap();

    let (wait_tx, wait_rx) = mpsc::channel();
    let wait_rx = Mutex::new(wait_rx);

    if size > platform::OsIpcSender::get_max_fragment_size() {
        b.iter(|| {
            crossbeam::scope(|scope| {
                scope.spawn(|| {
                    let wait_rx = wait_rx.lock().unwrap();
                    for _ in 0..ITERATIONS {
                        tx.send(&data, vec![], vec![]).unwrap();
                        if ITERATIONS > 1 {
                            // Prevent beginning of the next send
                            // from overlapping with receive of last fragment,
                            // as otherwise results of runs with a large tail fragment
                            // are significantly skewed.
                            wait_rx.recv().unwrap();
                        }
                    }
                });
                for _ in 0..ITERATIONS {
                    rx.recv().unwrap();
                    if ITERATIONS > 1 {
                        wait_tx.send(()).unwrap();
                    }
                }
                // For reasons mysterious to me,
                // not returning a value *from every branch*
                // adds some 100 ns or so of overhead to all results --
                // which is quite significant for very short tests...
                0
            })
        });
    } else {
        b.iter(|| {
            for _ in 0..ITERATIONS {
                tx.send(&data, vec![], vec![]).unwrap();
                rx.recv().unwrap();
            }
            0
        });
    }
}

#[bench]
fn size_00_1(b: &mut test::Bencher) {
    bench_size(b, 1);
}
#[bench]
fn size_01_2(b: &mut test::Bencher) {
    bench_size(b, 2);
}
#[bench]
fn size_02_4(b: &mut test::Bencher) {
    bench_size(b, 4);
}
#[bench]
fn size_03_8(b: &mut test::Bencher) {
    bench_size(b, 8);
}
#[bench]
fn size_04_16(b: &mut test::Bencher) {
    bench_size(b, 16);
}
#[bench]
fn size_05_32(b: &mut test::Bencher) {
    bench_size(b, 32);
}
#[bench]
fn size_06_64(b: &mut test::Bencher) {
    bench_size(b, 64);
}
#[bench]
fn size_07_128(b: &mut test::Bencher) {
    bench_size(b, 128);
}
#[bench]
fn size_08_256(b: &mut test::Bencher) {
    bench_size(b, 256);
}
#[bench]
fn size_09_512(b: &mut test::Bencher) {
    bench_size(b, 512);
}
#[bench]
fn size_10_1k(b: &mut test::Bencher) {
    bench_size(b, 1 * 1024);
}
#[bench]
fn size_11_2k(b: &mut test::Bencher) {
    bench_size(b, 2 * 1024);
}
#[bench]
fn size_12_4k(b: &mut test::Bencher) {
    bench_size(b, 4 * 1024);
}
#[bench]
fn size_13_8k(b: &mut test::Bencher) {
    bench_size(b, 8 * 1024);
}
#[bench]
fn size_14_16k(b: &mut test::Bencher) {
    bench_size(b, 16 * 1024);
}
#[bench]
fn size_15_32k(b: &mut test::Bencher) {
    bench_size(b, 32 * 1024);
}
#[bench]
fn size_16_64k(b: &mut test::Bencher) {
    bench_size(b, 64 * 1024);
}
#[bench]
fn size_17_128k(b: &mut test::Bencher) {
    bench_size(b, 128 * 1024);
}
#[bench]
fn size_18_256k(b: &mut test::Bencher) {
    bench_size(b, 256 * 1024);
}
#[bench]
fn size_19_512k(b: &mut test::Bencher) {
    bench_size(b, 512 * 1024);
}
#[bench]
fn size_20_1m(b: &mut test::Bencher) {
    bench_size(b, 1 * 1024 * 1024);
}
#[bench]
fn size_21_2m(b: &mut test::Bencher) {
    bench_size(b, 2 * 1024 * 1024);
}
#[bench]
fn size_22_4m(b: &mut test::Bencher) {
    bench_size(b, 4 * 1024 * 1024);
}
#[bench]
fn size_23_8m(b: &mut test::Bencher) {
    bench_size(b, 8 * 1024 * 1024);
}
