#[cfg(feature = "wasm-bindgen")]
use wasm_bindgen_test::*;

use getrandom::getrandom;

#[cfg(feature = "test-in-browser")]
wasm_bindgen_test_configure!(run_in_browser);

#[cfg_attr(feature = "wasm-bindgen", wasm_bindgen_test)]
#[test]
fn test_zero() {
    // Test that APIs are happy with zero-length requests
    getrandom(&mut [0u8; 0]).unwrap();
}

#[cfg_attr(feature = "wasm-bindgen", wasm_bindgen_test)]
#[test]
fn test_diff() {
    let mut v1 = [0u8; 1000];
    getrandom(&mut v1).unwrap();

    let mut v2 = [0u8; 1000];
    getrandom(&mut v2).unwrap();

    let mut n_diff_bits = 0;
    for i in 0..v1.len() {
        n_diff_bits += (v1[i] ^ v2[i]).count_ones();
    }

    // Check at least 1 bit per byte differs. p(failure) < 1e-1000 with random input.
    assert!(n_diff_bits >= v1.len() as u32);
}

#[cfg_attr(feature = "wasm-bindgen", wasm_bindgen_test)]
#[test]
fn test_huge() {
    let mut huge = [0u8; 100_000];
    getrandom(&mut huge).unwrap();
}

#[cfg(any(unix, windows, target_os = "redox", target_os = "fuchsia"))]
#[test]
fn test_multithreading() {
    use std::sync::mpsc::channel;
    use std::thread;

    let mut txs = vec![];
    for _ in 0..20 {
        let (tx, rx) = channel();
        txs.push(tx);

        thread::spawn(move || {
            // wait until all the tasks are ready to go.
            rx.recv().unwrap();
            let mut v = [0u8; 1000];

            for _ in 0..100 {
                getrandom(&mut v).unwrap();
                thread::yield_now();
            }
        });
    }

    // start all the tasks
    for tx in txs.iter() {
        tx.send(()).unwrap();
    }
}
