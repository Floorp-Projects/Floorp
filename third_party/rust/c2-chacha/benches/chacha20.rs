#![feature(test)]
extern crate c2_chacha;
extern crate stream_cipher;
extern crate test;

use c2_chacha::ChaCha20;
use stream_cipher::{NewStreamCipher, SyncStreamCipher};
use test::Bencher;

#[bench]
pub fn stream_10k(b: &mut Bencher) {
    let mut state = ChaCha20::new_var(&[0; 32], &[0; 8]).unwrap();
    let mut result = [0; 1024];
    b.iter(|| {
        for _ in 0..10 {
            state.apply_keystream(&mut result)
        }
    });
    b.bytes = 10240;
}
