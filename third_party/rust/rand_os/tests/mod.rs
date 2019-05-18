extern crate rand_os;

use rand_os::rand_core::RngCore;
use rand_os::OsRng;

#[test]
fn test_os_rng() {
    let mut r = OsRng::new().unwrap();

    r.next_u32();
    r.next_u64();

    let mut v1 = [0u8; 1000];
    r.fill_bytes(&mut v1);

    let mut v2 = [0u8; 1000];
    r.fill_bytes(&mut v2);

    let mut n_diff_bits = 0;
    for i in 0..v1.len() {
        n_diff_bits += (v1[i] ^ v2[i]).count_ones();
    }

    // Check at least 1 bit per byte differs. p(failure) < 1e-1000 with random input.
    assert!(n_diff_bits >= v1.len() as u32);
}

#[test]
fn test_os_rng_empty() {
    let mut r = OsRng::new().unwrap();

    let mut empty = [0u8; 0];
    r.fill_bytes(&mut empty);
}

#[test]
fn test_os_rng_huge() {
    let mut r = OsRng::new().unwrap();

    let mut huge = [0u8; 100_000];
    r.fill_bytes(&mut huge);
}

#[cfg(not(any(target_arch = "wasm32", target_arch = "asmjs")))]
#[test]
fn test_os_rng_tasks() {
    use std::sync::mpsc::channel;
    use std::thread;

    let mut txs = vec!();
    for _ in 0..20 {
        let (tx, rx) = channel();
        txs.push(tx);

        thread::spawn(move|| {
            // wait until all the tasks are ready to go.
            rx.recv().unwrap();

            // deschedule to attempt to interleave things as much
            // as possible (XXX: is this a good test?)
            let mut r = OsRng::new().unwrap();
            thread::yield_now();
            let mut v = [0u8; 1000];

            for _ in 0..100 {
                r.next_u32();
                thread::yield_now();
                r.next_u64();
                thread::yield_now();
                r.fill_bytes(&mut v);
                thread::yield_now();
            }
        });
    }

    // start all the tasks
    for tx in txs.iter() {
        tx.send(()).unwrap();
    }
}
