#![feature(plugin, test)]
#![plugin(speculate)]

extern crate diff;
extern crate test;

speculate! {
    describe "slice" {
        bench "empty" |b| {
            let slice = [0u8; 0];
            b.iter(|| ::diff::slice(&slice, &slice));
        }

        bench "10 equal items" |b| {
            let slice = [0u8; 10];
            b.iter(|| ::diff::slice(&slice, &slice));
        }

        bench "10 non-equal items" |b| {
            let (left, right) = ([0u8; 10], [1u8; 10]);
            b.iter(|| ::diff::slice(&left, &right));
        }

        bench "100 equal items" |b| {
            let slice = [0u8; 100];
            b.iter(|| ::diff::slice(&slice, &slice));
        }

        bench "100 non-equal items" |b| {
            let (left, right) = ([0u8; 100], [1u8; 100]);
            b.iter(|| ::diff::slice(&left, &right));
        }

        bench "1000 equal items" |b| {
            let slice = [0u8; 1000];
            b.iter(|| ::diff::slice(&slice, &slice));
        }

        bench "1000 non-equal items" |b| {
            let (left, right) = ([0u8; 1000], [1u8; 1000]);
            b.iter(|| ::diff::slice(&left, &right));
        }
    }
}
