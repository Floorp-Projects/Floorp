// Any copyright to the test code below is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

use criterion::black_box;
use criterion::criterion_group;
use criterion::criterion_main;
use criterion::BenchmarkId;
use criterion::Criterion;

use mapped_hyph::Hyphenator;
use std::fs;

const SAMPLE_SIZE: usize = 300;
const DIC_PATH: &str = "hyph_en_US.hyf";

fn bench_construct(c: &mut Criterion) {
    c.bench_function("construct", |b| {
        b.iter(|| {
            let dic = unsafe { mapped_hyph::load_file(DIC_PATH) }
                .expect(&format!("failed to load dictionary {}", DIC_PATH));
            let _ = Hyphenator::new(black_box(&*dic));
        })
    });
}

fn bench_find_hyphen_values(c: &mut Criterion) {
    // XXX: Should we copy this file to the crate to ensure reproducability?
    let data = fs::read_to_string("/usr/share/dict/words").expect("File reading failed.");
    let words: Vec<&str> = data.lines().take(SAMPLE_SIZE).collect();

    let dic = unsafe { mapped_hyph::load_file(DIC_PATH) }
        .expect(&format!("failed to load dictionary {}", DIC_PATH));
    let hyph = Hyphenator::new(&*dic);

    c.bench_with_input(
        BenchmarkId::new("bench_word", SAMPLE_SIZE),
        &words,
        |b, words| {
            b.iter(|| {
                let mut values: Vec<u8> = vec![0; 1000];
                for w in words {
                    hyph.find_hyphen_values(&w, &mut values);
                }
            });
        },
    );
}

criterion_group!(benches, bench_construct, bench_find_hyphen_values,);
criterion_main!(benches);
